/**
  ******************************************************************************
  * @file           : freertos.c
  * @brief          : FreeRTOS task definitions and initialization
  *
  * Task Architecture:
  *   ┌─────────────────────────────────────────────────────────────────┐
  *   │  UartRxTask (Priority 5)                                       │
  *   │  - UART1 수신 대기 (IDLE Line DMA)                              │
  *   │  - 명령 파싱 → CmdQueue 전송                                    │
  *   │  - GET 명령 시 즉시 응답                                        │
  *   └──────────────────────────┬──────────────────────────────────────┘
  *                              │ CmdQueue
  *   ┌──────────────────────────▼──────────────────────────────────────┐
  *   │  VoltCtrlTask (Priority 6 - Highest)                           │
  *   │  - CmdQueue에서 목표 전압 수신                                   │
  *   │  - 10ms 주기로 PI 피드백 제어 실행                               │
  *   │  - ADC(SPI1) 읽기 → PI 계산 → DAC(SPI2) 출력                    │
  *   └────────────────────────────────────────────────────────────────┘
  *
  *   ┌────────────────────────────────────────────────────────────────┐
  *   │  CurrMonTask (Priority 4)                                      │
  *   │  - 50ms 주기로 각 채널 전류 측정 (ADC CH1)                       │
  *   │  - 단락(과전류) / 단선(무전류) 감지                              │
  *   │  - 이상 감지 시 VoltageControl에 Fault 설정                     │
  *   └────────────────────────────────────────────────────────────────┘
  *
  *   ┌────────────────────────────────────────────────────────────────┐
  *   │  ReportTask (Priority 3)                                       │
  *   │  - 500ms 주기로 4채널 데이터 수집                                │
  *   │  - UART1 + FDCAN1으로 보고서 전송                               │
  *   └────────────────────────────────────────────────────────────────┘
  *
  *   ┌────────────────────────────────────────────────────────────────┐
  *   │  DebugTask (Priority 2 - Lowest)                               │
  *   │  - DebugQueue에서 메시지 수신                                   │
  *   │  - UART4로 디버그 정보 출력                                     │
  *   └────────────────────────────────────────────────────────────────┘
  ******************************************************************************
  */

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "main.h"
#include "ad5641.h"
#include "mcp3465r.h"
#include "voltage_control.h"
#include "comm_protocol.h"
#include <string.h>
#include <stdio.h>

/* -------------------------------------------------------------------------- */
/*                           External Handles                                 */
/* -------------------------------------------------------------------------- */
extern SPI_HandleTypeDef  hspi1;
extern SPI_HandleTypeDef  hspi2;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart4;
extern FDCAN_HandleTypeDef hfdcan1;

/* -------------------------------------------------------------------------- */
/*                          RTOS Object Definitions                           */
/* -------------------------------------------------------------------------- */

/* --- Task Handles --- */
osThreadId_t uartRxTaskHandle;
osThreadId_t voltCtrlTaskHandle;
osThreadId_t currMonTaskHandle;
osThreadId_t reportTaskHandle;
osThreadId_t debugTaskHandle;

/* --- Task Attributes --- */
static const osThreadAttr_t uartRxTask_attr = {
    .name       = "UartRxTask",
    .stack_size = 512 * 4,   /* 2048 bytes */
    .priority   = osPriorityAboveNormal,  /* 5 */
};

static const osThreadAttr_t voltCtrlTask_attr = {
    .name       = "VoltCtrlTask",
    .stack_size = 512 * 4,
    .priority   = osPriorityHigh,         /* 6 */
};

static const osThreadAttr_t currMonTask_attr = {
    .name       = "CurrMonTask",
    .stack_size = 384 * 4,   /* 1536 bytes */
    .priority   = osPriorityNormal,       /* 4 */
};

static const osThreadAttr_t reportTask_attr = {
    .name       = "ReportTask",
    .stack_size = 512 * 4,
    .priority   = osPriorityBelowNormal,  /* 3 */
};

static const osThreadAttr_t debugTask_attr = {
    .name       = "DebugTask",
    .stack_size = 256 * 4,   /* 1024 bytes */
    .priority   = osPriorityLow,          /* 2 */
};

/* --- Queue Handles --- */
osMessageQueueId_t cmdQueueHandle;
osMessageQueueId_t debugQueueHandle;

/* --- Mutex Handles --- */
osMutexId_t spi1MutexHandle;
osMutexId_t spi2MutexHandle;
osMutexId_t uart1MutexHandle;

/* --- Mutex Attributes (with priority inheritance) --- */
static const osMutexAttr_t spi1Mutex_attr = {
    .name      = "SPI1_Mutex",
    .attr_bits = osMutexPrioInherit,
};

static const osMutexAttr_t spi2Mutex_attr = {
    .name      = "SPI2_Mutex",
    .attr_bits = osMutexPrioInherit,
};

static const osMutexAttr_t uart1Mutex_attr = {
    .name      = "UART1_Mutex",
    .attr_bits = osMutexPrioInherit,
};

/* --- Semaphore Handles --- */
osSemaphoreId_t uart1RxSemHandle;

/* -------------------------------------------------------------------------- */
/*                          UART DMA RX Variables                             */
/* -------------------------------------------------------------------------- */
#define UART1_DMA_BUF_SIZE  128
static uint8_t uart1_dma_buf[UART1_DMA_BUF_SIZE];
static uint8_t uart1_rx_line[UART1_DMA_BUF_SIZE];
static volatile uint16_t uart1_rx_len = 0;

/* -------------------------------------------------------------------------- */
/*                          Task Function Prototypes                          */
/* -------------------------------------------------------------------------- */
static void StartUartRxTask(void *argument);
static void StartVoltCtrlTask(void *argument);
static void StartCurrMonTask(void *argument);
static void StartReportTask(void *argument);
static void StartDebugTask(void *argument);

/* -------------------------------------------------------------------------- */
/*                          FreeRTOS Initialization                           */
/* -------------------------------------------------------------------------- */

/**
  * @brief  FreeRTOS initialization - called from main() before osKernelStart()
  */
void MX_FREERTOS_Init(void)
{
    /* Create Mutexes */
    spi1MutexHandle  = osMutexNew(&spi1Mutex_attr);
    spi2MutexHandle  = osMutexNew(&spi2Mutex_attr);
    uart1MutexHandle = osMutexNew(&uart1Mutex_attr);

    /* Create Binary Semaphore for UART1 RX */
    uart1RxSemHandle = osSemaphoreNew(1, 0, NULL);

    /* Create Message Queues */
    cmdQueueHandle   = osMessageQueueNew(8, sizeof(VoltageCmd_t), NULL);
    debugQueueHandle = osMessageQueueNew(8, sizeof(DebugMsg_t), NULL);

    /* Initialize modules */
    Comm_Init();
    VoltageControl_Init();
    AD5641_Init(NULL, &hspi2, spi2MutexHandle);
    MCP3465R_Init(NULL, &hspi1, spi1MutexHandle);

    /* Create Tasks */
    uartRxTaskHandle   = osThreadNew(StartUartRxTask,   NULL, &uartRxTask_attr);
    voltCtrlTaskHandle = osThreadNew(StartVoltCtrlTask, NULL, &voltCtrlTask_attr);
    currMonTaskHandle  = osThreadNew(StartCurrMonTask,  NULL, &currMonTask_attr);
    reportTaskHandle   = osThreadNew(StartReportTask,   NULL, &reportTask_attr);
    debugTaskHandle    = osThreadNew(StartDebugTask,    NULL, &debugTask_attr);
}

/* -------------------------------------------------------------------------- */
/*                          UART1 IDLE Line Callback                          */
/* -------------------------------------------------------------------------- */

/**
  * @brief  Called from UART IDLE line interrupt handler
  *         Detects end of received line and signals UartRxTask
  */
void UART1_IDLE_Callback(void)
{
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE)) {
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);

        /* Stop DMA and get received count */
        HAL_UART_DMAStop(&huart1);

        uint16_t dma_remaining = (uint16_t)__HAL_DMA_GET_COUNTER(huart1.hdmarx);
        uart1_rx_len = UART1_DMA_BUF_SIZE - dma_remaining;

        if (uart1_rx_len > 0 && uart1_rx_len < UART1_DMA_BUF_SIZE) {
            memcpy(uart1_rx_line, uart1_dma_buf, uart1_rx_len);
            uart1_rx_line[uart1_rx_len] = '\0';

            /* Signal the UART RX task */
            osSemaphoreRelease(uart1RxSemHandle);
        }

        /* Restart DMA reception */
        HAL_UART_Receive_DMA(&huart1, uart1_dma_buf, UART1_DMA_BUF_SIZE);
    }
}

/* -------------------------------------------------------------------------- */
/*                     Task 1: UART Command Receiver                          */
/* -------------------------------------------------------------------------- */

/**
  * @brief  UART RX Task - receives and parses PC commands
  *         Priority: AboveNormal (5)
  *
  *  - Waits for UART1 IDLE line interrupt (via semaphore)
  *  - Parses received command string
  *  - SET command → sends to CmdQueue for VoltCtrlTask
  *  - GET command → sends immediate response
  */
static void StartUartRxTask(void *argument)
{
    (void)argument;
    VoltageCmd_t cmd;
    ChannelData_t ch_data;

    Comm_SendDebug("[INIT] UartRxTask started\r\n");

    /* Start DMA reception with IDLE line detection */
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    HAL_UART_Receive_DMA(&huart1, uart1_dma_buf, UART1_DMA_BUF_SIZE);

    for (;;) {
        /* Wait for data received signal */
        if (osSemaphoreAcquire(uart1RxSemHandle, osWaitForever) == osOK) {

            /* Parse the received line */
            if (Comm_ParseCommand((const char *)uart1_rx_line, &cmd)) {

                if (cmd.voltage < 0.0f) {
                    /* GET command */
                    if (cmd.channel == 0xFF) {
                        /* GET ALL - send all channel data */
                        for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
                            VoltageControl_GetChannelData(i, &ch_data);
                            Comm_SendChannelResponse(i, &ch_data);
                        }
                    } else {
                        /* GET single channel */
                        VoltageControl_GetChannelData(cmd.channel, &ch_data);
                        Comm_SendChannelResponse(cmd.channel, &ch_data);
                    }
                } else {
                    /* SET command - forward to voltage control task */
                    osMessageQueuePut(cmdQueueHandle, &cmd, 0, 10);
                    Comm_SendDebug("[UART] SET CH%d = %.3fV\r\n",
                                  cmd.channel, cmd.voltage);
                }
            } else {
                /* Invalid command - send error response */
                const char *err_msg = "ERR: Invalid command. Use: SET <ch> <voltage> or GET <ch|ALL>\r\n";
                osMutexAcquire(uart1MutexHandle, osWaitForever);
                HAL_UART_Transmit(&huart1, (uint8_t *)err_msg, strlen(err_msg), 100);
                osMutexRelease(uart1MutexHandle);
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/*                  Task 2: Voltage Feedback Control (PI)                     */
/* -------------------------------------------------------------------------- */

/**
  * @brief  Voltage Control Task - PI feedback loop
  *         Priority: High (6) - highest user task
  *
  *  10ms 주기:
  *   1. CmdQueue에서 새 목표 전압 확인 (non-blocking)
  *   2. 각 채널 ADC(CH0) 전압 측정
  *   3. PI 제어기 계산
  *   4. DAC 출력 조정
  */
static void StartVoltCtrlTask(void *argument)
{
    (void)argument;
    VoltageCmd_t cmd;
    TickType_t last_wake_time;

    Comm_SendDebug("[INIT] VoltCtrlTask started\r\n");

    /* Wait a bit for other init to complete */
    osDelay(100);

    last_wake_time = xTaskGetTickCount();

    for (;;) {
        /* Check for new voltage commands (non-blocking) */
        while (osMessageQueueGet(cmdQueueHandle, &cmd, NULL, 0) == osOK) {
            VoltageControl_SetTarget(cmd.channel, cmd.voltage);
        }

        /* Run PI feedback control for all 4 channels */
        VoltageControl_UpdateAll();

        /* Wait for next period (10ms = 100Hz control loop) */
        osDelayUntil(last_wake_time + FEEDBACK_INTERVAL_MS);
        last_wake_time += FEEDBACK_INTERVAL_MS;
    }
}

/* -------------------------------------------------------------------------- */
/*              Task 3: Current Monitor (Short/Open Detection)                */
/* -------------------------------------------------------------------------- */

/**
  * @brief  Current Monitor Task - fault detection
  *         Priority: Normal (4)
  *
  *  50ms 주기:
  *   1. 각 채널의 ADC CH1(전류 센싱) 읽기
  *   2. 전류값으로 단락/단선 판별
  *     - 단락(Short): 전류 > CURRENT_SHORT_THRESHOLD
  *     - 단선(Open): 전류 < CURRENT_OPEN_THRESHOLD (전압 > 0.5V일 때)
  *   3. 이상 감지 시 채널 상태 업데이트 및 경고
  */
static void StartCurrMonTask(void *argument)
{
    (void)argument;
    int32_t raw_current;
    float   current_ma;
    ChannelData_t ch_data;
    TickType_t last_wake_time;

    Comm_SendDebug("[INIT] CurrMonTask started\r\n");

    osDelay(200);  /* Wait for voltage control to stabilize */

    last_wake_time = xTaskGetTickCount();

    for (;;) {
        for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {

            /* Read current from ADC CH1 */
            if (MCP3465R_ReadChannel(ch, MCP3465R_CH_CURRENT, &raw_current) == HAL_OK) {
                current_ma = MCP3465R_RawToCurrent(raw_current);

                /* Get current channel data for status check */
                VoltageControl_GetChannelData(ch, &ch_data);

                /* Update current reading */
                ch_data.current_ma = current_ma;

                /* --- Fault Detection Logic --- */
                ChannelStatus_t new_status = CH_STATUS_OK;

                /* Short circuit detection */
                if (current_ma > (CURRENT_SHORT_THRESHOLD * 1000.0f)) {
                    new_status = CH_STATUS_SHORT;
                    Comm_SendDebug("[FAULT] CH%d SHORT detected! I=%.1fmA\r\n",
                                  ch, current_ma);

                    /* Turn on error LED */
                    HAL_GPIO_WritePin(LED_ERROR_GPIO_Port, LED_ERROR_Pin, GPIO_PIN_SET);
                }
                /* Open circuit detection (only when voltage is being applied) */
                else if (ch_data.target_voltage > VOLTAGE_OPEN_CHECK_MIN &&
                         current_ma < (CURRENT_OPEN_THRESHOLD * 1000.0f)) {
                    new_status = CH_STATUS_OPEN;
                    Comm_SendDebug("[FAULT] CH%d OPEN detected! I=%.3fmA\r\n",
                                  ch, current_ma);
                }

                /* Update fault status in voltage controller */
                VoltageControl_SetFault(ch, new_status);

                /* If fault cleared, turn off error LED (check all channels) */
                if (new_status == CH_STATUS_OK) {
                    uint8_t any_fault = 0;
                    ChannelData_t tmp;
                    for (uint8_t j = 0; j < NUM_CHANNELS; j++) {
                        VoltageControl_GetChannelData(j, &tmp);
                        if (tmp.status != CH_STATUS_OK) {
                            any_fault = 1;
                            break;
                        }
                    }
                    if (!any_fault) {
                        HAL_GPIO_WritePin(LED_ERROR_GPIO_Port, LED_ERROR_Pin, GPIO_PIN_RESET);
                    }
                }
            }
        }

        /* Wait for next period (50ms = 20Hz) */
        osDelayUntil(last_wake_time + CURRENT_MON_INTERVAL_MS);
        last_wake_time += CURRENT_MON_INTERVAL_MS;
    }
}

/* -------------------------------------------------------------------------- */
/*                 Task 4: Periodic Report (UART1 + FDCAN1)                   */
/* -------------------------------------------------------------------------- */

/**
  * @brief  Report Task - periodic data transmission
  *         Priority: BelowNormal (3)
  *
  *  500ms 주기:
  *   1. 4채널 전압/전류/상태 데이터 수집
  *   2. UART1으로 ASCII 보고서 전송
  *   3. FDCAN1으로 바이너리 보고서 전송
  */
static void StartReportTask(void *argument)
{
    (void)argument;
    ReportData_t report;
    TickType_t last_wake_time;

    Comm_SendDebug("[INIT] ReportTask started\r\n");

    osDelay(500);  /* Wait for system to stabilize */

    last_wake_time = xTaskGetTickCount();

    for (;;) {
        /* Collect all channel data */
        VoltageControl_GetReportData(&report);

        /* Toggle status LED to show system is alive */
        HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);

        /* Send report via UART1 */
        Comm_SendReportUART(&report);

        /* Send report via FDCAN1 */
        Comm_SendReportFDCAN(&report);

        /* Wait for next report interval (500ms) */
        osDelayUntil(last_wake_time + REPORT_INTERVAL_MS);
        last_wake_time += REPORT_INTERVAL_MS;
    }
}

/* -------------------------------------------------------------------------- */
/*                     Task 5: Debug Output (UART4)                           */
/* -------------------------------------------------------------------------- */

/**
  * @brief  Debug Task - outputs debug messages via UART4
  *         Priority: Low (2)
  *
  *  - DebugQueue에서 메시지 대기
  *  - UART4로 디버그 정보 출력
  */
static void StartDebugTask(void *argument)
{
    (void)argument;
    DebugMsg_t dbg_msg;

    /* Startup message */
    const char *startup = "\r\n=== Voltage Feedback Current Monitor ===\r\n"
                          "MCU: STM32L552R | FreeRTOS | 4CH\r\n"
                          "Commands: SET <ch> <voltage> | GET <ch|ALL>\r\n"
                          "=========================================\r\n";
    HAL_UART_Transmit(&huart4, (uint8_t *)startup, strlen(startup), 200);

    for (;;) {
        /* Wait for debug messages */
        if (osMessageQueueGet(debugQueueHandle, &dbg_msg, NULL, osWaitForever) == osOK) {
            HAL_UART_Transmit(&huart4, (uint8_t *)dbg_msg.msg,
                              strlen(dbg_msg.msg), 100);
        }
    }
}

/* -------------------------------------------------------------------------- */
/*                       FreeRTOS Callback Functions                          */
/* -------------------------------------------------------------------------- */

/**
  * @brief  Stack overflow hook - called when a task stack overflow is detected
  */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    char buf[64];
    snprintf(buf, sizeof(buf), "[FATAL] Stack overflow in task: %s\r\n", pcTaskName);
    HAL_UART_Transmit(&huart4, (uint8_t *)buf, strlen(buf), 100);

    /* Turn on error LED and halt */
    HAL_GPIO_WritePin(LED_ERROR_GPIO_Port, LED_ERROR_Pin, GPIO_PIN_SET);

    for (;;) {
        /* Halt - watchdog will eventually reset if enabled */
    }
}

/**
  * @brief  Malloc failed hook
  */
void vApplicationMallocFailedHook(void)
{
    const char *msg = "[FATAL] FreeRTOS malloc failed!\r\n";
    HAL_UART_Transmit(&huart4, (uint8_t *)msg, strlen(msg), 100);

    HAL_GPIO_WritePin(LED_ERROR_GPIO_Port, LED_ERROR_Pin, GPIO_PIN_SET);

    for (;;) {
        /* Halt */
    }
}
