/**
  ******************************************************************************
  * @file    veh_frame.h
  * @brief   Ethernet / 802.1Q 헤더 해석 (하드웨어 독립, PC 에서 단위 시험 가능)
  ******************************************************************************
  */

#ifndef VEH_FRAME_H
#define VEH_FRAME_H

#include <stdint.h>

#define VEH_ETHTYPE_IPV4      0x0800U
#define VEH_ETHTYPE_ARP       0x0806U
#define VEH_ETHTYPE_VLAN      0x8100U   /* 802.1Q C-tag          */
#define VEH_ETHTYPE_QINQ      0x88A8U   /* 802.1ad S-tag         */
#define VEH_ETHTYPE_IPV6      0x86DDU
#define VEH_ETHTYPE_AVTP      0x22F0U   /* IEEE 1722 (AVB)       */
#define VEH_ETHTYPE_GPTP      0x88F7U   /* IEEE 802.1AS / 1588   */
#define VEH_ETHTYPE_LLDP      0x88CCU
#define VEH_ETHTYPE_MACSEC    0x88E5U

#define VEH_VLAN_NONE         0xFFFFU   /* 태그 없는 프레임      */

typedef struct
{
  uint8_t  dst[6];
  uint8_t  src[6];
  uint8_t  tagged;       /* 1 = 802.1Q/802.1ad 태그 있음             */
  uint8_t  pcp;          /* Priority Code Point 0~7                 */
  uint8_t  dei;          /* Drop Eligible Indicator                 */
  uint16_t vid;          /* VLAN ID, 태그 없으면 VEH_VLAN_NONE      */
  uint16_t ethertype;    /* 태그 뒤(안쪽) EtherType                  */
  uint16_t header_len;   /* 14 (태그 없음) / 18 (태그 1개)           */
} veh_frame_info_t;

/* 0 = 성공, -1 = 너무 짧음 */
int veh_frame_parse(const uint8_t *frame, uint32_t len, veh_frame_info_t *out);

/* TCI = PCP(3) | DEI(1) | VID(12) */
uint16_t veh_vlan_tci(uint8_t pcp, uint16_t vid);

const char *veh_ethertype_name(uint16_t ethertype);

#endif /* VEH_FRAME_H */
