/**
  ******************************************************************************
  * @file    ethernetif.h
  * @brief   lwIP <-> STM32H7 ETH(RMII) <-> LAN8770 인터페이스
  ******************************************************************************
  */

#ifndef ETHERNETIF_H
#define ETHERNETIF_H

#include "lwip/err.h"
#include "lwip/netif.h"
#include "lan8770.h"
#include "main.h"

err_t ethernetif_init(struct netif *netif);
void  ethernetif_input(struct netif *netif);
void  ethernet_link_check_state(struct netif *netif);

u32_t sys_jiffies(void);
u32_t sys_now(void);

/* 애플리케이션에서 PHY 상태(SQI, 역할 등)를 읽을 때 사용 */
extern lan8770_Object_t LAN8770;
uint8_t ethernetif_phy_ready(void);

/* 확장 훅 (기본 구현은 ethernetif.c 의 __weak, 차량 기능은 veh_vlan.c 에서 대체) */
uint8_t ethernetif_rx_hook(struct pbuf *p);
void    ethernetif_post_init_hook(ETH_HandleTypeDef *ethHandle);

#endif /* ETHERNETIF_H */
