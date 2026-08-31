/**
  ******************************************************************************
  * @file    app_types.h
  * @brief   SurgeDetector 공용 타입/전역 핸들
  ******************************************************************************
  */
#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cmsis_os2.h"
#include "app_cfg.h"

/* ------------------------------------------------------------------
 * 결과 코드
 * ----------------------------------------------------------------*/
typedef enum
{
  SD_OK        = 0,
  SD_ERR       = -1,
  SD_ERR_PARAM = -2,
  SD_ERR_TMO   = -3,
  SD_ERR_CRC   = -4,
  SD_ERR_BUSY  = -5,
  SD_ERR_HW    = -6
} sd_res_t;

/* ------------------------------------------------------------------
 * FPGA 1회 취득 결과
 * ----------------------------------------------------------------*/
typedef struct
{
  uint32_t tick_ms;                 /* MCU 수신 시각 */
  uint16_t seq;                     /* FPGA 시퀀스 */
  uint16_t ch[SD_ADC_CH_NUM];       /* 6채널 피크값 */
  uint8_t  status;                  /* FPGA 상태 바이트 */
  uint8_t  rsv[3];
} sd_sample_t;                      /* 24 bytes */

/* ------------------------------------------------------------------
 * 출력 큐 항목 (ASCII 라인)
 * ----------------------------------------------------------------*/
typedef struct
{
  uint16_t len;
  char     data[SD_LINE_MAX];
} sd_line_t;

/* ------------------------------------------------------------------
 * 이벤트 플래그
 * ----------------------------------------------------------------*/
#define SD_EVT_WIFI_UP        (1u << 0)
#define SD_EVT_TCP_UP         (1u << 1)
#define SD_EVT_FPGA_RUN       (1u << 2)
#define SD_EVT_CFG_DIRTY      (1u << 3)
#define SD_EVT_WIFI_RECONF    (1u << 4)

/* ------------------------------------------------------------------
 * 런타임 통계
 * ----------------------------------------------------------------*/
typedef struct
{
  uint32_t trig_cnt;        /* EXTI 트리거 수 */
  uint32_t rx_frame;        /* 정상 수신 프레임 */
  uint32_t rx_crcerr;       /* CRC 오류 */
  uint32_t rx_timeout;      /* 트리거 후 프레임 미도착 */
  uint32_t tx_uart;         /* RS485 송신 라인 수 */
  uint32_t tx_usb;          /* USB 송신 라인 수 */
  uint32_t tx_wifi;         /* WiFi 송신 라인 수 */
  uint32_t drop_sample;     /* qSample overflow */
  uint32_t drop_wifi;       /* qWifiTx overflow */
  uint32_t drop_usb;        /* qUsbTx overflow */
  uint32_t wifi_reconn;     /* WiFi 재연결 횟수 */
} sd_stat_t;

/* ------------------------------------------------------------------
 * 전역 커널 오브젝트 (app_main.c 에서 정의)
 * ----------------------------------------------------------------*/
extern osMessageQueueId_t g_qSample;
extern osMessageQueueId_t g_qWifiTx;
extern osMessageQueueId_t g_qUsbTx;
extern osMessageQueueId_t g_qUsbRx;

extern osSemaphoreId_t    g_semTrig;

extern osMutexId_t        g_mtxFlash;
extern osMutexId_t        g_mtxCfg;
extern osMutexId_t        g_mtxAt;

extern osEventFlagsId_t   g_evtSys;

extern osThreadId_t       g_thFpga;
extern osThreadId_t       g_thRouter;
extern osThreadId_t       g_thWifi;
extern osThreadId_t       g_thCliUart;
extern osThreadId_t       g_thCliUsb;
extern osThreadId_t       g_thUsbTx;

extern sd_stat_t          g_stat;

/* 큐가 가득 차면 가장 오래된 항목을 버리고 넣는다 (drop-oldest) */
bool sd_queue_put_overwrite(osMessageQueueId_t q, const void *item, uint32_t *drop_cnt);

#endif /* APP_TYPES_H */
