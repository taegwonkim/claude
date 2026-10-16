/**
  ******************************************************************************
  * @file    vcan_proto.c
  * @brief   PC 링크 프로토콜 (CAN FD) - 프레임 생성 / 명령 해석
  ******************************************************************************
  */

#include "vcan_proto.h"
#include <string.h>

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v)
{
  p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* ---------------------------------------------------------------------------
 *  CAN FD 길이
 * ------------------------------------------------------------------------ */
static const uint8_t dlc_len[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64 };

uint8_t vcan_round_len(uint8_t len)
{
  uint8_t i;

  for (i = 0; i < 16U; i++)
  {
    if (dlc_len[i] >= len)
    {
      return dlc_len[i];
    }
  }
  return 64U;
}

uint8_t vcan_len_to_dlc(uint8_t len)
{
  uint8_t i, r = vcan_round_len(len);

  for (i = 0; i < 16U; i++)
  {
    if (dlc_len[i] == r)
    {
      return i;
    }
  }
  return 15U;
}

uint8_t vcan_dlc_to_len(uint8_t dlc)
{
  return dlc_len[dlc & 0x0FU];
}

/* ---------------------------------------------------------------------------
 *  생성
 * ------------------------------------------------------------------------ */
/* STATUS (32 B)
 *  0 ver | 1 flags | 2 role | 3 sqi | 4 uptime_ms(4) | 8 ecu_cnt(2) | 10 svc_cnt(2)
 * 12 rx_frames(4) | 16 tx_frames(4) | 20 rx_lost(4) | 24 someip_msgs(4) | 28 ip(4, a.b.c.d 순) */
uint8_t vcan_encode_status(uint8_t *buf, const vcan_status_t *s)
{
  memset(buf, 0, VCAN_LEN_STATUS);
  buf[0] = VCAN_PROTO_VERSION;
  buf[1] = s->flags;
  buf[2] = s->phy_role;
  buf[3] = s->sqi;
  put32(&buf[4], s->uptime_ms);
  put16(&buf[8], s->ecu_count);
  put16(&buf[10], s->service_count);
  put32(&buf[12], s->rx_frames);
  put32(&buf[16], s->tx_frames);
  put32(&buf[20], s->rx_lost);
  put32(&buf[24], s->someip_messages);
  buf[28] = (uint8_t)(s->ip >> 24);         /* IP 는 사람이 읽는 순서대로 */
  buf[29] = (uint8_t)(s->ip >> 16);
  buf[30] = (uint8_t)(s->ip >> 8);
  buf[31] = (uint8_t)s->ip;
  return VCAN_LEN_STATUS;
}

/* ETH_STATS (48 B) : u32 x 12
 *  rx_frames, tx_frames, rx_dma_missed, rx_fifo_overflow, rx_fifo_missed, rx_pool_empty,
 *  rx_crc_errors, rx_align_errors, tx_errors, mon_frames, mon_tagged, vlan_dropped */
uint8_t vcan_encode_eth_stats(uint8_t *buf, const vcan_eth_stats_t *s)
{
  const uint32_t v[12] =
  {
    s->rx_frames, s->tx_frames, s->rx_dma_missed, s->rx_fifo_overflow, s->rx_fifo_missed,
    s->rx_pool_empty, s->rx_crc_errors, s->rx_align_errors, s->tx_errors,
    s->mon_frames, s->mon_tagged, s->vlan_dropped
  };
  uint32_t i;

  for (i = 0; i < 12U; i++)
  {
    put32(&buf[i * 4U], v[i]);
  }
  return VCAN_LEN_ETH_STATS;
}

static void put_ip(uint8_t *p, uint32_t ip)
{
  p[0] = (uint8_t)(ip >> 24); p[1] = (uint8_t)(ip >> 16); p[2] = (uint8_t)(ip >> 8); p[3] = (uint8_t)ip;
}

/* ECU (48 B)
 *  0 index | 1 total | 2 ip(4) | 6 mac(6) | 12 vid(2, 0xFFFF=없음) | 14 flags(b0 mac_changed, b1 idle)
 * 15 num_offers | 16 packets(4) | 20 bytes(4) | 24 last_age_ms(4) | 28 n_flows | 29 n_events
 * 30 flow_overflow(1, 255 포화) | 31 event_overflow(1) | 32 offers 4 x (service(2), instance(2)) */
uint8_t vcan_encode_ecu(uint8_t *buf, const veh_ecu_t *e, uint8_t index, uint8_t total,
                        uint32_t now_us, uint32_t idle_us)
{
  uint32_t i, nf = 0U, ne = 0U, age = now_us - e->last_us;

  memset(buf, 0, VCAN_LEN_ECU);
  for (i = 0; i < APP_ECU_MAX_FLOWS; i++)  nf += (e->flows[i].iv.count != 0U);
  for (i = 0; i < APP_ECU_MAX_EVENTS; i++) ne += (e->events[i].iv.count != 0U);

  buf[0] = index;
  buf[1] = total;
  put_ip(&buf[2], e->ip);
  memcpy(&buf[6], e->mac, 6);
  put16(&buf[12], e->vid);
  buf[14] = (uint8_t)((e->mac_changed ? 0x01U : 0U) | ((age > idle_us) ? 0x02U : 0U));
  buf[15] = e->num_offers;
  put32(&buf[16], e->packets);
  put32(&buf[20], e->bytes);
  put32(&buf[24], age / 1000U);
  buf[28] = (uint8_t)nf;
  buf[29] = (uint8_t)ne;
  buf[30] = (uint8_t)((e->flow_overflow > 255U) ? 255U : e->flow_overflow);
  buf[31] = (uint8_t)((e->event_overflow > 255U) ? 255U : e->event_overflow);
  for (i = 0; (i < e->num_offers) && (i < 4U); i++)
  {
    put16(&buf[32U + (i * 4U)], e->offers[i].service_id);
    put16(&buf[34U + (i * 4U)], e->offers[i].instance_id);
  }
  return VCAN_LEN_ECU;
}

/* EVENT (48 B)
 *  0 ecu_ip(4) | 4 service(2) | 6 method(2) | 8 msg_type | 9 n_payload | 10 rsv(2)
 * 12 count(4) | 16 avg_us(4) | 20 min_us(4) | 24 max_us(4) | 28 last_len(4) | 32 payload(8) | 40 rsv(8) */
uint8_t vcan_encode_event(uint8_t *buf, uint32_t ecu_ip, const veh_event_t *ev)
{
  uint32_t n = (ev->last_len < APP_ECU_PAYLOAD_BYTES) ? ev->last_len : APP_ECU_PAYLOAD_BYTES;

  if (n > 8U)
  {
    n = 8U;
  }
  memset(buf, 0, VCAN_LEN_EVENT);
  put_ip(&buf[0], ecu_ip);
  put16(&buf[4], ev->service_id);
  put16(&buf[6], ev->method_id);
  buf[8] = ev->msg_type;
  buf[9] = (uint8_t)n;
  put32(&buf[12], ev->iv.count);
  put32(&buf[16], ev->iv.avg_us);
  put32(&buf[20], ev->iv.min_us);
  put32(&buf[24], ev->iv.max_us);
  put32(&buf[28], ev->last_len);
  memcpy(&buf[32], ev->last_payload, n);
  return VCAN_LEN_EVENT;
}

/* FLOW (24 B)
 *  0 ecu_ip(4) | 4 proto | 5 rsv | 6 sport(2) | 8 dport(2) | 10 rsv(2) | 12 count(4) | 16 avg_us(4) | 20 max_us(4) */
uint8_t vcan_encode_flow(uint8_t *buf, uint32_t ecu_ip, const veh_flow_t *f)
{
  memset(buf, 0, VCAN_LEN_FLOW);
  put_ip(&buf[0], ecu_ip);
  buf[4] = f->proto;
  put16(&buf[6], f->sport);
  put16(&buf[8], f->dport);
  put32(&buf[12], f->iv.count);
  put32(&buf[16], f->iv.avg_us);
  put32(&buf[20], f->iv.max_us);
  return VCAN_LEN_FLOW;
}

/* SERVICE (32 B)
 *  0 service(2) | 2 instance(2) | 4 major | 5 l4_proto | 6 rsv(2) | 8 minor(4) | 12 ttl(4)
 * 16 ep_ip(4) | 20 ep_port(2) | 22 rsv(2) | 24 from_ip(4) | 28 offers(4) */
uint8_t vcan_encode_service(uint8_t *buf, const vcan_service_t *s)
{
  memset(buf, 0, VCAN_LEN_SERVICE);
  put16(&buf[0], s->service_id);
  put16(&buf[2], s->instance_id);
  buf[4] = s->major;
  buf[5] = s->l4_proto;
  put32(&buf[8], s->minor);
  put32(&buf[12], s->ttl);
  put_ip(&buf[16], s->ep_ip);
  put16(&buf[20], s->ep_port);
  put_ip(&buf[24], s->from_ip);
  put32(&buf[28], s->offers);
  return VCAN_LEN_SERVICE;
}

/* DUMP_END (8 B) : 0 records(2) | 2 tx_dropped(2) | 4 rsv(4) */
uint8_t vcan_encode_dump_end(uint8_t *buf, uint16_t records, uint16_t dropped)
{
  memset(buf, 0, VCAN_LEN_DUMP_END);
  put16(&buf[0], records);
  put16(&buf[2], dropped);
  return VCAN_LEN_DUMP_END;
}

/* RESPONSE (16 B)
 *  0 cmd | 1 result | 2 proto_ver | 3 rsv | 4 fw_ver(2) | 6 rsv(2) | 8 uptime_ms(4) | 12 value(4) */
uint8_t vcan_encode_response(uint8_t *buf, uint8_t cmd, uint8_t result,
                             uint32_t uptime_ms, uint32_t value)
{
  memset(buf, 0, VCAN_LEN_RESPONSE);
  buf[0] = cmd;
  buf[1] = result;
  buf[2] = VCAN_PROTO_VERSION;
  put16(&buf[4], VCAN_FW_VERSION);
  put32(&buf[8], uptime_ms);
  put32(&buf[12], value);
  return VCAN_LEN_RESPONSE;
}

/* ---------------------------------------------------------------------------
 *  명령 해석
 * ------------------------------------------------------------------------ */
int vcan_decode_command(const uint8_t *data, uint8_t len, vcan_command_t *out)
{
  if ((data == NULL) || (out == NULL) || (len == 0U) || (len > VCAN_MAX_LEN))
  {
    return -1;
  }
  memset(out, 0, sizeof(*out));
  out->cmd = data[0];
  out->len = (uint8_t)(len - 1U);
  memcpy(out->arg, &data[1], out->len);
  return 0;
}
