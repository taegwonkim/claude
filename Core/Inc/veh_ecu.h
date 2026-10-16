/**
  ******************************************************************************
  * @file    veh_ecu.h
  * @brief   ECU 테이블 : 송신자 IP 별로 MAC / VLAN / 흐름 / SOME/IP 이벤트 / 주기 집계
  ******************************************************************************
  * 하드웨어 독립 (시간은 호출하는 쪽이 µs 로 넘김) → PC 에서 단위 시험 가능
  *
  *   veh_ecu_on_frame()  : 모든 IPv4 수신 프레임 (veh_vlan.c 의 rx 훅에서)
  *   veh_ecu_on_someip() : SOME/IP 메시지마다 (app_someip.c 에서)
  *   veh_ecu_on_offer()  : SD OfferService 마다 (app_someip.c 에서)
  ******************************************************************************
  */

#ifndef VEH_ECU_H
#define VEH_ECU_H

#include <stdint.h>
#include "veh_frame.h"
#include "vehicle_net_conf.h"

/* 송신 간격 통계 [µs] */
typedef struct
{
  uint32_t count;
  uint32_t last_us;
  uint32_t avg_us;     /* 지수 이동 평균 (1/8) */
  uint32_t min_us;
  uint32_t max_us;
} veh_interval_t;

typedef struct
{
  uint8_t        proto;
  uint16_t       sport;
  uint16_t       dport;
  veh_interval_t iv;
} veh_flow_t;

typedef struct
{
  uint16_t       service_id;
  uint16_t       method_id;
  uint8_t        msg_type;
  uint32_t       last_len;
  uint8_t        last_payload[APP_ECU_PAYLOAD_BYTES];
  veh_interval_t iv;
} veh_event_t;

typedef struct
{
  uint16_t service_id;
  uint16_t instance_id;
  uint8_t  major;
} veh_offer_t;

typedef struct
{
  uint8_t        used;
  uint32_t       ip;                 /* a.b.c.d → 0xaabbccdd */
  uint8_t        mac[6];
  uint16_t       vid;                /* VEH_VLAN_NONE = 태그 없음 */
  uint8_t        mac_changed;        /* 같은 IP 가 다른 MAC 으로 보임 (게이트웨이 경유 등) */
  uint32_t       packets;
  uint32_t       bytes;
  uint32_t       first_us;
  uint32_t       last_us;
  veh_flow_t     flows[APP_ECU_MAX_FLOWS];
  uint32_t       flow_overflow;
  veh_event_t    events[APP_ECU_MAX_EVENTS];
  uint32_t       event_overflow;
  veh_offer_t    offers[APP_ECU_MAX_OFFERS];
  uint8_t        num_offers;
} veh_ecu_t;

void veh_ecu_reset(void);

/* IPv4 프레임 1 개 */
void veh_ecu_on_frame(const veh_frame_info_t *fi, const veh_ipv4_info_t *ip, uint32_t now_us);

/* SOME/IP 메시지 1 개 (src_ip 는 0xaabbccdd 형식) */
void veh_ecu_on_someip(uint32_t src_ip, uint16_t service_id, uint16_t method_id,
                       uint8_t msg_type, const uint8_t *payload, uint32_t len,
                       uint32_t now_us);

/* SD OfferService 1 개 */
void veh_ecu_on_offer(uint32_t src_ip, uint16_t service_id, uint16_t instance_id, uint8_t major,
                      uint32_t now_us);

uint32_t         veh_ecu_count(void);
const veh_ecu_t *veh_ecu_get(uint32_t index);        /* index 번째 사용 중인 ECU */
const veh_ecu_t *veh_ecu_find(uint32_t ip);
uint32_t         veh_ecu_dropped(void);              /* 테이블이 가득 차서 못 넣은 IP 수 */

/* ECU 1 개를 자세히 출력 */
void veh_ecu_print(const veh_ecu_t *e, uint32_t now_us, uint32_t idle_us);
/* 다음 ECU 1 개 출력 (돌아가며). 출력했으면 1 */
int  veh_ecu_print_next(uint32_t now_us, uint32_t idle_us);
/* 한 줄 요약 목록 */
void veh_ecu_print_summary(uint32_t now_us, uint32_t idle_us);

#endif /* VEH_ECU_H */
