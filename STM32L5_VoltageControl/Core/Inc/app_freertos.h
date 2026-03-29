/**
 ******************************************************************************
 * @file    app_freertos.h
 * @brief   FreeRTOS Application Tasks, Queues, Semaphores, Mutexes
 ******************************************************************************
 */
#ifndef __APP_FREERTOS_H
#define __APP_FREERTOS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os2.h"
#include "app_types.h"

/* ── Task Handles ─────────────────────────────────────────────────────── */
extern osThreadId_t uartCmdTaskHandle;
extern osThreadId_t voltageCtrlTaskHandle;
extern osThreadId_t adcReadTaskHandle;
extern osThreadId_t faultDetectTaskHandle;
extern osThreadId_t commTxTaskHandle;

/* ── Queue Handles ────────────────────────────────────────────────────── */
extern osMessageQueueId_t uartCmdQueueHandle;
extern osMessageQueueId_t adcDataQueueHandle;
extern osMessageQueueId_t commTxQueueHandle;
extern osMessageQueueId_t faultEventQueueHandle;

/* ── Mutex Handles ────────────────────────────────────────────────────── */
extern osMutexId_t spiMutexHandle;
extern osMutexId_t dacMutexHandle;
extern osMutexId_t uartTxMutexHandle;

/* ── Semaphore Handles ────────────────────────────────────────────────── */
extern osSemaphoreId_t uartRxSemHandle;
extern osSemaphoreId_t adcConvSemHandle;

/* ── Shared Data ──────────────────────────────────────────────────────── */
extern ChannelCtrl_t g_channels[NUM_CHANNELS];

/* ── Task Entry Functions ─────────────────────────────────────────────── */
void StartUartCmdTask(void *argument);
void StartVoltageCtrlTask(void *argument);
void StartAdcReadTask(void *argument);
void StartFaultDetectTask(void *argument);
void StartCommTxTask(void *argument);

/* ── FreeRTOS Object Creation ─────────────────────────────────────────── */
void App_FreeRTOS_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_FREERTOS_H */
