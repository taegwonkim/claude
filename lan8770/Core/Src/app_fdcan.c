/**
  ******************************************************************************
  * @file    app_fdcan.c
  * @brief   PC 링크 : FDCAN1 (CAN FD, 500 kbit/s / 2 Mbit/s, BRS)
  ******************************************************************************
  * - 주기 전송 : STATUS, ETH_STATS (APP_PCLINK_STATUS_MS)
  * - 덤프     : ECU → 그 ECU 의 FLOW / EVENT → SERVICE → DUMP_END
  *              APP_PCLINK_DUMP_MS 마다 또는 PC 의 DUMP 명령으로 시작.
  *              메인 루프 한 번에 APP_PCLINK_DUMP_BURST 프레임씩, Tx FIFO 여유를 보며 전송
  * - 명령     : ID = BASE + 0x40 (PING / DUMP / RESET_STATS / SET_STATUS_MS / SET_DUMP_MS)
  * - Bus-Off  : 100 ms 마다 확인해서 자동 복구
  * - 인터럽트를 쓰지 않고 메인 루프에서 폴링 (lwIP NO_SYS 구조와 동일)
  ******************************************************************************
  */

#include "main.h"
#include "app_fdcan.h"
#include "pc_link_conf.h"
#include "vcan_proto.h"
#include "veh_ecu.h"
#include "veh_vlan.h"
#include "veh_log.h"
#include "app_someip.h"
#include "ethernetif.h"
#include "vehicle_net_conf.h"
#include "lwip/netif.h"
#include <string.h>

#if APP_PCLINK_ENABLE

extern FDCAN_HandleTypeDef hfdcan1;
extern struct netif gnetif;

typedef enum
{
  DUMP_IDLE = 0,
  DUMP_ECU,
  DUMP_SERVICE,
  DUMP_END
} dump_phase_t;

static app_fdcan_counters_t cnt;
static uint32_t status_ms = APP_PCLINK_STATUS_MS;
static uint32_t dump_ms   = APP_PCLINK_DUMP_MS;
static uint32_t status_tick, dump_tick, busoff_tick;

static dump_phase_t dump_phase;
static uint32_t dump_ecu;        /* 몇 번째 ECU                      */
static uint32_t dump_sub;        /* 0 = ECU 레코드, 1.. = 흐름 → 이벤트 */
static uint32_t dump_svc;
static uint16_t dump_records;
static uint32_t dump_drop_start;

/* ---------------------------------------------------------------------------
 *  송신
 * ------------------------------------------------------------------------ */
static int send_frame(uint8_t id_offset, const uint8_t *data, uint8_t len)
{
  FDCAN_TxHeaderTypeDef h;
  uint8_t buf[VCAN_MAX_LEN];
  uint8_t flen = vcan_round_len(len);

  if (!cnt.started)
  {
    return -1;
  }
  memset(buf, 0, sizeof(buf));
  memcpy(buf, data, len);

  h.Identifier          = APP_PCLINK_ID_BASE + id_offset;
  h.IdType              = FDCAN_STANDARD_ID;
  h.TxFrameType         = FDCAN_DATA_FRAME;
  h.DataLength          = vcan_len_to_dlc(flen);    /* HAL 의 FDCAN_DLC_BYTES_x 와 같은 코드 */
  h.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  h.BitRateSwitch       = FDCAN_BRS_ON;
  h.FDFormat            = FDCAN_FD_CAN;
  h.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
  h.MessageMarker       = 0U;

  if ((HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) == 0U) ||
      (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &h, buf) != HAL_OK))
  {
    cnt.tx_dropped++;
    return -1;
  }
  cnt.tx_frames++;
  return 0;
}

/* ---------------------------------------------------------------------------
 *  주기 프레임
 * ------------------------------------------------------------------------ */
static void send_status(void)
{
  vcan_status_t st;
  vcan_eth_stats_t es;
  const ethernetif_stats_t *e = ethernetif_stats();
  const veh_vlan_counters_t *v = veh_vlan_counters();
  lan8770_Role_t role;
  uint8_t buf[VCAN_MAX_LEN], len;

  memset(&st, 0, sizeof(st));
  st.flags = (uint8_t)((netif_is_link_up(&gnetif) ? VCAN_ST_LINK_UP : 0U) |
                       (ethernetif_phy_ready() ? VCAN_ST_PHY_READY : 0U) |
                       (APP_VLAN_ENABLE ? VCAN_ST_VLAN_ON : 0U) |
                       (APP_SOMEIP_ENABLE ? VCAN_ST_SOMEIP_ON : 0U) |
                       (e->counter_overflow ? VCAN_ST_CNT_OVERFLOW : 0U));
  st.phy_role = 0xFFU;
  st.sqi      = 0xFFU;
  if (ethernetif_phy_ready() && (LAN8770_GetRole(&LAN8770, &role) == LAN8770_STATUS_OK))
  {
    st.phy_role = (role == LAN8770_ROLE_MASTER) ? 1U : 0U;
  }
  if (netif_is_link_up(&gnetif))
  {
    uint8_t sqi;

    if (LAN8770_GetSQI(&LAN8770, &sqi) == LAN8770_STATUS_OK)
    {
      st.sqi = sqi;
    }
  }
  st.uptime_ms       = HAL_GetTick();
  st.ecu_count       = (uint16_t)veh_ecu_count();
  st.service_count   = (uint16_t)APP_SOMEIP_ServiceCount();
  st.rx_frames       = e->rx_frames;
  st.tx_frames       = e->tx_frames;
  st.rx_lost         = ethernetif_stats_rx_lost();
  st.someip_messages = APP_SOMEIP_Counters()->rx_messages;
  st.ip              = lwip_ntohl(ip4_addr_get_u32(netif_ip4_addr(&gnetif)));
  len = vcan_encode_status(buf, &st);
  (void)send_frame(VCAN_ID_STATUS, buf, len);

  es.rx_frames        = e->rx_frames;
  es.tx_frames        = e->tx_frames;
  es.rx_dma_missed    = e->rx_dma_missed;
  es.rx_fifo_overflow = e->rx_fifo_overflow;
  es.rx_fifo_missed   = e->rx_fifo_missed;
  es.rx_pool_empty    = e->rx_pool_empty;
  es.rx_crc_errors    = e->rx_crc_errors;
  es.rx_align_errors  = e->rx_align_errors;
  es.tx_errors        = e->tx_errors;
  es.mon_frames       = v->rx_frames;
  es.mon_tagged       = v->rx_tagged;
  es.vlan_dropped     = v->rx_vlan_dropped;
  len = vcan_encode_eth_stats(buf, &es);
  (void)send_frame(VCAN_ID_ETH_STATS, buf, len);
}

/* ---------------------------------------------------------------------------
 *  덤프
 * ------------------------------------------------------------------------ */
void APP_FDCAN_StartDump(void)
{
  dump_phase      = DUMP_ECU;
  dump_ecu        = 0U;
  dump_sub        = 0U;
  dump_svc        = 0U;
  dump_records    = 0U;
  dump_drop_start = cnt.tx_dropped;
}

/* n 번째 사용 중인 흐름/이벤트 (없으면 NULL) */
static const veh_flow_t *nth_flow(const veh_ecu_t *e, uint32_t n)
{
  uint32_t i, k = 0U;

  for (i = 0; i < APP_ECU_MAX_FLOWS; i++)
  {
    if ((e->flows[i].iv.count != 0U) && (k++ == n))
    {
      return &e->flows[i];
    }
  }
  return NULL;
}

static const veh_event_t *nth_event(const veh_ecu_t *e, uint32_t n)
{
  uint32_t i, k = 0U;

  for (i = 0; i < APP_ECU_MAX_EVENTS; i++)
  {
    if ((e->events[i].iv.count != 0U) && (k++ == n))
    {
      return &e->events[i];
    }
  }
  return NULL;
}

static uint32_t count_flows(const veh_ecu_t *e)
{
  uint32_t i, n = 0U;

  for (i = 0; i < APP_ECU_MAX_FLOWS; i++)
  {
    n += (e->flows[i].iv.count != 0U);
  }
  return n;
}

/* 덤프 프레임 1 개 전송. 보낼 게 남았으면 1 */
static int dump_step(void)
{
  uint8_t buf[VCAN_MAX_LEN], len;

  switch (dump_phase)
  {
    case DUMP_ECU:
    {
      const veh_ecu_t *e = veh_ecu_get(dump_ecu);
      uint32_t nf;

      if (e == NULL)
      {
        dump_phase = DUMP_SERVICE;
        return 1;
      }
      nf = count_flows(e);
      if (dump_sub == 0U)
      {
        len = vcan_encode_ecu(buf, e, (uint8_t)dump_ecu, (uint8_t)veh_ecu_count(),
                              veh_time_us(), APP_ECU_IDLE_MS * 1000U);
        (void)send_frame(VCAN_ID_ECU, buf, len);
      }
      else if (dump_sub <= nf)
      {
        const veh_flow_t *f = nth_flow(e, dump_sub - 1U);

        if (f != NULL)
        {
          len = vcan_encode_flow(buf, e->ip, f);
          (void)send_frame(VCAN_ID_FLOW, buf, len);
        }
      }
      else
      {
        const veh_event_t *ev = nth_event(e, dump_sub - 1U - nf);

        if (ev == NULL)
        {
          dump_ecu++;           /* 이 ECU 끝 → 다음 ECU */
          dump_sub = 0U;
          return 1;
        }
        len = vcan_encode_event(buf, e->ip, ev);
        (void)send_frame(VCAN_ID_EVENT, buf, len);
      }
      dump_sub++;
      dump_records++;
      return 1;
    }

    case DUMP_SERVICE:
    {
      app_someip_service_t s;
      vcan_service_t vs;

      if (APP_SOMEIP_GetService(dump_svc, &s) != 0)
      {
        dump_phase = DUMP_END;
        return 1;
      }
      vs.service_id  = s.service_id;
      vs.instance_id = s.instance_id;
      vs.major       = s.major;
      vs.l4_proto    = s.has_ep ? s.ep_proto : 0U;
      vs.minor       = s.minor;
      vs.ttl         = s.ttl;
      vs.ep_ip       = ((uint32_t)s.ep_ip[0] << 24) | ((uint32_t)s.ep_ip[1] << 16) |
                       ((uint32_t)s.ep_ip[2] << 8) | s.ep_ip[3];
      vs.ep_port     = s.ep_port;
      vs.from_ip     = s.from_ip;
      vs.offers      = s.offers;
      len = vcan_encode_service(buf, &vs);
      (void)send_frame(VCAN_ID_SERVICE, buf, len);
      dump_svc++;
      dump_records++;
      return 1;
    }

    case DUMP_END:
    {
      uint32_t dropped = cnt.tx_dropped - dump_drop_start;

      len = vcan_encode_dump_end(buf, dump_records, (uint16_t)((dropped > 0xFFFFU) ? 0xFFFFU : dropped));
      (void)send_frame(VCAN_ID_DUMP_END, buf, len);
      dump_phase = DUMP_IDLE;
      return 0;
    }

    default:
      return 0;
  }
}

/* ---------------------------------------------------------------------------
 *  명령
 * ------------------------------------------------------------------------ */
static uint32_t get_u16(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }
static uint32_t get_u32(const uint8_t *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void handle_command(const uint8_t *data, uint8_t len)
{
  vcan_command_t c;
  uint8_t buf[VCAN_MAX_LEN], rlen, res = VCAN_RES_OK;
  uint32_t value = 0U;

  if (vcan_decode_command(data, len, &c) != 0)
  {
    return;
  }
  cnt.rx_commands++;

  switch (c.cmd)
  {
    case VCAN_CMD_PING:
      value = cnt.tx_frames;
      break;

    case VCAN_CMD_DUMP:
      APP_FDCAN_StartDump();
      break;

    case VCAN_CMD_RESET_STATS:
      ethernetif_stats_reset();
      veh_ecu_reset();
      VLOG("[PC  ] statistics reset by PC\r\n");
      break;

    case VCAN_CMD_SET_STATUS_MS:
      if (c.len < 2U)
      {
        res = VCAN_RES_BAD_ARG;
        break;
      }
      value = get_u16(c.arg);
      if ((value != 0U) && (value < 50U))
      {
        res = VCAN_RES_BAD_ARG;
        break;
      }
      status_ms = value;
      break;

    case VCAN_CMD_SET_DUMP_MS:
      if (c.len < 4U)
      {
        res = VCAN_RES_BAD_ARG;
        break;
      }
      value = get_u32(c.arg);
      if ((value != 0U) && (value < 1000U))
      {
        res = VCAN_RES_BAD_ARG;
        break;
      }
      dump_ms = value;
      break;

    default:
      res = VCAN_RES_UNKNOWN_CMD;
      break;
  }

  rlen = vcan_encode_response(buf, c.cmd, res, HAL_GetTick(), value);
  (void)send_frame(VCAN_ID_RESPONSE, buf, rlen);
}

static void poll_rx(void)
{
  FDCAN_RxHeaderTypeDef h;
  uint8_t data[VCAN_MAX_LEN];

  while (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) > 0U)
  {
    if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &h, data) != HAL_OK)
    {
      break;
    }
    if ((h.IdType == FDCAN_STANDARD_ID) && (h.Identifier == (APP_PCLINK_ID_BASE + VCAN_ID_COMMAND)))
    {
      handle_command(data, vcan_dlc_to_len((uint8_t)h.DataLength));
    }
  }
}

static void check_bus_off(void)
{
  FDCAN_ProtocolStatusTypeDef ps;

  if ((HAL_FDCAN_GetProtocolStatus(&hfdcan1, &ps) == HAL_OK) && (ps.BusOff != 0U))
  {
    cnt.bus_off++;
    VLOG_RL("[PC  ] FDCAN bus-off -> restart (%lu)\r\n", (unsigned long)cnt.bus_off);
    (void)HAL_FDCAN_Stop(&hfdcan1);
    (void)HAL_FDCAN_Start(&hfdcan1);
  }
}

/* ---------------------------------------------------------------------------
 *  공개 함수
 * ------------------------------------------------------------------------ */
void APP_FDCAN_Init(void)
{
  FDCAN_FilterTypeDef f;

  /* 명령 ID 하나만 Rx FIFO0 으로, 나머지는 모두 버림 */
  f.IdType       = FDCAN_STANDARD_ID;
  f.FilterIndex  = 0U;
  f.FilterType   = FDCAN_FILTER_MASK;
  f.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  f.FilterID1    = APP_PCLINK_ID_BASE + VCAN_ID_COMMAND;
  f.FilterID2    = 0x7FFU;
  f.RxBufferIndex       = 0U;
  f.IsCalibrationMsg    = 0U;

  if ((HAL_FDCAN_ConfigFilter(&hfdcan1, &f) != HAL_OK) ||
      (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_REJECT, FDCAN_REJECT,
                                    FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) != HAL_OK))
  {
    VLOG("[PC  ] FDCAN filter config failed\r\n");
    return;
  }

  /* 데이터 비트레이트 1 Mbit/s 이상에서는 송신 지연 보상이 필요 */
  if ((HAL_FDCAN_ConfigTxDelayCompensation(&hfdcan1,
                                           hfdcan1.Init.DataPrescaler * hfdcan1.Init.DataTimeSeg1,
                                           0U) != HAL_OK) ||
      (HAL_FDCAN_EnableTxDelayCompensation(&hfdcan1) != HAL_OK))
  {
    VLOG("[PC  ] FDCAN TDC config failed\r\n");
    return;
  }

  if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK)
  {
    VLOG("[PC  ] FDCAN start failed\r\n");
    return;
  }
  cnt.started = 1U;
  status_tick = dump_tick = busoff_tick = HAL_GetTick();
  VLOG("[PC  ] FDCAN1 PC link: CAN FD 500k/2M BRS, ID 0x%03X..0x%03X, cmd 0x%03X\r\n",
       (unsigned)APP_PCLINK_ID_BASE, (unsigned)(APP_PCLINK_ID_BASE + VCAN_ID_DUMP_END),
       (unsigned)(APP_PCLINK_ID_BASE + VCAN_ID_COMMAND));
}

void APP_FDCAN_Process(void)
{
  uint32_t now = HAL_GetTick(), i;

  if (!cnt.started)
  {
    return;
  }

  if ((now - busoff_tick) >= 100U)
  {
    busoff_tick = now;
    check_bus_off();
  }

  poll_rx();

  if ((status_ms != 0U) && ((now - status_tick) >= status_ms))
  {
    status_tick = now;
    send_status();
  }

  if ((dump_ms != 0U) && (dump_phase == DUMP_IDLE) && ((now - dump_tick) >= dump_ms))
  {
    dump_tick = now;
    APP_FDCAN_StartDump();
  }

  for (i = 0; (i < APP_PCLINK_DUMP_BURST) && (dump_phase != DUMP_IDLE); i++)
  {
    if (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) <= APP_PCLINK_TX_RESERVE)
    {
      break;
    }
    if (!dump_step())
    {
      break;
    }
  }
}

const app_fdcan_counters_t *APP_FDCAN_Counters(void)
{
  return &cnt;
}

#else /* !APP_PCLINK_ENABLE */

static app_fdcan_counters_t cnt;
void APP_FDCAN_Init(void) {}
void APP_FDCAN_Process(void) {}
void APP_FDCAN_StartDump(void) {}
const app_fdcan_counters_t *APP_FDCAN_Counters(void) { return &cnt; }

#endif /* APP_PCLINK_ENABLE */
