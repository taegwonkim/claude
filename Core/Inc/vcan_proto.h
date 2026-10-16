/**
  ******************************************************************************
  * @file    vcan_proto.h
  * @brief   PC 링크 프로토콜 (CAN FD) - 프레임 생성 / 명령 해석 (하드웨어 독립)
  ******************************************************************************
  * 모든 다바이트 값은 little endian. CAN ID 는 BASE(기본 0x700) + 오프셋.
  *
  *  장치 → PC
  *   +0x00 STATUS      32 B  상태 요약 (주기)
  *   +0x01 ETH_STATS   48 B  송수신 / 손실 카운터 (주기)
  *   +0x10 ECU         48 B  ECU 1 개 (IP, MAC, VLAN, 패킷, Offer 4 개)
  *   +0x11 EVENT       48 B  ECU 의 SOME/IP 이벤트 1 개 (주기 통계, payload 8 B)
  *   +0x12 FLOW        24 B  ECU 의 흐름 1 개 (프로토콜, 포트, 주기)
  *   +0x13 SERVICE     32 B  SOME/IP-SD 로 발견한 서비스 1 개
  *   +0x14 DUMP_END     8 B  덤프 끝 (레코드 수)
  *   +0x41 RESPONSE    16 B  명령 응답
  *  PC → 장치
  *   +0x40 COMMAND      8 B  [0]=명령, [1..] 인자
  *
  * 바이트 배치는 vcan_proto.c 의 각 vcan_encode_* 함수와 tools/vnet_can_monitor.py 참고
  ******************************************************************************
  */

#ifndef VCAN_PROTO_H
#define VCAN_PROTO_H

#include <stdint.h>
#include "veh_ecu.h"

#define VCAN_PROTO_VERSION      1U
#define VCAN_FW_VERSION         0x0100U    /* 1.0 */

/* CAN ID 오프셋 */
#define VCAN_ID_STATUS          0x00U
#define VCAN_ID_ETH_STATS       0x01U
#define VCAN_ID_ECU             0x10U
#define VCAN_ID_EVENT           0x11U
#define VCAN_ID_FLOW            0x12U
#define VCAN_ID_SERVICE         0x13U
#define VCAN_ID_DUMP_END        0x14U
#define VCAN_ID_COMMAND         0x40U
#define VCAN_ID_RESPONSE        0x41U

/* 프레임 길이 (CAN FD 유효 길이) */
#define VCAN_LEN_STATUS         32U
#define VCAN_LEN_ETH_STATS      48U
#define VCAN_LEN_ECU            48U
#define VCAN_LEN_EVENT          48U
#define VCAN_LEN_FLOW           24U
#define VCAN_LEN_SERVICE        32U
#define VCAN_LEN_DUMP_END       8U
#define VCAN_LEN_RESPONSE       16U
#define VCAN_MAX_LEN            64U

/* 명령 */
#define VCAN_CMD_PING           0x01U   /* 응답 : 버전, uptime                       */
#define VCAN_CMD_DUMP           0x02U   /* ECU / 흐름 / 이벤트 / 서비스 전체 즉시 전송 */
#define VCAN_CMD_RESET_STATS    0x03U   /* ETH 카운터 + ECU 테이블 초기화             */
#define VCAN_CMD_SET_STATUS_MS  0x04U   /* [1..2] u16 주기 ms (0 = 끔)               */
#define VCAN_CMD_SET_DUMP_MS    0x05U   /* [1..4] u32 주기 ms (0 = 끔)               */

/* 응답 결과 */
#define VCAN_RES_OK             0x00U
#define VCAN_RES_UNKNOWN_CMD    0x01U
#define VCAN_RES_BAD_ARG        0x02U

/* STATUS flags */
#define VCAN_ST_LINK_UP         0x01U
#define VCAN_ST_PHY_READY       0x02U
#define VCAN_ST_VLAN_ON         0x04U
#define VCAN_ST_SOMEIP_ON       0x08U
#define VCAN_ST_CNT_OVERFLOW    0x10U

typedef struct
{
  uint8_t  flags;
  uint8_t  phy_role;         /* 0 slave, 1 master, 0xFF 모름 */
  uint8_t  sqi;              /* 0~7, 0xFF 모름               */
  uint32_t uptime_ms;
  uint16_t ecu_count;
  uint16_t service_count;
  uint32_t rx_frames;
  uint32_t tx_frames;
  uint32_t rx_lost;
  uint32_t someip_messages;
  uint32_t ip;               /* 0xaabbccdd */
} vcan_status_t;

typedef struct
{
  uint32_t rx_frames, tx_frames;
  uint32_t rx_dma_missed, rx_fifo_overflow, rx_fifo_missed, rx_pool_empty;
  uint32_t rx_crc_errors, rx_align_errors, tx_errors;
  uint32_t mon_frames, mon_tagged, vlan_dropped;
} vcan_eth_stats_t;

typedef struct
{
  uint16_t service_id;
  uint16_t instance_id;
  uint8_t  major;
  uint8_t  l4_proto;         /* 0x06 TCP, 0x11 UDP, 0 = endpoint 없음 */
  uint32_t minor;
  uint32_t ttl;
  uint32_t ep_ip;            /* 0xaabbccdd */
  uint16_t ep_port;
  uint32_t from_ip;
  uint32_t offers;
} vcan_service_t;

typedef struct
{
  uint8_t cmd;
  uint8_t len;
  uint8_t arg[VCAN_MAX_LEN - 1U];
} vcan_command_t;

/* ---- CAN FD 길이 ---- */
uint8_t vcan_round_len(uint8_t len);      /* 다음 유효 길이 (0..8,12,16,20,24,32,48,64) */
uint8_t vcan_len_to_dlc(uint8_t len);     /* 유효 길이 → DLC 코드 0~15 */
uint8_t vcan_dlc_to_len(uint8_t dlc);     /* DLC 코드 → 바이트 수 */

/* ---- 생성 : buf 에 쓰고 프레임 길이를 돌려줌 ---- */
uint8_t vcan_encode_status(uint8_t *buf, const vcan_status_t *s);
uint8_t vcan_encode_eth_stats(uint8_t *buf, const vcan_eth_stats_t *s);
uint8_t vcan_encode_ecu(uint8_t *buf, const veh_ecu_t *e, uint8_t index, uint8_t total,
                        uint32_t now_us, uint32_t idle_us);
uint8_t vcan_encode_event(uint8_t *buf, uint32_t ecu_ip, const veh_event_t *ev);
uint8_t vcan_encode_flow(uint8_t *buf, uint32_t ecu_ip, const veh_flow_t *f);
uint8_t vcan_encode_service(uint8_t *buf, const vcan_service_t *s);
uint8_t vcan_encode_dump_end(uint8_t *buf, uint16_t records, uint16_t dropped);
uint8_t vcan_encode_response(uint8_t *buf, uint8_t cmd, uint8_t result,
                             uint32_t uptime_ms, uint32_t value);

/* ---- 명령 해석 : 0 = 성공 ---- */
int vcan_decode_command(const uint8_t *data, uint8_t len, vcan_command_t *out);

#endif /* VCAN_PROTO_H */
