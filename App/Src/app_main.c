/**
  ******************************************************************************
  * @file    app_main.c
  * @brief   커널 오브젝트/태스크 생성 및 하트비트
  ******************************************************************************
  */
#include "app_main.h"
#include "uart_link.h"
#include "cfg_store.h"
#include "w25q40.h"
#include "fpga_link.h"
#include "wifi_task.h"
#include "data_router.h"
#include "usb_bridge.h"
#include "cli.h"
#include "main.h"

/* ---------------------------------------------------------------------------
 * 전역 커널 오브젝트
 * -------------------------------------------------------------------------*/
osMessageQueueId_t g_qSample;
osMessageQueueId_t g_qWifiTx;
osMessageQueueId_t g_qUsbTx;
osMessageQueueId_t g_qUsbRx;

osSemaphoreId_t    g_semTrig;

osMutexId_t        g_mtxFlash;
osMutexId_t        g_mtxCfg;
osMutexId_t        g_mtxAt;

osEventFlagsId_t   g_evtSys;

osThreadId_t       g_thFpga;
osThreadId_t       g_thRouter;
osThreadId_t       g_thWifi;
osThreadId_t       g_thCliUart;
osThreadId_t       g_thCliUsb;
osThreadId_t       g_thUsbTx;

sd_stat_t          g_stat;

/* ---------------------------------------------------------------------------
 * 큐 헬퍼 : 가득 차면 가장 오래된 항목을 버리고 넣는다
 * -------------------------------------------------------------------------*/
bool sd_queue_put_overwrite(osMessageQueueId_t q, const void *item, uint32_t *drop_cnt)
{
  uint8_t scratch[sizeof(sd_line_t)];

  if (q == NULL) { return false; }

  if (osMessageQueuePut(q, item, 0u, 0u) == osOK)
  {
    return true;
  }

  /* 한 칸 비우고 재시도 */
  if (osMessageQueueGet(q, scratch, NULL, 0u) == osOK)
  {
    if (drop_cnt != NULL) { (*drop_cnt)++; }
  }

  return (osMessageQueuePut(q, item, 0u, 0u) == osOK);
}

/* ---------------------------------------------------------------------------
 * 커널 오브젝트 생성
 * -------------------------------------------------------------------------*/
static const osMutexAttr_t k_mtx_flash = { "mtxFlash", osMutexPrioInherit, NULL, 0U };
static const osMutexAttr_t k_mtx_cfg   = { "mtxCfg",   osMutexPrioInherit, NULL, 0U };
static const osMutexAttr_t k_mtx_at    = { "mtxAt",    osMutexPrioInherit, NULL, 0U };

static const osSemaphoreAttr_t k_sem_trig = { "semTrig", 0U, NULL, 0U };
static const osEventFlagsAttr_t k_evt_sys = { "evtSys",  0U, NULL, 0U };

static const osMessageQueueAttr_t k_q_sample = { "qSample", 0U, NULL, 0U, NULL, 0U };
static const osMessageQueueAttr_t k_q_wifitx = { "qWifiTx", 0U, NULL, 0U, NULL, 0U };
static const osMessageQueueAttr_t k_q_usbtx  = { "qUsbTx",  0U, NULL, 0U, NULL, 0U };
static const osMessageQueueAttr_t k_q_usbrx  = { "qUsbRx",  0U, NULL, 0U, NULL, 0U };

sd_res_t App_PreKernelInit(void)
{
  memset(&g_stat, 0, sizeof(g_stat));

  g_mtxFlash = osMutexNew(&k_mtx_flash);
  g_mtxCfg   = osMutexNew(&k_mtx_cfg);
  g_mtxAt    = osMutexNew(&k_mtx_at);

  /* 트리거는 카운팅 세마포어 : 연속 트리거가 몰려도 최대 4개까지 유지 */
  g_semTrig  = osSemaphoreNew(4u, 0u, &k_sem_trig);

  g_evtSys   = osEventFlagsNew(&k_evt_sys);

  g_qSample  = osMessageQueueNew(SD_Q_SAMPLE_LEN, sizeof(sd_sample_t), &k_q_sample);
  g_qWifiTx  = osMessageQueueNew(SD_Q_WIFITX_LEN, sizeof(sd_line_t),   &k_q_wifitx);
  g_qUsbTx   = osMessageQueueNew(SD_Q_USBTX_LEN,  sizeof(sd_line_t),   &k_q_usbtx);
  g_qUsbRx   = osMessageQueueNew(SD_Q_USBRX_LEN,  sizeof(uint8_t),     &k_q_usbrx);

  if ((g_mtxFlash == NULL) || (g_mtxCfg == NULL) || (g_mtxAt == NULL) ||
      (g_semTrig == NULL)  || (g_evtSys == NULL) ||
      (g_qSample == NULL)  || (g_qWifiTx == NULL) ||
      (g_qUsbTx == NULL)   || (g_qUsbRx == NULL))
  {
    return SD_ERR;
  }

  /* 기본 설정으로 먼저 채워 둔다 (플래시 적재는 App_Main 에서) */
  cfgstore_defaults(&g_cfg);

  return SD_OK;
}

/* ---------------------------------------------------------------------------
 * 태스크 속성
 * -------------------------------------------------------------------------*/
static const osThreadAttr_t k_th_fpga = {
  .name = "tskFpga",   .stack_size = SD_STK_FPGA * 4u,     .priority = osPriorityRealtime
};
static const osThreadAttr_t k_th_router = {
  .name = "tskRouter", .stack_size = SD_STK_ROUTER * 4u,   .priority = osPriorityAboveNormal
};
static const osThreadAttr_t k_th_wifi = {
  .name = "tskWifi",   .stack_size = SD_STK_WIFI * 4u,     .priority = osPriorityNormal
};
static const osThreadAttr_t k_th_cli_uart = {
  .name = "tskCliU",   .stack_size = SD_STK_CLI_UART * 4u, .priority = osPriorityBelowNormal
};
static const osThreadAttr_t k_th_cli_usb = {
  .name = "tskCliB",   .stack_size = SD_STK_CLI_USB * 4u,  .priority = osPriorityBelowNormal
};
static const osThreadAttr_t k_th_usbtx = {
  .name = "tskUsbTx",  .stack_size = SD_STK_USBTX * 4u,    .priority = osPriorityLow
};

/* ---------------------------------------------------------------------------
 * defaultTask 본문
 * -------------------------------------------------------------------------*/
void App_Main(void)
{
  uint32_t tick;

  /* 1) UART 링크(DMA 수신) 시작 — 스케줄러가 돌기 시작한 뒤에 해야 안전 */
  if (uartlink_init_all() != SD_OK)
  {
    HAL_GPIO_WritePin(LED_ERR_GPIO_Port, LED_ERR_Pin, GPIO_PIN_SET);
  }

  /* 2) 외부 플래시에서 설정 적재 (실패 시 기본값으로 계속 동작) */
  if (cfgstore_init() != SD_OK)
  {
    HAL_GPIO_WritePin(LED_ERR_GPIO_Port, LED_ERR_Pin, GPIO_PIN_SET);
  }

  /* 3) 태스크 생성 */
  g_thFpga    = osThreadNew(fpga_task,     NULL, &k_th_fpga);
  g_thRouter  = osThreadNew(router_task,   NULL, &k_th_router);
  g_thWifi    = osThreadNew(wifi_task,     NULL, &k_th_wifi);
  g_thCliUart = osThreadNew(cli_uart_task, NULL, &k_th_cli_uart);
  g_thCliUsb  = osThreadNew(cli_usb_task,  NULL, &k_th_cli_usb);
  g_thUsbTx   = osThreadNew(usb_tx_task,   NULL, &k_th_usbtx);

  if ((g_thFpga == NULL) || (g_thRouter == NULL) || (g_thWifi == NULL) ||
      (g_thCliUart == NULL) || (g_thCliUsb == NULL) || (g_thUsbTx == NULL))
  {
    HAL_GPIO_WritePin(LED_ERR_GPIO_Port, LED_ERR_Pin, GPIO_PIN_SET);
  }

  /* 4) 하트비트 루프 */
  tick = osKernelGetTickCount();
  for (;;)
  {
    tick += SD_HEARTBEAT_MS;
    osDelayUntil(tick);

    HAL_GPIO_TogglePin(LED_RUN_GPIO_Port, LED_RUN_Pin);

#if SD_USE_IWDG
    extern IWDG_HandleTypeDef hiwdg;
    (void)HAL_IWDG_Refresh(&hiwdg);
#endif
  }
}
