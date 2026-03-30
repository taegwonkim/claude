/**
 ******************************************************************************
 * @file    app_freertos.c
 * @brief   FreeRTOS Application - Task/Queue/Mutex/Semaphore Creation
 ******************************************************************************
 */
#include "app_freertos.h"
#include "app_config.h"

/* ── Task Handles ─────────────────────────────────────────────────────── */
osThreadId_t uartCmdTaskHandle;
osThreadId_t voltageCtrlTaskHandle;
osThreadId_t adcReadTaskHandle;
osThreadId_t faultDetectTaskHandle;
osThreadId_t commTxTaskHandle;

/* ── Queue Handles ────────────────────────────────────────────────────── */
osMessageQueueId_t uartCmdQueueHandle;
osMessageQueueId_t adcDataQueueHandle;
osMessageQueueId_t commTxQueueHandle;
osMessageQueueId_t faultEventQueueHandle;

/* ── Mutex Handles ────────────────────────────────────────────────────── */
osMutexId_t spiMutexHandle;
osMutexId_t dacMutexHandle;
osMutexId_t uartTxMutexHandle;

/* ── Semaphore Handles ────────────────────────────────────────────────── */
osSemaphoreId_t uartRxSemHandle;
osSemaphoreId_t adcConvSemHandle;

/* ── Shared Data ──────────────────────────────────────────────────────── */
ChannelCtrl_t g_channels[NUM_CHANNELS];

/* ── Task Attributes ──────────────────────────────────────────────────── */
static const osThreadAttr_t uartCmdTask_attr = {
    .name = "UartCmdTask",
    .stack_size = 512 * 4,   /* 2048 bytes */
    .priority = osPriorityHigh,
};

static const osThreadAttr_t voltageCtrlTask_attr = {
    .name = "VoltCtrlTask",
    .stack_size = 512 * 4,
    .priority = osPriorityAboveNormal,
};

static const osThreadAttr_t adcReadTask_attr = {
    .name = "AdcReadTask",
    .stack_size = 512 * 4,
    .priority = osPriorityNormal,
};

static const osThreadAttr_t faultDetectTask_attr = {
    .name = "FaultDetTask",
    .stack_size = 384 * 4,
    .priority = osPriorityNormal,
};

static const osThreadAttr_t commTxTask_attr = {
    .name = "CommTxTask",
    .stack_size = 512 * 4,
    .priority = osPriorityBelowNormal,
};

/* ── Mutex Attributes ─────────────────────────────────────────────────── */
static const osMutexAttr_t spiMutex_attr = {
    .name = "SpiMutex",
    .attr_bits = osMutexPrioInherit,  /* Priority inheritance to prevent inversion */
};

static const osMutexAttr_t dacMutex_attr = {
    .name = "DacMutex",
    .attr_bits = osMutexPrioInherit,
};

static const osMutexAttr_t uartTxMutex_attr = {
    .name = "UartTxMutex",
    .attr_bits = osMutexPrioInherit,
};

/* ── FreeRTOS Initialization ──────────────────────────────────────────── */

void App_FreeRTOS_Init(void)
{
    /* ── Create Mutexes ───────────────────────────────────────────────── */
    spiMutexHandle = osMutexNew(&spiMutex_attr);
    dacMutexHandle = osMutexNew(&dacMutex_attr);
    uartTxMutexHandle = osMutexNew(&uartTxMutex_attr);

    /* ── Create Semaphores ────────────────────────────────────────────── */
    uartRxSemHandle = osSemaphoreNew(1, 0, NULL);   /* Binary, initially empty */
    adcConvSemHandle = osSemaphoreNew(1, 0, NULL);

    /* ── Create Queues ────────────────────────────────────────────────── */
    uartCmdQueueHandle = osMessageQueueNew(UART_CMD_QUEUE_SIZE,
                                           sizeof(UartCmd_t), NULL);
    adcDataQueueHandle = osMessageQueueNew(ADC_DATA_QUEUE_SIZE,
                                           sizeof(AdcData_t), NULL);
    commTxQueueHandle = osMessageQueueNew(COMM_TX_QUEUE_SIZE,
                                          sizeof(CommMsg_t), NULL);
    faultEventQueueHandle = osMessageQueueNew(FAULT_EVENT_QUEUE_SIZE,
                                              sizeof(FaultEvent_t), NULL);

    /* ── Create Tasks ─────────────────────────────────────────────────── */
    uartCmdTaskHandle = osThreadNew(StartUartCmdTask, NULL, &uartCmdTask_attr);
    voltageCtrlTaskHandle = osThreadNew(StartVoltageCtrlTask, NULL, &voltageCtrlTask_attr);
    adcReadTaskHandle = osThreadNew(StartAdcReadTask, NULL, &adcReadTask_attr);
    faultDetectTaskHandle = osThreadNew(StartFaultDetectTask, NULL, &faultDetectTask_attr);
    commTxTaskHandle = osThreadNew(StartCommTxTask, NULL, &commTxTask_attr);
}
