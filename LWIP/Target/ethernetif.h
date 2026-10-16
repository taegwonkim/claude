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

/* 송수신 / 손실 통계 */
typedef struct
{
  uint32_t rx_frames;          /* lwIP 로 넘긴 프레임                          */
  uint32_t tx_frames;
  uint32_t rx_dma_missed;      /* 빈 Rx 디스크립터가 없어 DMA 가 버림 (DMACMFCR) */
  uint32_t rx_fifo_overflow;   /* Rx FIFO 넘침 (MTLRQMPOCR)                     */
  uint32_t rx_fifo_missed;     /* Rx FIFO 에서 버림 (MTLRQMPOCR)                */
  uint32_t rx_pool_empty;      /* lwIP Rx 버퍼 풀이 빈 횟수                     */
  uint32_t rx_crc_errors;      /* CRC 오류 (배선/노이즈)                        */
  uint32_t rx_align_errors;
  uint32_t tx_errors;
  uint8_t  counter_overflow;   /* 하드웨어 카운터가 넘친 적 있음 → 값은 최소치  */
} ethernetif_stats_t;

void                      ethernetif_poll_hw_counters(void);
const ethernetif_stats_t *ethernetif_stats(void);
uint32_t                  ethernetif_stats_rx_lost(void);   /* dma_missed + fifo_* */
void                      ethernetif_stats_reset(void);

/* 확장 훅 (기본 구현은 ethernetif.c 의 __weak, 차량 기능은 veh_vlan.c 에서 대체) */
uint8_t ethernetif_rx_hook(struct pbuf *p);
void    ethernetif_post_init_hook(ETH_HandleTypeDef *ethHandle);

#endif /* ETHERNETIF_H */
