/**
  ******************************************************************************
  * @file    app_fdcan.h
  * @brief   PC 링크 : FDCAN1 (CAN FD) 로 상태 / 손실 카운터 / ECU 테이블 전송, 명령 수신
  ******************************************************************************
  */

#ifndef APP_FDCAN_H
#define APP_FDCAN_H

#include <stdint.h>

typedef struct
{
  uint32_t tx_frames;
  uint32_t tx_dropped;      /* Tx FIFO 가득 참 (PC 어댑터 미연결 / ACK 없음 등) */
  uint32_t rx_commands;
  uint32_t bus_off;         /* Bus-Off 복구 횟수 */
  uint8_t  started;
} app_fdcan_counters_t;

void APP_FDCAN_Init(void);
void APP_FDCAN_Process(void);
void APP_FDCAN_StartDump(void);
const app_fdcan_counters_t *APP_FDCAN_Counters(void);

#endif /* APP_FDCAN_H */
