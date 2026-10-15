/**
  ******************************************************************************
  * @file    lwip_hooks.h
  * @brief   lwIP 훅 함수 선언 (LWIP_HOOK_FILENAME 으로 lwIP 소스에 포함됨)
  *          훅 매크로 자체는 lwipopts.h 의 USER CODE 1 에 정의되어 있습니다.
  *          (PBUF_LINK_HLEN 이 VLAN 태그 4 바이트를 포함하도록 opt.h 가 보게 하려고)
  ******************************************************************************
  */

#ifndef LWIP_HOOKS_H
#define LWIP_HOOKS_H

#include "lwip/arch.h"

struct netif;
struct pbuf;
struct eth_addr;
struct eth_hdr;
struct eth_vlan_hdr;

/* 송신 : >= 0 이면 그 값(TCI)으로 802.1Q 태그 추가, -1 이면 태그 없음 */
s32_t veh_vlan_set_hook(struct netif *netif, struct pbuf *p,
                        const struct eth_addr *src, const struct eth_addr *dst,
                        u16_t eth_type);

/* 수신 : 태그 붙은 프레임을 lwIP 가 처리할지 (1 = 처리, 0 = 버림) */
int veh_vlan_check_hook(struct netif *netif, const struct eth_hdr *hdr,
                        const struct eth_vlan_hdr *vlan);

#endif /* LWIP_HOOKS_H */
