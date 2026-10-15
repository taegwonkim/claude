/**
  ******************************************************************************
  * @file    app_someip.c
  * @brief   SOME/IP 수신 + SOME/IP-SD 서비스 발견
  ******************************************************************************
  * - UDP 30490(SD) 과 추가 포트에서 SOME/IP 메시지를 받아 헤더를 해석하고 로그
  * - SD 의 Offer/StopOffer/Find/Subscribe 를 해석해서 "발견한 서비스 테이블" 유지
  *   (서비스 ID, 인스턴스, 버전, endpoint, TTL 만료 처리)
  * - SD 가 아닌 메시지는 (서비스, 메서드/이벤트) 별로 개수를 집계
  * - APP_SOMEIP_SEND_FIND = 1 일 때만 FindService 를 송신 (기본 꺼짐)
  ******************************************************************************
  */

#include "main.h"
#include "app_someip.h"
#include "someip.h"
#include "veh_log.h"
#include "veh_ecu.h"
#include "vehicle_net_conf.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#if LWIP_IGMP
#include "lwip/igmp.h"
#endif
#include <string.h>

#if APP_SOMEIP_ENABLE

extern struct netif gnetif;

#define RX_BUF_SIZE      1600U
#define MAX_METHODS      32U
#define EXTRA_PORT_MAX   4U

typedef struct
{
  uint8_t  used;
  uint16_t service_id;
  uint16_t instance_id;
  uint8_t  major;
  uint32_t minor;
  uint32_t ttl;
  uint8_t  has_ep;
  uint8_t  ep_ip[4];
  uint8_t  ep_proto;
  uint16_t ep_port;
  ip4_addr_t from;
  uint32_t last_seen;
  uint32_t offers;
} svc_entry_t;

typedef struct
{
  uint16_t service_id;
  uint16_t method_id;
  uint8_t  msg_type;
  uint32_t count;
  uint32_t last_len;
} method_entry_t;

static struct udp_pcb *sd_pcb;
static struct udp_pcb *extra_pcb[EXTRA_PORT_MAX];
static uint8_t         rx_buf[RX_BUF_SIZE];
static svc_entry_t     svcs[APP_SOMEIP_MAX_SERVICES];
static method_entry_t  methods[MAX_METHODS];
static uint32_t        method_overflow;
static app_someip_counters_t cnt;
static uint32_t        table_tick;
static ip_addr_t       sd_mcast;
#if APP_SOMEIP_SEND_FIND
static uint32_t        find_tick;
static uint16_t        sd_session = 1U;
static uint8_t         sd_reboot  = 1U;
#endif

/* ---------------------------------------------------------------------------
 *  서비스 테이블
 * ------------------------------------------------------------------------ */
static svc_entry_t *svc_find(uint16_t service, uint16_t instance)
{
  uint32_t i;

  for (i = 0; i < APP_SOMEIP_MAX_SERVICES; i++)
  {
    if (svcs[i].used && (svcs[i].service_id == service) && (svcs[i].instance_id == instance))
    {
      return &svcs[i];
    }
  }
  return NULL;
}

static svc_entry_t *svc_alloc(void)
{
  uint32_t i;

  for (i = 0; i < APP_SOMEIP_MAX_SERVICES; i++)
  {
    if (!svcs[i].used)
    {
      memset(&svcs[i], 0, sizeof(svcs[i]));
      svcs[i].used = 1U;
      return &svcs[i];
    }
  }
  return NULL;
}

static void print_svc(const svc_entry_t *s)
{
  uint32_t age = HAL_GetTick() - s->last_seen;

  VLOG("[SD  ]   0x%04X/0x%04X v%u.%lu", (unsigned)s->service_id, (unsigned)s->instance_id,
       (unsigned)s->major, (unsigned long)s->minor);
  if (s->has_ep)
  {
    VLOG("  %s %u.%u.%u.%u:%u", (s->ep_proto == 0x06U) ? "TCP" : "UDP",
         s->ep_ip[0], s->ep_ip[1], s->ep_ip[2], s->ep_ip[3], (unsigned)s->ep_port);
  }
  VLOG("  from %s", ip4addr_ntoa(&s->from));
  if (s->ttl == SOMEIP_SD_TTL_INFINITE)
  {
    VLOG("  ttl inf");
  }
  else
  {
    VLOG("  ttl %lus", (unsigned long)s->ttl);
  }
  VLOG("  offers %lu  age %lu.%lus\r\n", (unsigned long)s->offers,
       (unsigned long)(age / 1000U), (unsigned long)((age % 1000U) / 100U));
}

/* ---------------------------------------------------------------------------
 *  메서드/이벤트 집계
 * ------------------------------------------------------------------------ */
static void method_count(const someip_msg_t *m)
{
  uint32_t i;

  for (i = 0; i < MAX_METHODS; i++)
  {
    if ((methods[i].count != 0U) && (methods[i].service_id == m->service_id) &&
        (methods[i].method_id == m->method_id))
    {
      methods[i].count++;
      methods[i].last_len = m->payload_len;
      methods[i].msg_type = m->message_type;
      return;
    }
  }
  for (i = 0; i < MAX_METHODS; i++)
  {
    if (methods[i].count == 0U)
    {
      methods[i].service_id = m->service_id;
      methods[i].method_id  = m->method_id;
      methods[i].msg_type   = m->message_type;
      methods[i].last_len   = m->payload_len;
      methods[i].count      = 1U;
      return;
    }
  }
  method_overflow++;
}

/* ---------------------------------------------------------------------------
 *  SD 처리
 * ------------------------------------------------------------------------ */
static void handle_sd(const someip_msg_t *m, const ip_addr_t *from, u16_t port)
{
  someip_sd_t sd;
  someip_sd_entry_t e;
  someip_sd_option_t ep;
  uint32_t i, n;

  cnt.rx_sd_messages++;
  if (someip_sd_parse(m->payload, m->payload_len, &sd) != 0)
  {
    cnt.rx_errors++;
    VLOG_RL("[SD  ] malformed SD from %s\r\n", ipaddr_ntoa(from));
    return;
  }

  n = someip_sd_entry_count(&sd);
  for (i = 0; i < n; i++)
  {
    uint8_t has_ep;

    if (someip_sd_get_entry(&sd, i, &e) != 0)
    {
      break;
    }
    has_ep = (someip_sd_entry_endpoint(&sd, &e, &ep) == 0) ? 1U : 0U;

    if ((e.type == SOMEIP_SD_SUBSCRIBE) || (e.type == SOMEIP_SD_SUBSCRIBE_ACK))
    {
      VLOG_RL("[SD  ] %s:%u %s svc 0x%04X inst 0x%04X eg 0x%04X v%u ttl %lu\r\n",
              ipaddr_ntoa(from), (unsigned)port, someip_sd_entry_name(&e),
              (unsigned)e.service_id, (unsigned)e.instance_id, (unsigned)e.eventgroup_id,
              (unsigned)e.major_version, (unsigned long)e.ttl);
      continue;
    }

    if (e.type == SOMEIP_SD_FIND_SERVICE)
    {
      VLOG_RL("[SD  ] %s:%u FindService svc 0x%04X inst 0x%04X\r\n", ipaddr_ntoa(from),
              (unsigned)port, (unsigned)e.service_id, (unsigned)e.instance_id);
      continue;
    }

    if (e.type == SOMEIP_SD_OFFER_SERVICE)
    {
      svc_entry_t *s = svc_find(e.service_id, e.instance_id);

#if APP_ECU_TABLE_ENABLE
      if (e.ttl != 0U)
      {
        veh_ecu_on_offer(lwip_ntohl(ip4_addr_get_u32(ip_2_ip4(from))), e.service_id,
                         e.instance_id, e.major_version, veh_time_us());
      }
#endif

      if (e.ttl == 0U)
      {
        if (s != NULL)
        {
          VLOG("[SD  ] StopOffer 0x%04X/0x%04X\r\n", (unsigned)e.service_id, (unsigned)e.instance_id);
          s->used = 0U;
        }
        continue;
      }

      if (s == NULL)
      {
        s = svc_alloc();
        if (s == NULL)
        {
          VLOG_RL("[SD  ] service table full (APP_SOMEIP_MAX_SERVICES)\r\n");
          continue;
        }
        s->service_id  = e.service_id;
        s->instance_id = e.instance_id;
        s->last_seen   = HAL_GetTick();
        VLOG("[SD  ] NEW service:\r\n");
        s->major  = e.major_version;
        s->minor  = e.minor_version;
        s->ttl    = e.ttl;
        s->has_ep = has_ep;
        if (has_ep)
        {
          memcpy(s->ep_ip, ep.ipv4, 4);
          s->ep_proto = ep.l4_proto;
          s->ep_port  = ep.port;
        }
        ip4_addr_copy(s->from, *ip_2_ip4(from));
        s->offers = 1U;
        print_svc(s);
        continue;
      }

      s->major     = e.major_version;
      s->minor     = e.minor_version;
      s->ttl       = e.ttl;
      s->last_seen = HAL_GetTick();
      s->offers++;
      if (has_ep)
      {
        s->has_ep = 1U;
        memcpy(s->ep_ip, ep.ipv4, 4);
        s->ep_proto = ep.l4_proto;
        s->ep_port  = ep.port;
      }
      ip4_addr_copy(s->from, *ip_2_ip4(from));
      continue;
    }

    VLOG_RL("[SD  ] %s unknown entry type 0x%02X\r\n", ipaddr_ntoa(from), (unsigned)e.type);
  }
}

/* ---------------------------------------------------------------------------
 *  일반 SOME/IP 메시지
 * ------------------------------------------------------------------------ */
static void handle_msg(const someip_msg_t *m, const ip_addr_t *from, u16_t port)
{
#if APP_USE_UART_LOG
  uint32_t i, n;
#endif

  method_count(m);

  if (!veh_log_allow())
  {
    return;
  }
#if APP_USE_UART_LOG
  printf("[SOME] %s:%u svc 0x%04X %s 0x%04X len %lu cli 0x%04X ses 0x%04X %s %s if%u",
         ipaddr_ntoa(from), (unsigned)port, (unsigned)m->service_id,
         someip_is_event(m) ? "evt" : "mth", (unsigned)m->method_id,
         (unsigned long)m->payload_len, (unsigned)m->client_id, (unsigned)m->session_id,
         someip_msg_type_name(m->message_type), someip_return_code_name(m->return_code),
         (unsigned)m->interface_version);
  if (m->protocol_version != SOMEIP_PROTOCOL_VERSION)
  {
    printf(" (proto %u!)", (unsigned)m->protocol_version);
  }
  n = (m->payload_len < APP_SOMEIP_LOG_PAYLOAD) ? m->payload_len : APP_SOMEIP_LOG_PAYLOAD;
  if (n > 0U)
  {
    printf(" |");
    for (i = 0; i < n; i++)
    {
      printf(" %02X", m->payload[i]);
    }
    if (m->payload_len > n)
    {
      printf(" ..");
    }
  }
  printf("\r\n");
#else
  LWIP_UNUSED_ARG(from);
  LWIP_UNUSED_ARG(port);
#endif
}

/* ---------------------------------------------------------------------------
 *  UDP 수신
 * ------------------------------------------------------------------------ */
static void someip_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                        const ip_addr_t *addr, u16_t port)
{
  uint32_t len, off = 0U;

  LWIP_UNUSED_ARG(arg);
  LWIP_UNUSED_ARG(pcb);

  if (p == NULL)
  {
    return;
  }
  cnt.rx_datagrams++;

  /* 체인 pbuf 를 연속 버퍼로 */
  len = pbuf_copy_partial(p, rx_buf, (u16_t)((p->tot_len < RX_BUF_SIZE) ? p->tot_len : RX_BUF_SIZE), 0);
  pbuf_free(p);

  /* UDP 데이터그램 하나에 SOME/IP 메시지가 여러 개 들어 있을 수 있음 */
  while (off < len)
  {
    someip_msg_t m;
    int32_t used = someip_parse(&rx_buf[off], len - off, &m);

    if (used <= 0)
    {
      cnt.rx_errors++;
      VLOG_RL("[SOME] %s:%u malformed (%ld) at offset %lu of %lu\r\n", ipaddr_ntoa(addr),
              (unsigned)port, (long)used, (unsigned long)off, (unsigned long)len);
      break;
    }
    cnt.rx_messages++;
#if APP_ECU_TABLE_ENABLE
    veh_ecu_on_someip(lwip_ntohl(ip4_addr_get_u32(ip_2_ip4(addr))), m.service_id, m.method_id,
                      m.message_type, m.payload, m.payload_len, veh_time_us());
#endif

    if (someip_is_sd(&m))
    {
      handle_sd(&m, addr, port);
    }
    else
    {
      handle_msg(&m, addr, port);
    }
    off += (uint32_t)used;
  }
}

static struct udp_pcb *open_port(uint16_t port)
{
  struct udp_pcb *pcb = udp_new();

  if (pcb == NULL)
  {
    VLOG("[SOME] udp_new failed (port %u)\r\n", (unsigned)port);
    return NULL;
  }
  if (udp_bind(pcb, IP_ADDR_ANY, port) != ERR_OK)
  {
    VLOG("[SOME] bind failed (port %u)\r\n", (unsigned)port);
    udp_remove(pcb);
    return NULL;
  }
  udp_recv(pcb, someip_recv, NULL);
  return pcb;
}

#if APP_SOMEIP_SEND_FIND
static void send_find(void)
{
  uint8_t msg[48];
  uint32_t len;
  struct pbuf *p;

  len = someip_sd_build_find(msg, sizeof(msg), APP_SOMEIP_CLIENT_ID, sd_session,
                             (uint8_t)((sd_reboot ? SOMEIP_SD_FLAG_REBOOT : 0U) | SOMEIP_SD_FLAG_UNICAST),
                             0xFFFFU, 0xFFFFU, 0xFFU, 0xFFFFFFFFU, 3U);
  if ((len == 0U) || (sd_pcb == NULL))
  {
    return;
  }
  p = pbuf_alloc(PBUF_TRANSPORT, (u16_t)len, PBUF_RAM);
  if (p == NULL)
  {
    return;
  }
  memcpy(p->payload, msg, len);
  if (udp_sendto(sd_pcb, p, &sd_mcast, APP_SOMEIP_SD_PORT) == ERR_OK)
  {
    cnt.tx_find++;
  }
  pbuf_free(p);

  sd_session++;
  if (sd_session == 0U)          /* Session ID 는 1 부터, 한 바퀴 돌면 reboot 플래그 해제 */
  {
    sd_session = 1U;
    sd_reboot  = 0U;
  }
}
#endif

/* ---------------------------------------------------------------------------
 *  공개 함수
 * ------------------------------------------------------------------------ */
void APP_SOMEIP_Init(void)
{
  static const uint16_t extra[] = APP_SOMEIP_EXTRA_PORTS;
  uint32_t i, k = 0U;

  static const uint8_t mc[4] = { APP_SOMEIP_SD_MCAST_ADDR };

  IP_ADDR4(&sd_mcast, mc[0], mc[1], mc[2], mc[3]);

  sd_pcb = open_port(APP_SOMEIP_SD_PORT);
  VLOG("[SOME] SD listen udp/%u, multicast %s (IGMP join %s)\r\n", (unsigned)APP_SOMEIP_SD_PORT,
       ipaddr_ntoa(&sd_mcast), APP_SOMEIP_IGMP_JOIN ? "on" : "off - passive");

#if LWIP_IGMP
  if (igmp_joingroup_netif(&gnetif, ip_2_ip4(&sd_mcast)) != ERR_OK)
  {
    VLOG("[SOME] IGMP join failed\r\n");
  }
#endif

  for (i = 0; (i < (sizeof(extra) / sizeof(extra[0]))) && (k < EXTRA_PORT_MAX); i++)
  {
    if ((extra[i] != 0U) && (extra[i] != APP_SOMEIP_SD_PORT))
    {
      extra_pcb[k] = open_port(extra[i]);
      if (extra_pcb[k] != NULL)
      {
        VLOG("[SOME] listen udp/%u\r\n", (unsigned)extra[i]);
        k++;
      }
    }
  }

#if APP_SOMEIP_SEND_FIND
  VLOG("[SOME] WARNING: FindService TX enabled (every %lu ms)\r\n",
       (unsigned long)APP_SOMEIP_FIND_PERIOD_MS);
  find_tick = HAL_GetTick();
#endif
  table_tick = HAL_GetTick();
}

void APP_SOMEIP_Process(void)
{
  uint32_t i, now = HAL_GetTick();

  /* TTL 만료 */
  for (i = 0; i < APP_SOMEIP_MAX_SERVICES; i++)
  {
    if (svcs[i].used && (svcs[i].ttl != SOMEIP_SD_TTL_INFINITE) &&
        ((now - svcs[i].last_seen) > ((svcs[i].ttl * 1000U) + 500U)))
    {
      VLOG("[SD  ] expired 0x%04X/0x%04X\r\n", (unsigned)svcs[i].service_id,
           (unsigned)svcs[i].instance_id);
      svcs[i].used = 0U;
    }
  }

#if APP_SOMEIP_SEND_FIND
  if (netif_is_link_up(&gnetif) && ((now - find_tick) >= APP_SOMEIP_FIND_PERIOD_MS))
  {
    find_tick = now;
    send_find();
  }
#endif

  if ((now - table_tick) >= APP_SOMEIP_TABLE_PERIOD_MS)
  {
    table_tick = now;
    APP_SOMEIP_PrintTable();
  }
}

void APP_SOMEIP_PrintTable(void)
{
  uint32_t i;

  VLOG("[SD  ] rx dgram=%lu msg=%lu sd=%lu err=%lu find_tx=%lu, services=%lu\r\n",
       (unsigned long)cnt.rx_datagrams, (unsigned long)cnt.rx_messages,
       (unsigned long)cnt.rx_sd_messages, (unsigned long)cnt.rx_errors,
       (unsigned long)cnt.tx_find, (unsigned long)APP_SOMEIP_ServiceCount());
  for (i = 0; i < APP_SOMEIP_MAX_SERVICES; i++)
  {
    if (svcs[i].used)
    {
      print_svc(&svcs[i]);
    }
  }
  for (i = 0; i < MAX_METHODS; i++)
  {
    if (methods[i].count != 0U)
    {
      VLOG("[SOME]   0x%04X %s 0x%04X %-12s count %lu last_len %lu\r\n",
           (unsigned)methods[i].service_id, (methods[i].method_id & 0x8000U) ? "evt" : "mth",
           (unsigned)methods[i].method_id, someip_msg_type_name(methods[i].msg_type),
           (unsigned long)methods[i].count, (unsigned long)methods[i].last_len);
    }
  }
  if (method_overflow != 0U)
  {
    VLOG("[SOME]   (method table full, +%lu)\r\n", (unsigned long)method_overflow);
  }
}

int APP_SOMEIP_GetService(uint32_t index, app_someip_service_t *out)
{
  uint32_t i, k = 0U;

  for (i = 0; i < APP_SOMEIP_MAX_SERVICES; i++)
  {
    if (!svcs[i].used)
    {
      continue;
    }
    if (k++ == index)
    {
      out->service_id  = svcs[i].service_id;
      out->instance_id = svcs[i].instance_id;
      out->major       = svcs[i].major;
      out->minor       = svcs[i].minor;
      out->ttl         = svcs[i].ttl;
      out->has_ep      = svcs[i].has_ep;
      memcpy(out->ep_ip, svcs[i].ep_ip, 4);
      out->ep_proto    = svcs[i].ep_proto;
      out->ep_port     = svcs[i].ep_port;
      out->from_ip     = lwip_ntohl(ip4_addr_get_u32(&svcs[i].from));
      out->offers      = svcs[i].offers;
      return 0;
    }
  }
  return -1;
}

uint32_t APP_SOMEIP_ServiceCount(void)
{
  uint32_t i, n = 0U;

  for (i = 0; i < APP_SOMEIP_MAX_SERVICES; i++)
  {
    n += svcs[i].used;
  }
  return n;
}

const app_someip_counters_t *APP_SOMEIP_Counters(void)
{
  return &cnt;
}

#else /* !APP_SOMEIP_ENABLE */

static app_someip_counters_t cnt;
void APP_SOMEIP_Init(void) {}
void APP_SOMEIP_Process(void) {}
void APP_SOMEIP_PrintTable(void) {}
int  APP_SOMEIP_GetService(uint32_t index, app_someip_service_t *out) { (void)index; (void)out; return -1; }
uint32_t APP_SOMEIP_ServiceCount(void) { return 0U; }
const app_someip_counters_t *APP_SOMEIP_Counters(void) { return &cnt; }

#endif /* APP_SOMEIP_ENABLE */
