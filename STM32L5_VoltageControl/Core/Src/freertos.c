/* =========================================================
 * freertos.c - FreeRTOS 태스크 / 큐 / 세마포어 구현
 *
 * STM32CubeMX가 생성하는 freertos.c를 기반으로 실제
 * 애플리케이션 코드를 통합한 완성본입니다.
 *
 * 태스크 구조:
 *   UartRxTask       (Priority 3) - UART 수신 + 명령 파싱
 *   UartTxTask       (Priority 2) - UART 송신 (큐에서 꺼내어 DMA 전송)
 *   AdcReadTask      (Priority 4) - MCP3465R SPI 읽기
 *   VoltageControlTask(Priority 5) - PI 피드백 제어 루프
 *   CurrentMonitorTask(Priority 4) - 전류 감시 + 폴트 처리
 * =========================================================*/

/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

#include "main.h"
#include "mcp3465r.h"
#include "voltage_control.h"
#include "uart_protocol.h"
#include "current_monitor.h"

#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* =========================================================
 * FreeRTOS 객체 핸들 (CubeMX GUI에서 생성하거나 직접 선언)
 * =========================================================*/

/* 태스크 핸들 */
osThreadId_t UartRxTaskHandle;
osThreadId_t UartTxTaskHandle;
osThreadId_t AdcReadTaskHandle;
osThreadId_t VoltageControlTaskHandle;
osThreadId_t CurrentMonitorTaskHandle;

/* 태스크 속성 */
const osThreadAttr_t UartRxTask_attributes = {
    .name       = "UartRxTask",
    .stack_size = 512 * 4,   /* 512 words = 2048 bytes */
    .priority   = (osPriority_t) osPriorityNormal,
};
const osThreadAttr_t UartTxTask_attributes = {
    .name       = "UartTxTask",
    .stack_size = 512 * 4,
    .priority   = (osPriority_t) osPriorityBelowNormal,
};
const osThreadAttr_t AdcReadTask_attributes = {
    .name       = "AdcReadTask",
    .stack_size = 512 * 4,
    .priority   = (osPriority_t) osPriorityAboveNormal,
};
const osThreadAttr_t VoltCtrlTask_attributes = {
    .name       = "VoltCtrlTask",
    .stack_size = 512 * 4,
    .priority   = (osPriority_t) osPriorityHigh,
};
const osThreadAttr_t CurrMonTask_attributes = {
    .name       = "CurrMonTask",
    .stack_size = 256 * 4,   /* 256 words = 1024 bytes */
    .priority   = (osPriority_t) osPriorityAboveNormal,
};

/* 메시지 큐 핸들 */
osMessageQueueId_t xCmdQueueHandle;     /* PC 명령 → VoltageControlTask */
osMessageQueueId_t xRespQueueHandle;    /* 응답 → UartTxTask */
osMessageQueueId_t xAdcResultQueueHandle; /* ADC 결과 공유 */

/* 큐 속성 */
const osMessageQueueAttr_t xCmdQueue_attributes = {
    .name = "xCmdQueue"
};
const osMessageQueueAttr_t xRespQueue_attributes = {
    .name = "xRespQueue"
};
const osMessageQueueAttr_t xAdcResultQueue_attributes = {
    .name = "xAdcResultQueue"
};

/* 뮤텍스 핸들 */
osMutexId_t xSpiMutexHandle;    /* SPI 버스 접근 보호 */
osMutexId_t xUartTxMutexHandle; /* UART TX 접근 보호 */

const osMutexAttr_t xSpiMutex_attributes = {
    .name = "xSpiMutex",
    .attr_bits = osMutexRecursive | osMutexPrioInherit,
};
const osMutexAttr_t xUartTxMutex_attributes = {
    .name = "xUartTxMutex",
    .attr_bits = osMutexPrioInherit,
};

/* 이진 세마포어 핸들 */
osSemaphoreId_t xAdcDataReadyHandle; /* ADC IRQ → AdcReadTask */
osSemaphoreId_t xUartRxReadyHandle;  /* UART DMA 완료 → UartRxTask */

const osSemaphoreAttr_t xAdcDataReady_attributes = {
    .name = "xAdcDataReady"
};
const osSemaphoreAttr_t xUartRxReady_attributes = {
    .name = "xUartRxReady"
};

/* =========================================================
 * 애플리케이션 전역 데이터
 * =========================================================*/
static MCP3465R_Handle_t  g_adc;
static VoltageCtrl_t      g_vctrl;
static CurrentMonitor_t   g_currmon;

/* UART DMA 수신 버퍼 */
#define UART_DMA_BUF_SIZE   256
static uint8_t  uart_rx_dma_buf[UART_DMA_BUF_SIZE];
static uint16_t uart_rx_prev_pos = 0;

/* =========================================================
 * MX_FREERTOS_Init - CubeMX가 호출하는 초기화 함수
 * main.c의 MX_FREERTOS_Init() 또는 StartDefaultTask에서 호출
 * =========================================================*/
void MX_FREERTOS_Init(void)
{
    /* ----- 뮤텍스 생성 ----- */
    xSpiMutexHandle    = osMutexNew(&xSpiMutex_attributes);
    xUartTxMutexHandle = osMutexNew(&xUartTxMutex_attributes);

    /* ----- 세마포어 생성 (초기값 0) ----- */
    xAdcDataReadyHandle = osSemaphoreNew(1, 0, &xAdcDataReady_attributes);
    xUartRxReadyHandle  = osSemaphoreNew(1, 0, &xUartRxReady_attributes);

    /* ----- 메시지 큐 생성 ----- */
    xCmdQueueHandle       = osMessageQueueNew(10, sizeof(CmdQueueItem_t),
                                               &xCmdQueue_attributes);
    xRespQueueHandle      = osMessageQueueNew(20, sizeof(RespQueueItem_t),
                                               &xRespQueue_attributes);
    xAdcResultQueueHandle = osMessageQueueNew(8, sizeof(MCP3465R_Result_t),
                                               &xAdcResultQueue_attributes);

    /* ----- 태스크 생성 ----- */
    UartRxTaskHandle = osThreadNew(vUartRxTask, NULL,
                                    &UartRxTask_attributes);
    UartTxTaskHandle = osThreadNew(vUartTxTask, NULL,
                                    &UartTxTask_attributes);
    AdcReadTaskHandle = osThreadNew(vAdcReadTask, NULL,
                                     &AdcReadTask_attributes);
    VoltageControlTaskHandle = osThreadNew(vVoltageControlTask, NULL,
                                            &VoltCtrlTask_attributes);
    CurrentMonitorTaskHandle = osThreadNew(vCurrentMonitorTask, NULL,
                                            &CurrMonTask_attributes);
}

/* =========================================================
 * TASK 1: vUartRxTask
 * UART 수신 + 명령 파싱 + 큐 전달
 *
 * DMA 원형 버퍼(Circular)로 수신하여 새 데이터만 파싱
 * =========================================================*/
void vUartRxTask(void *argument)
{
    /* DMA 원형 수신 시작 */
    HAL_UARTEx_ReceiveToIdle_DMA(PROTO_UART,
                                  uart_rx_dma_buf,
                                  UART_DMA_BUF_SIZE);

    /* 프로토콜 초기화 (버전/도움말 출력) */
    Proto_Init();

    static char line_buf[PROTO_CMD_MAX_LEN];
    static uint8_t line_idx = 0;

    for (;;) {
        /* UART 데이터 수신 대기 (세마포어 또는 타임아웃) */
        osSemaphoreAcquire(xUartRxReadyHandle, osWaitForever);

        /* DMA 버퍼에서 새 데이터 읽기 */
        uint16_t cur_pos = UART_DMA_BUF_SIZE
                         - __HAL_DMA_GET_COUNTER(PROTO_UART->hdmarx);

        uint16_t start = uart_rx_prev_pos;
        uint16_t end   = cur_pos;

        /* 원형 버퍼 처리 */
        while (start != end) {
            uint8_t c = uart_rx_dma_buf[start];
            start = (start + 1) % UART_DMA_BUF_SIZE;

            if (c == '\n' || c == '\r') {
                if (line_idx > 0) {
                    line_buf[line_idx] = '\0';

                    /* 명령 파싱 후 큐에 삽입 */
                    CmdQueueItem_t item;
                    strncpy(item.data, line_buf, PROTO_CMD_MAX_LEN - 1);
                    item.data[PROTO_CMD_MAX_LEN - 1] = '\0';
                    item.len = line_idx;

                    osMessageQueuePut(xCmdQueueHandle, &item, 0, 0);
                    line_idx = 0;
                }
            } else if (line_idx < PROTO_CMD_MAX_LEN - 1) {
                line_buf[line_idx++] = (char)c;
            }
        }

        uart_rx_prev_pos = cur_pos;
    }
}

/* =========================================================
 * TASK 2: vUartTxTask
 * 응답 큐에서 꺼내어 UART DMA 전송
 * =========================================================*/
void vUartTxTask(void *argument)
{
    static RespQueueItem_t resp;
    static uint8_t tx_buf[PROTO_TX_BUF_SIZE];

    for (;;) {
        /* 큐에서 응답 대기 */
        if (osMessageQueueGet(xRespQueueHandle, &resp, NULL, osWaitForever) == osOK) {
            /* 뮤텍스로 TX 보호 */
            osMutexAcquire(xUartTxMutexHandle, osWaitForever);

            memcpy(tx_buf, resp.data, resp.len);

            /* DMA 전송 (블로킹하지 않음, HAL이 완료 콜백으로 처리) */
            HAL_UART_Transmit_DMA(PROTO_UART, tx_buf, resp.len);

            /* 전송 완료 대기 (단순하게 HAL 상태 폴링) */
            uint32_t tick = osKernelGetTickCount();
            while (PROTO_UART->gState == HAL_UART_STATE_BUSY_TX) {
                if ((osKernelGetTickCount() - tick) > 100) break; /* 100ms 타임아웃 */
                osDelay(1);
            }

            osMutexRelease(xUartTxMutexHandle);
        }
    }
}

/* =========================================================
 * TASK 3: vAdcReadTask
 * MCP3465R 전체 채널 순차 읽기
 * ADC IRQ 세마포어 또는 주기적으로 실행
 * =========================================================*/
void vAdcReadTask(void *argument)
{
    /* SPI 뮤텍스 획득 후 MCP3465R 초기화 */
    osMutexAcquire(xSpiMutexHandle, osWaitForever);
    HAL_StatusTypeDef init_ret = MCP3465R_Init(&g_adc);
    osMutexRelease(xSpiMutexHandle);

    if (init_ret != HAL_OK) {
        Proto_SendEvent("ERROR", "MCP3465R init failed");
        /* 초기화 실패 시 주기적 재시도 */
        for (;;) {
            osDelay(5000);
            osMutexAcquire(xSpiMutexHandle, osWaitForever);
            init_ret = MCP3465R_Init(&g_adc);
            osMutexRelease(xSpiMutexHandle);
            if (init_ret == HAL_OK) {
                Proto_SendEvent("INFO", "MCP3465R init OK (retry)");
                break;
            }
        }
    } else {
        Proto_SendEvent("INFO", "MCP3465R init OK");
    }

    /* ADC 읽기 주기: 20ms (50Hz 샘플링) */
    const uint32_t ADC_PERIOD_MS = 20;
    uint32_t last_wake = osKernelGetTickCount();

    for (;;) {
        /* IRQ 세마포어 대기 (타임아웃 = ADC 주기 × 2) */
        /* SCAN 모드이므로 매 채널마다 IRQ 발생 */
        /* 타임아웃으로 폴링도 지원 */
        osSemaphoreAcquire(xAdcDataReadyHandle, ADC_PERIOD_MS * 2);

        /* SPI 뮤텍스 획득 */
        if (osMutexAcquire(xSpiMutexHandle, 50) == osOK) {
            /* 전체 채널 읽기 */
            MCP3465R_ReadAllChannels(&g_adc);
            osMutexRelease(xSpiMutexHandle);
        }

        /* 주기 맞추기 */
        osDelayUntil(&last_wake, ADC_PERIOD_MS);
        last_wake = osKernelGetTickCount();
    }
}

/* =========================================================
 * TASK 4: vVoltageControlTask
 * PI 피드백 제어 루프 + PC 명령 처리
 * 우선순위 가장 높음 → 실시간 제어 보장
 * =========================================================*/
void vVoltageControlTask(void *argument)
{
    /* 전압 제어기 초기화 */
    VoltageCtrl_Init(&g_vctrl);

    /* 시스템 전체 활성화 */
    VoltageCtrl_EnableAll(&g_vctrl, true);

    Proto_SendEvent("INFO", "VoltageControl started");

    /* 제어 주기 설정 */
    const uint32_t CTRL_PERIOD = CTRL_PERIOD_MS; /* 10ms */
    uint32_t last_wake = osKernelGetTickCount();

    static CmdQueueItem_t cmd_item;
    static ParsedCmd_t    parsed_cmd;

    for (;;) {
        /* ----- 명령 큐 처리 (비블로킹, 있으면 즉시 처리) ----- */
        while (osMessageQueueGet(xCmdQueueHandle, &cmd_item, NULL, 0) == osOK) {

            if (Proto_ParseCmd(cmd_item.data, &parsed_cmd)) {
                switch (parsed_cmd.type) {

                    case CMD_SET:
                        VoltageCtrl_SetTarget(&g_vctrl,
                                               parsed_cmd.channel,
                                               parsed_cmd.value);
                        Proto_SendOKf("CH%d target=%ldmV",
                                      parsed_cmd.channel + 1,
                                      (long)parsed_cmd.value);
                        break;

                    case CMD_GET:
                        Proto_SendChannelInfo(&g_vctrl, parsed_cmd.channel);
                        break;

                    case CMD_STATUS:
                        Proto_SendStatus(&g_vctrl);
                        break;

                    case CMD_ENABLE:
                        VoltageCtrl_Enable(&g_vctrl,
                                            parsed_cmd.channel,
                                            (bool)parsed_cmd.value);
                        Proto_SendOKf("CH%d %s",
                                      parsed_cmd.channel + 1,
                                      parsed_cmd.value ? "enabled" : "disabled");
                        break;

                    case CMD_RESET:
                        if (parsed_cmd.all_channels) {
                            for (int i = 0; i < VOLTAGE_NUM_CH; i++) {
                                VoltageCtrl_Reset(&g_vctrl, i);
                                g_currmon.ch[i].fault = FAULT_NONE;
                                g_currmon.ch[i].fault_confirm = 0;
                            }
                            Proto_SendOK("ALL channels reset");
                        } else {
                            VoltageCtrl_Reset(&g_vctrl, parsed_cmd.channel);
                            g_currmon.ch[parsed_cmd.channel].fault = FAULT_NONE;
                            Proto_SendOKf("CH%d reset", parsed_cmd.channel + 1);
                        }
                        break;

                    case CMD_HELP:
                        Proto_SendHelp();
                        break;

                    case CMD_VERSION:
                        Proto_SendVersion();
                        break;

                    default:
                        Proto_SendError(ERR_SYNTAX, cmd_item.data);
                        break;
                }
            } else {
                Proto_SendError(ERR_SYNTAX, cmd_item.data);
            }
        }

        /* ----- PI 제어 업데이트 ----- */
        VoltageCtrl_Update(&g_vctrl, &g_adc);

        /* ----- 주기적 상태 출력 (5초마다) ----- */
        static uint32_t status_counter = 0;
        if (++status_counter >= (5000 / CTRL_PERIOD)) {
            status_counter = 0;
            /* 자동 상태 출력 (원하면 주석 해제) */
            /* Proto_SendStatus(&g_vctrl); */
        }

        /* 정확한 주기 제어 */
        osDelayUntil(&last_wake, CTRL_PERIOD);
        last_wake = osKernelGetTickCount();
    }
}

/* =========================================================
 * TASK 5: vCurrentMonitorTask
 * 전류 감시 + 단락/단선 감지
 * =========================================================*/
void vCurrentMonitorTask(void *argument)
{
    /* 전류 모니터 초기화 */
    CurrMon_Init(&g_currmon);

    Proto_SendEvent("INFO", "CurrentMonitor started");

    /* 모니터링 주기: 20ms */
    const uint32_t MON_PERIOD_MS = 20;
    uint32_t last_wake = osKernelGetTickCount();

    for (;;) {
        /* 전류 측정 + 폴트 감지 + 처리 */
        CurrMon_Update(&g_currmon, &g_adc, &g_vctrl);

        osDelayUntil(&last_wake, MON_PERIOD_MS);
        last_wake = osKernelGetTickCount();
    }
}

/* =========================================================
 * HAL 콜백 함수들 (ISR에서 호출)
 * main.c 또는 stm32l5xx_it.c에 위치해도 됨
 * =========================================================*/

/**
 * @brief UART RX 완료 / Idle 감지 콜백 (DMA)
 *        HAL_UARTEx_RxEventCallback 사용 (ReceiveToIdle_DMA)
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        /* UartRxTask 세마포어 해제 (ISR → 태스크 알림) */
        xSemaphoreGiveFromISR(xUartRxReadyHandle, &xHigherPriorityTaskWoken);

        /* DMA 재시작 */
        HAL_UARTEx_ReceiveToIdle_DMA(huart,
                                      uart_rx_dma_buf,
                                      UART_DMA_BUF_SIZE);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
 * @brief GPIO EXTI 콜백 (MCP3465R IRQ 핀 - PB7)
 *        데이터 준비 알림 → AdcReadTask 세마포어 해제
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == MCP3465R_IRQ_PIN) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(xAdcDataReadyHandle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* =========================================================
 * FreeRTOS 훅 함수들
 * =========================================================*/

/**
 * @brief 스택 오버플로우 감지 훅 (configCHECK_FOR_STACK_OVERFLOW = 2)
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    /* 스택 오버플로우 발생 시 시스템 안전 정지 */
    /* 실제 제품에서는 watchdog 리셋 또는 에러 로깅 */
    __disable_irq();

    /* 디버그: 어떤 태스크에서 발생했는지 LED 또는 UART로 표시 */
    /* (이 시점에서 UART는 신뢰할 수 없으므로 LED 권장) */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET); /* 에러 LED */

    while (1) { __NOP(); }
}

/**
 * @brief 메모리 할당 실패 훅 (configUSE_MALLOC_FAILED_HOOK = 1)
 */
void vApplicationMallocFailedHook(void)
{
    /* Heap 부족 → 시스템 정지 */
    __disable_irq();
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);
    while (1) { __NOP(); }
}

/**
 * @brief Idle 훅 (configUSE_IDLE_HOOK = 0이면 불필요)
 *        저전력 모드 진입 가능
 */
void vApplicationIdleHook(void)
{
    /* __WFI(); */ /* 슬립 모드 (필요 시 활성화) */
}
