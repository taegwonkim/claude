/**
  ******************************************************************************
  * @file    veh_ecu.c
  * @brief   ECU 테이블 : 송신자 IP 별 집계
  ******************************************************************************
  * - 키 : 송신자 IPv4 주소
  * - MAC / VLAN : 그 IP 로 들어온 마지막 프레임 값. MAC 이 바뀌면 표시
  *   (게이트웨이를 거친 패킷은 게이트웨이 MAC 으로 보이므로 IP 기준이 더 확실)
  * - 흐름(flow) : (프로토콜, 송신 포트, 수신 포트) 별 개수와 송신 주기
  * - 이벤트     : SOME/IP (서비스, 메서드/이벤트) 별 개수, 주기, 마지막 payload
  * - Offer      : SOME/IP-SD 로 이 IP 가 제공한다고 알린 서비스
  ******************************************************************************
  */

#include "veh_ecu.h"
#include "someip.h"
#include "veh_log.h"
#include <string.h>

static veh_ecu_t ecus[APP_ECU_MAX];
static uint32_t  dropped;
static uint32_t  rr_index;

/* ---------------------------------------------------------------------------
 *  내부
 * ------------------------------------------------------------------------ */
static void iv_update(veh_interval_t *s, uint32_t now_us)
{
  if (s->count > 0U)
  {
    uint32_t d = now_us - s->last_us;

    if (s->count == 1U)
    {
      s->avg_us = d;
      s->min_us = d;
      s->max_us = d;
    }
    else
    {
      s->avg_us = (uint32_t)((int64_t)s->avg_us + (((int64_t)d - (int64_t)s->avg_us) / 8));
      if (d < s->min_us) s->min_us = d;
      if (d > s->max_us) s->max_us = d;
    }
  }
  s->last_us = now_us;
  s->count++;
}

static void print_ip(uint32_t ip)
{
  VLOG("%u.%u.%u.%u", (unsigned)(ip >> 24), (unsigned)((ip >> 16) & 0xFFU),
       (unsigned)((ip >> 8) & 0xFFU), (unsigned)(ip & 0xFFU));
}

static void print_mac(const uint8_t *m)
{
  VLOG("%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static void print_vid(uint16_t vid)
{
  if (vid == VEH_VLAN_NONE)
  {
    VLOG("untagged");
  }
  else
  {
    VLOG("VID %u", (unsigned)vid);
  }
}

/* µs 를 "12.3ms" 형식으로 */
static void print_ms(uint32_t us)
{
  VLOG("%lu.%lums", (unsigned long)(us / 1000U), (unsigned long)((us % 1000U) / 100U));
}

static void print_interval(const veh_interval_t *s)
{
  VLOG("count %lu", (unsigned long)s->count);
  if (s->count >= 2U)
  {
    VLOG("  period ");
    print_ms(s->avg_us);
    VLOG(" (");
    print_ms(s->min_us);
    VLOG("..");
    print_ms(s->max_us);
    VLOG(")");
  }
}

static veh_ecu_t *ecu_lookup(uint32_t ip, uint8_t create, uint32_t now_us)
{
  uint32_t i;

  for (i = 0; i < APP_ECU_MAX; i++)
  {
    if (ecus[i].used && (ecus[i].ip == ip))
    {
      return &ecus[i];
    }
  }
  if (!create)
  {
    return NULL;
  }
  for (i = 0; i < APP_ECU_MAX; i++)
  {
    if (!ecus[i].used)
    {
      memset(&ecus[i], 0, sizeof(ecus[i]));
      ecus[i].used     = 1U;
      ecus[i].ip       = ip;
      ecus[i].vid      = VEH_VLAN_NONE;
      ecus[i].first_us = now_us;
      ecus[i].last_us  = now_us;
      return &ecus[i];
    }
  }
  dropped++;
  return NULL;
}

/* ---------------------------------------------------------------------------
 *  입력
 * ------------------------------------------------------------------------ */
void veh_ecu_reset(void)
{
  memset(ecus, 0, sizeof(ecus));
  dropped  = 0U;
  rr_index = 0U;
}

void veh_ecu_on_frame(const veh_frame_info_t *fi, const veh_ipv4_info_t *ip, uint32_t now_us)
{
  veh_ecu_t *e;
  uint32_t i;
  uint8_t  is_new;

  if ((fi == NULL) || (ip == NULL))
  {
    return;
  }

  is_new = (ecu_lookup(ip->src, 0U, now_us) == NULL) ? 1U : 0U;
  e = ecu_lookup(ip->src, 1U, now_us);
  if (e == NULL)
  {
    return;
  }

  if (is_new)
  {
    memcpy(e->mac, fi->src, 6);
    e->vid = fi->vid;
    VLOG("[ECU ] NEW ");
    print_ip(e->ip);
    VLOG("  mac ");
    print_mac(e->mac);
    VLOG("  ");
    print_vid(e->vid);
    VLOG("\r\n");
  }
  else if (memcmp(e->mac, fi->src, 6) != 0)
  {
    e->mac_changed = 1U;
    memcpy(e->mac, fi->src, 6);
  }
  e->vid = fi->vid;
  e->packets++;
  e->bytes  += ip->total_len;
  e->last_us = now_us;

  /* 흐름 */
  for (i = 0; i < APP_ECU_MAX_FLOWS; i++)
  {
    veh_flow_t *f = &e->flows[i];

    if ((f->iv.count != 0U) && (f->proto == ip->proto) && (f->sport == ip->sport) && (f->dport == ip->dport))
    {
      iv_update(&f->iv, now_us);
      return;
    }
  }
  for (i = 0; i < APP_ECU_MAX_FLOWS; i++)
  {
    veh_flow_t *f = &e->flows[i];

    if (f->iv.count == 0U)
    {
      f->proto = ip->proto;
      f->sport = ip->sport;
      f->dport = ip->dport;
      iv_update(&f->iv, now_us);
      return;
    }
  }
  e->flow_overflow++;
}

void veh_ecu_on_someip(uint32_t src_ip, uint16_t service_id, uint16_t method_id,
                       uint8_t msg_type, const uint8_t *payload, uint32_t len,
                       uint32_t now_us)
{
  veh_ecu_t *e = ecu_lookup(src_ip, 1U, now_us);
  veh_event_t *ev = NULL;
  uint32_t i, n;

  if (e == NULL)
  {
    return;
  }
  e->last_us = now_us;

  for (i = 0; i < APP_ECU_MAX_EVENTS; i++)
  {
    if ((e->events[i].iv.count != 0U) && (e->events[i].service_id == service_id) &&
        (e->events[i].method_id == method_id))
    {
      ev = &e->events[i];
      break;
    }
  }
  if (ev == NULL)
  {
    for (i = 0; i < APP_ECU_MAX_EVENTS; i++)
    {
      if (e->events[i].iv.count == 0U)
      {
        ev = &e->events[i];
        ev->service_id = service_id;
        ev->method_id  = method_id;
        break;
      }
    }
  }
  if (ev == NULL)
  {
    e->event_overflow++;
    return;
  }

  ev->msg_type = msg_type;
  ev->last_len = len;
  n = (len < APP_ECU_PAYLOAD_BYTES) ? len : APP_ECU_PAYLOAD_BYTES;
  memset(ev->last_payload, 0, sizeof(ev->last_payload));
  if ((payload != NULL) && (n > 0U))
  {
    memcpy(ev->last_payload, payload, n);
  }
  iv_update(&ev->iv, now_us);
}

void veh_ecu_on_offer(uint32_t src_ip, uint16_t service_id, uint16_t instance_id, uint8_t major,
                      uint32_t now_us)
{
  veh_ecu_t *e = ecu_lookup(src_ip, 1U, now_us);
  uint32_t i;

  if (e == NULL)
  {
    return;
  }
  for (i = 0; i < e->num_offers; i++)
  {
    if ((e->offers[i].service_id == service_id) && (e->offers[i].instance_id == instance_id))
    {
      e->offers[i].major = major;
      return;
    }
  }
  if (e->num_offers < APP_ECU_MAX_OFFERS)
  {
    e->offers[e->num_offers].service_id  = service_id;
    e->offers[e->num_offers].instance_id = instance_id;
    e->offers[e->num_offers].major       = major;
    e->num_offers++;
  }
}

/* ---------------------------------------------------------------------------
 *  조회
 * ------------------------------------------------------------------------ */
uint32_t veh_ecu_count(void)
{
  uint32_t i, n = 0U;

  for (i = 0; i < APP_ECU_MAX; i++)
  {
    n += ecus[i].used;
  }
  return n;
}

const veh_ecu_t *veh_ecu_get(uint32_t index)
{
  uint32_t i, k = 0U;

  for (i = 0; i < APP_ECU_MAX; i++)
  {
    if (ecus[i].used)
    {
      if (k == index)
      {
        return &ecus[i];
      }
      k++;
    }
  }
  return NULL;
}

const veh_ecu_t *veh_ecu_find(uint32_t ip)
{
  return ecu_lookup(ip, 0U, 0U);
}

uint32_t veh_ecu_dropped(void)
{
  return dropped;
}

/* ---------------------------------------------------------------------------
 *  출력
 * ------------------------------------------------------------------------ */
void veh_ecu_print(const veh_ecu_t *e, uint32_t now_us, uint32_t idle_us)
{
  uint32_t i, k;
  uint32_t ago = now_us - e->last_us;

  VLOG("[ECU ] ");
  print_ip(e->ip);
  VLOG("  mac ");
  print_mac(e->mac);
  if (e->mac_changed)
  {
    VLOG("(changed)");
  }
  VLOG("  ");
  print_vid(e->vid);
  VLOG("  pkts %lu  bytes %lu  last %lu.%lus ago%s\r\n", (unsigned long)e->packets,
       (unsigned long)e->bytes, (unsigned long)(ago / 1000000U),
       (unsigned long)((ago % 1000000U) / 100000U), (ago > idle_us) ? " (idle)" : "");

  if (e->num_offers > 0U)
  {
    VLOG("[ECU ]    offers");
    for (i = 0; i < e->num_offers; i++)
    {
      VLOG(" 0x%04X/0x%04X v%u", (unsigned)e->offers[i].service_id,
           (unsigned)e->offers[i].instance_id, (unsigned)e->offers[i].major);
    }
    VLOG("\r\n");
  }

  for (i = 0; i < APP_ECU_MAX_FLOWS; i++)
  {
    const veh_flow_t *f = &e->flows[i];

    if (f->iv.count == 0U)
    {
      continue;
    }
    VLOG("[ECU ]    flow %-4s", veh_ipproto_name(f->proto));
    if ((f->proto == VEH_IPPROTO_UDP) || (f->proto == VEH_IPPROTO_TCP))
    {
      VLOG(" %5u->%-5u ", (unsigned)f->sport, (unsigned)f->dport);
    }
    else
    {
      VLOG(" proto %-5u   ", (unsigned)f->proto);
    }
    print_interval(&f->iv);
    VLOG("\r\n");
  }

  for (i = 0; i < APP_ECU_MAX_EVENTS; i++)
  {
    const veh_event_t *ev = &e->events[i];
    uint32_t n;

    if (ev->iv.count == 0U)
    {
      continue;
    }
    VLOG("[ECU ]    %s 0x%04X/0x%04X %-12s ", (ev->method_id & 0x8000U) ? "evt" : "mth",
         (unsigned)ev->service_id, (unsigned)ev->method_id, someip_msg_type_name(ev->msg_type));
    print_interval(&ev->iv);
    VLOG("  len %lu", (unsigned long)ev->last_len);
    n = (ev->last_len < APP_ECU_PAYLOAD_BYTES) ? ev->last_len : APP_ECU_PAYLOAD_BYTES;
    if (n > 0U)
    {
      VLOG(" |");
      for (k = 0; k < n; k++)
      {
        VLOG(" %02X", ev->last_payload[k]);
      }
      if (ev->last_len > n)
      {
        VLOG(" ..");
      }
    }
    VLOG("\r\n");
  }

  if ((e->flow_overflow | e->event_overflow) != 0U)
  {
    VLOG("[ECU ]    (table full: flows+%lu events+%lu)\r\n", (unsigned long)e->flow_overflow,
         (unsigned long)e->event_overflow);
  }
}

int veh_ecu_print_next(uint32_t now_us, uint32_t idle_us)
{
  uint32_t tries;

  for (tries = 0; tries < APP_ECU_MAX; tries++)
  {
    const veh_ecu_t *e = &ecus[rr_index];

    rr_index = (rr_index + 1U) % APP_ECU_MAX;
    if (e->used)
    {
      veh_ecu_print(e, now_us, idle_us);
      return 1;
    }
  }
  return 0;
}

void veh_ecu_print_summary(uint32_t now_us, uint32_t idle_us)
{
  uint32_t i;

  VLOG("[ECU ] %lu ECU(s)%s\r\n", (unsigned long)veh_ecu_count(),
       (dropped != 0U) ? " (table full, some IPs not recorded)" : "");
  for (i = 0; i < APP_ECU_MAX; i++)
  {
    const veh_ecu_t *e = &ecus[i];
    uint32_t k, flows = 0U, events = 0U;

    if (!e->used)
    {
      continue;
    }
    for (k = 0; k < APP_ECU_MAX_FLOWS; k++)  flows  += (e->flows[k].iv.count != 0U);
    for (k = 0; k < APP_ECU_MAX_EVENTS; k++) events += (e->events[k].iv.count != 0U);

    VLOG("[ECU ]   ");
    print_ip(e->ip);
    VLOG("  ");
    print_mac(e->mac);
    VLOG("  ");
    print_vid(e->vid);
    VLOG("  pkts %lu  flows %lu  events %lu  offers %u%s\r\n", (unsigned long)e->packets,
         (unsigned long)flows, (unsigned long)events, (unsigned)e->num_offers,
         ((now_us - e->last_us) > idle_us) ? "  (idle)" : "");
  }
}
