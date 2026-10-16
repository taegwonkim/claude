/**
  ******************************************************************************
  * @file    veh_vlan.h
  * @brief   VLAN 태그 송수신 + 네트워크 모니터 (수신 프레임 통계)
  ******************************************************************************
  * lwIP 훅 (LWIP/Target/lwip_hooks.h 에서 연결)
  *   LWIP_HOOK_VLAN_SET   → veh_vlan_set_hook()   : 송신 태그
  *   LWIP_HOOK_VLAN_CHECK → veh_vlan_check_hook() : 수신 VLAN 필터
  * ethernetif.c 훅
  *   ethernetif_rx_hook()        : 모든 수신 프레임 통계 + 태그 없는 프레임 필터
  *   ethernetif_post_init_hook() : MAC 필터 (멀티캐스트 / promiscuous)
  ******************************************************************************
  */

#ifndef VEH_VLAN_H
#define VEH_VLAN_H

#include <stdint.h>

void veh_vlan_init(void);
void veh_netmon_process(void);   /* 주기적 통계 출력 */
void veh_netmon_print(void);     /* 즉시 출력        */

typedef struct
{
  uint32_t rx_frames;
  uint32_t rx_tagged;
  uint32_t rx_vlan_dropped;      /* 다른 VLAN 이라 lwIP 로 넘기지 않은 수 */
  uint32_t rx_untagged_dropped;
} veh_vlan_counters_t;

const veh_vlan_counters_t *veh_vlan_counters(void);

#endif /* VEH_VLAN_H */
