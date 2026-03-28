/**
 * @file    main.c
 * @brief   STM32H5 FreeRTOS 4채널 DAC/ADC 전압 제어 시스템
 *          메인 파일: 하드웨어 초기화 + FreeRTOS 태스크
 *
 * ============================================================
 * FreeRTOS 태스크 구성
 * ============================================================
 *
 *  ┌─────────────────────────────────────────────────────────┐
 *  │  우선순위   태스크명           주기      설명            │
 *  ├─────────────────────────────────────────────────────────┤
 *  │  최고(4)   vTaskFaultMonitor   1ms      폴트 감지/차단  │
 *  │  4-1(3)    vTaskUartRx         이벤트   명령 처리       │
 *  │  4-2(2)    vTaskControl        10ms     PID 제어 루프   │
 *  │  4-3(1)    vTaskAdcSample      5ms      ADC 측정        │
 *  │  4-4(0)    vTaskUartTx         100ms    텔레메트리 전송 │
 *  └─────────────────────────────────────────────────────────┘
 *
 * ============================================================
 * 데이터 흐름
 * ============================================================
 *
 *  PC ──UART──> [vTaskUartRx]
 *                    │ xCmdQueue (Command_t)
 *                    ▼
 *             [vTaskControl] <── ADC 측정값 (공유 채널 구조체)
 *                    │               ▲
 *                    │          [vTaskAdcSample]
 *                    │               ▲
 *                    │          ADC DMA 완료 세마포어
 *                    │
 *                    ├──DAC 출력──> CH1~CH4 전압 출력
 *                    │
 *                    ▼ xTxQueue (TxMessage_t)
 *             [vTaskUartTx] ──UART──> PC
 *
 *  [vTaskFaultMonitor] ─ 모든 채널 과전류/오픈서킷 감시
 *                         └─ 폴트 발생 시 ALERT 즉시 전송
 *
 * ============================================================
 * STM32H563ZI 클럭 설정 (250MHz)
 * ============================================================
 *  HSE  : 8MHz (Nucleo 기판 X3)
 *  PLL1 : M=2, N=125, P=2 -> 250MHz (SYSCLK)
 *  AHB  : /1  = 250MHz
 *  APB1 : /2  = 125MHz (USART3)
 *  APB2 : /2  = 125MHz
 *  ADC  : PLL2 or AHB/4
 */

#include "main.h"
#include "channel_ctrl.h"
#include "dac_drv.h"
#include "adc_drv.h"
#include "uart_protocol.h"
#include <stdio.h>
#include <string.h>

/* =========================================================
 * 주변장치 핸들 정의
 * ========================================================= */
DAC_HandleTypeDef   hdac1;
SPI_HandleTypeDef   hspi2;
ADC_HandleTypeDef   hadc1;
UART_HandleTypeDef  huart3;
DMA_HandleTypeDef   hdma_adc1;

/* =========================================================
 * FreeRTOS 오브젝트 핸들 정의
 * ========================================================= */
QueueHandle_t       xCmdQueue;      /* 명령 큐:  깊이 CMD_QUEUE_SIZE */
QueueHandle_t       xTxQueue;       /* 전송 큐:  깊이 UART_TX_QUEUE_SIZE */
SemaphoreHandle_t   xAdcSemaphore;  /* ADC DMA 완료 이진 세마포어 */
SemaphoreHandle_t   xChannelMutex;  /* 채널 구조체 뮤텍스 */

/* =========================================================
 * 태스크 핸들
 * ========================================================= */
static TaskHandle_t xFaultTask;
static TaskHandle_t xUartRxTask;
static TaskHandle_t xControlTask;
static TaskHandle_t xAdcTask;
static TaskHandle_t xUartTxTask;

/* =========================================================
 * 전방 선언
 * ========================================================= */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_DAC1_Init(void);
static void MX_SPI2_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART3_UART_Init(void);

/* FreeRTOS 태스크 함수 */
static void vTaskFaultMonitor(void *pvParameters);
static void vTaskUartRx(void *pvParameters);
static void vTaskControl(void *pvParameters);
static void vTaskAdcSample(void *pvParameters);
static void vTaskUartTx(void *pvParameters);

/* =========================================================
 * main()
 * ========================================================= */
int main(void)
{
    /* HAL 초기화 */
    HAL_Init();

    /* 클럭 설정 (250MHz) */
    SystemClock_Config();

    /* GPIO 초기화 */
    MX_GPIO_Init();

    /* DMA 초기화 (ADC DMA보다 먼저) */
    MX_DMA_Init();

    /* 주변장치 초기화 */
    MX_DAC1_Init();
    MX_SPI2_Init();
    MX_ADC1_Init();
    MX_USART3_UART_Init();

    /* -------------------------------------------------------
     * FreeRTOS 오브젝트 생성
     * ----------------------------------------------------- */
    xCmdQueue     = xQueueCreate(8U,  sizeof(Command_t));
    xTxQueue      = xQueueCreate(32U, sizeof(TxMessage_t));
    xAdcSemaphore = xSemaphoreCreateBinary();
    xChannelMutex = xSemaphoreCreateMutex();

    if (xCmdQueue == NULL || xTxQueue == NULL ||
        xAdcSemaphore == NULL || xChannelMutex == NULL) {
        Error_Handler();
    }

    /* -------------------------------------------------------
     * 애플리케이션 초기화
     * ----------------------------------------------------- */
    DAC_Drv_Init();
    ADC_Drv_Init();
    Channel_Init();
    UartProto_Init();

    /* ADC DMA 시작 */
    ADC_Drv_StartDMA();

    /* -------------------------------------------------------
     * FreeRTOS 태스크 생성
     * ----------------------------------------------------- */
    xTaskCreate(vTaskFaultMonitor, "Fault",
                STACK_FAULT, NULL, TASK_PRI_FAULT, &xFaultTask);

    xTaskCreate(vTaskUartRx, "UartRx",
                STACK_UART_RX, NULL, TASK_PRI_UART_RX, &xUartRxTask);

    xTaskCreate(vTaskControl, "Control",
                STACK_CONTROL, NULL, TASK_PRI_CONTROL, &xControlTask);

    xTaskCreate(vTaskAdcSample, "ADC",
                STACK_ADC, NULL, TASK_PRI_ADC_SAMPLE, &xAdcTask);

    xTaskCreate(vTaskUartTx, "UartTx",
                STACK_UART_TX, NULL, TASK_PRI_UART_TX, &xUartTxTask);

    /* -------------------------------------------------------
     * 스케줄러 시작 (이후 main() 반환하지 않음)
     * ----------------------------------------------------- */
    vTaskStartScheduler();

    /* 여기에 도달하면 힙 부족 */
    Error_Handler();
    return 0;
}

/* =========================================================
 * ┌────────────────────────────────────────────────────┐
 * │  태스크 1: vTaskFaultMonitor                        │
 * │  우선순위: 최고   주기: 1ms                         │
 * │  역할: 모든 채널의 폴트 조건을 실시간 감시          │
 * │        - 과전류: i_meas > i_limit → 즉시 차단       │
 * │        - 오픈서킷: 설정전압 인가 중 전류 없음       │
 * └────────────────────────────────────────────────────┘
 * ========================================================= */
static void vTaskFaultMonitor(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        /* 모든 채널 폴트 검사 */
        for (uint8_t ch = 0U; ch < NUM_CHANNELS; ch++) {
            Channel_CheckFaults(ch);
        }

        /* LED: 하나라도 폴트이면 빨간 LED 점등 */
        bool any_fault = false;
        for (uint8_t ch = 0U; ch < NUM_CHANNELS; ch++) {
            if (g_channels[ch].status != CH_STATUS_DISABLED &&
                g_channels[ch].status != CH_STATUS_ENABLED) {
                any_fault = true;
                break;
            }
        }
        HAL_GPIO_WritePin(LED_RED_GPIO, LED_RED_PIN,
                          any_fault ? GPIO_PIN_SET : GPIO_PIN_RESET);

        vTaskDelayUntil(&xLastWakeTime,
                        pdMS_TO_TICKS(FAULT_CHECK_PERIOD_MS));
    }
}

/* =========================================================
 * ┌────────────────────────────────────────────────────┐
 * │  태스크 2: vTaskUartRx                              │
 * │  우선순위: 높음   트리거: xCmdQueue 이벤트          │
 * │  역할: PC 명령 큐에서 꺼내 채널 제어 함수 호출      │
 * └────────────────────────────────────────────────────┘
 * ========================================================= */
static void vTaskUartRx(void *pvParameters)
{
    (void)pvParameters;
    Command_t cmd;

    for (;;) {
        /* 명령 큐 대기 (블로킹) */
        if (xQueueReceive(xCmdQueue, &cmd, portMAX_DELAY) == pdPASS) {
            uint8_t idx = (cmd.channel > 0U) ? (cmd.channel - 1U) : 0U;

            switch (cmd.type) {

                /* ---- 전압 설정 ---- */
                case CMD_SET_VOLTAGE:
                    if (cmd.channel >= 1U && cmd.channel <= NUM_CHANNELS) {
                        Channel_SetVoltage(idx, cmd.value);
                        UartProto_SendOk();
                    } else {
                        UartProto_SendError("INVALID_CH");
                    }
                    break;

                /* ---- 단일 채널 상태 조회 ---- */
                case CMD_GET_STATUS:
                    if (cmd.channel >= 1U && cmd.channel <= NUM_CHANNELS) {
                        UartProto_SendChannelData(idx);
                    } else {
                        UartProto_SendError("INVALID_CH");
                    }
                    break;

                /* ---- 전체 채널 조회 ---- */
                case CMD_GET_ALL:
                    UartProto_SendAllData();
                    break;

                /* ---- 채널 활성화 ---- */
                case CMD_ENABLE:
                    if (cmd.channel >= 1U && cmd.channel <= NUM_CHANNELS) {
                        Channel_Enable(idx);
                        UartProto_SendOk();
                    } else {
                        UartProto_SendError("INVALID_CH");
                    }
                    break;

                /* ---- 채널 비활성화 ---- */
                case CMD_DISABLE:
                    if (cmd.channel >= 1U && cmd.channel <= NUM_CHANNELS) {
                        Channel_Disable(idx);
                        UartProto_SendOk();
                    } else {
                        UartProto_SendError("INVALID_CH");
                    }
                    break;

                /* ---- PID Kp 설정 ---- */
                case CMD_SET_KP:
                    if (cmd.channel >= 1U && cmd.channel <= NUM_CHANNELS) {
                        xSemaphoreTake(xChannelMutex, portMAX_DELAY);
                        g_channels[idx].pid.kp = cmd.value;
                        xSemaphoreGive(xChannelMutex);
                        UartProto_SendOk();
                    }
                    break;

                /* ---- PID Ki 설정 ---- */
                case CMD_SET_KI:
                    if (cmd.channel >= 1U && cmd.channel <= NUM_CHANNELS) {
                        xSemaphoreTake(xChannelMutex, portMAX_DELAY);
                        g_channels[idx].pid.ki = cmd.value;
                        xSemaphoreGive(xChannelMutex);
                        UartProto_SendOk();
                    }
                    break;

                /* ---- PID Kd 설정 ---- */
                case CMD_SET_KD:
                    if (cmd.channel >= 1U && cmd.channel <= NUM_CHANNELS) {
                        xSemaphoreTake(xChannelMutex, portMAX_DELAY);
                        g_channels[idx].pid.kd = cmd.value;
                        xSemaphoreGive(xChannelMutex);
                        UartProto_SendOk();
                    }
                    break;

                /* ---- 전류 제한 설정 ---- */
                case CMD_SET_ILIMIT:
                    if (cmd.channel >= 1U && cmd.channel <= NUM_CHANNELS) {
                        xSemaphoreTake(xChannelMutex, portMAX_DELAY);
                        g_channels[idx].i_limit_ma = cmd.value;
                        xSemaphoreGive(xChannelMutex);
                        UartProto_SendOk();
                    }
                    break;

                default:
                    UartProto_SendError("UNHANDLED");
                    break;
            }
        }
    }
}

/* =========================================================
 * ┌────────────────────────────────────────────────────┐
 * │  태스크 3: vTaskAdcSample                           │
 * │  우선순위: 보통   주기: ADC DMA 완료 이벤트         │
 * │  역할: ADC DMA 완료 세마포어 대기 → 측정값 업데이트 │
 * └────────────────────────────────────────────────────┘
 * ========================================================= */
static void vTaskAdcSample(void *pvParameters)
{
    (void)pvParameters;

    for (;;) {
        /* ADC DMA 변환 완료 대기 (ISR에서 세마포어 Give) */
        if (xSemaphoreTake(xAdcSemaphore,
                           pdMS_TO_TICKS(ADC_SAMPLE_PERIOD_MS * 2U))
            == pdPASS)
        {
            /* 각 채널 측정값 업데이트 */
            for (uint8_t ch = 0U; ch < NUM_CHANNELS; ch++) {
                float v_mv = ADC_Drv_GetVoltageMv(ch);
                float i_ma = ADC_Drv_GetCurrentMa(ch);
                Channel_UpdateMeasurements(ch, v_mv, i_ma);
            }
        }
        /* 타임아웃 시 ADC 재시작 */
        else {
            ADC_Drv_StartDMA();
        }
    }
}

/* =========================================================
 * ┌────────────────────────────────────────────────────┐
 * │  태스크 4: vTaskControl                             │
 * │  우선순위: 높음   주기: 10ms                        │
 * │  역할: 모든 채널 PID 제어 루프 실행                 │
 * │        부하 변동에 따라 DAC 출력 자동 조절          │
 * └────────────────────────────────────────────────────┘
 * ========================================================= */
static void vTaskControl(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        /* 각 채널 PID 제어 실행 */
        for (uint8_t ch = 0U; ch < NUM_CHANNELS; ch++) {
            Channel_RunControl(ch);
        }

        /* 초록 LED 하트비트 토글 (500ms) */
        static uint32_t led_cnt = 0U;
        if (++led_cnt >= (500U / CONTROL_PERIOD_MS)) {
            HAL_GPIO_TogglePin(LED_GREEN_GPIO, LED_GREEN_PIN);
            led_cnt = 0U;
        }

        vTaskDelayUntil(&xLastWakeTime,
                        pdMS_TO_TICKS(CONTROL_PERIOD_MS));
    }
}

/* =========================================================
 * ┌────────────────────────────────────────────────────┐
 * │  태스크 5: vTaskUartTx                              │
 * │  우선순위: 낮음   주기: xTxQueue 이벤트 + 100ms     │
 * │  역할: TX 큐에서 문자열 꺼내 UART 전송              │
 * │        100ms마다 자동 텔레메트리 전송               │
 * └────────────────────────────────────────────────────┘
 * ========================================================= */
static void vTaskUartTx(void *pvParameters)
{
    (void)pvParameters;
    TxMessage_t msg;
    TickType_t  xLastTelemetry = xTaskGetTickCount();

    for (;;) {
        /* TX 큐에서 메시지 전송 (최대 10ms 대기) */
        while (xQueueReceive(xTxQueue, &msg, pdMS_TO_TICKS(10U)) == pdPASS) {
            HAL_UART_Transmit(&huart3,
                              (const uint8_t *)msg.msg,
                              (uint16_t)strlen(msg.msg),
                              100U);
        }

        /* 100ms 주기 자동 텔레메트리 */
        if ((xTaskGetTickCount() - xLastTelemetry) >=
            pdMS_TO_TICKS(TELEMETRY_PERIOD_MS))
        {
            xLastTelemetry = xTaskGetTickCount();

            /* 활성화된 채널만 전송 */
            for (uint8_t ch = 0U; ch < NUM_CHANNELS; ch++) {
                if (g_channels[ch].enabled ||
                    g_channels[ch].status != CH_STATUS_DISABLED) {
                    UartProto_SendChannelData(ch);
                    /* 큐에서 바로 꺼내 전송 */
                    if (xQueueReceive(xTxQueue, &msg, 0U) == pdPASS) {
                        HAL_UART_Transmit(&huart3,
                                          (const uint8_t *)msg.msg,
                                          (uint16_t)strlen(msg.msg),
                                          100U);
                    }
                }
            }
        }
    }
}

/* =========================================================
 * 클럭 설정 (STM32H563, 250MHz)
 * HSE 8MHz -> PLL1 (M=2, N=125, P=2) -> 250MHz
 * ========================================================= */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInit = {0};
    RCC_ClkInitTypeDef RCC_ClkInit = {0};

    /* 전원 공급 범위 설정 (250MHz는 VOS0 필요) */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) { }

    /* HSE + PLL 설정 */
    RCC_OscInit.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInit.HSEState            = RCC_HSE_ON;
    RCC_OscInit.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInit.PLL.PLLSource       = RCC_PLL1_SOURCE_HSE;
    RCC_OscInit.PLL.PLLM            = 2U;
    RCC_OscInit.PLL.PLLN            = 125U;
    RCC_OscInit.PLL.PLLP            = 2U;
    RCC_OscInit.PLL.PLLQ            = 2U;
    RCC_OscInit.PLL.PLLR            = 2U;
    RCC_OscInit.PLL.PLLRGE          = RCC_PLL1_VCIRANGE_2;
    RCC_OscInit.PLL.PLLVCOSEL       = RCC_PLL1_VCORANGE_WIDE;
    RCC_OscInit.PLL.PLLFRACN        = 0U;

    if (HAL_RCC_OscConfig(&RCC_OscInit) != HAL_OK) {
        Error_Handler();
    }

    /* 버스 클럭 설정 */
    RCC_ClkInit.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                                  RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInit.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInit.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInit.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInit.APB2CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInit.APB3CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInit, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
}

/* =========================================================
 * GPIO 초기화
 * ========================================================= */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_Init = {0};

    /* 클럭 활성화 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* MCP4922 CS 핀 (PB12): 기본 HIGH */
    HAL_GPIO_WritePin(MCP4922_CS_GPIO, MCP4922_CS_PIN, GPIO_PIN_SET);
    GPIO_Init.Pin   = MCP4922_CS_PIN;
    GPIO_Init.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_Init.Pull  = GPIO_NOPULL;
    GPIO_Init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MCP4922_CS_GPIO, &GPIO_Init);

    /* 녹색 LED (PA5) */
    GPIO_Init.Pin   = LED_GREEN_PIN;
    GPIO_Init.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_Init.Pull  = GPIO_NOPULL;
    GPIO_Init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GREEN_GPIO, &GPIO_Init);

    /* 빨간 LED (PG0) - Nucleo H563ZI LD6 */
    GPIO_Init.Pin   = LED_RED_PIN;
    HAL_GPIO_Init(LED_RED_GPIO, &GPIO_Init);
}

/* =========================================================
 * DMA 초기화 (ADC1용)
 * ========================================================= */
static void MX_DMA_Init(void)
{
    /* GPDMA1 클럭 활성화 (STM32H5는 GPDMA 사용) */
    __HAL_RCC_GPDMA1_CLK_ENABLE();

    /* DMA 인터럽트 설정 */
    HAL_NVIC_SetPriority(GPDMA1_Channel0_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);
}

/* =========================================================
 * DAC1 초기화 (CH1: PA4, CH2: PA5)
 * ========================================================= */
static void MX_DAC1_Init(void)
{
    __HAL_RCC_DAC1_CLK_ENABLE();

    hdac1.Instance = DAC1;
    if (HAL_DAC_Init(&hdac1) != HAL_OK) {
        Error_Handler();
    }

    DAC_ChannelConfTypeDef sConfig = {0};
    sConfig.DAC_HighFrequency       = DAC_HIGH_FREQUENCY_INTERFACE_MODE_ABOVE_80MHZ;
    sConfig.DAC_DMADoubleDataMode   = DISABLE;
    sConfig.DAC_SignedFormat         = DISABLE;
    sConfig.DAC_SampleAndHold       = DAC_SAMPLEANDHOLD_DISABLE;
    sConfig.DAC_Trigger              = DAC_TRIGGER_NONE;
    sConfig.DAC_OutputBuffer         = DAC_OUTPUTBUFFER_ENABLE;
    sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_EXTERNAL;
    sConfig.DAC_UserTrimming         = DAC_TRIMMING_FACTORY;

    if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_2) != HAL_OK) {
        Error_Handler();
    }
}

/* =========================================================
 * SPI2 초기화 (MCP4922용, Mode0, 20MHz)
 * MOSI: PB15, SCK: PB13
 * ========================================================= */
static void MX_SPI2_Init(void)
{
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* GPIO 핀 설정: SPI2 AF (PB13=SCK, PB15=MOSI) */
    GPIO_InitTypeDef GPIO_Init = {0};
    GPIO_Init.Pin       = GPIO_PIN_13 | GPIO_PIN_15;
    GPIO_Init.Mode      = GPIO_MODE_AF_PP;
    GPIO_Init.Pull      = GPIO_NOPULL;
    GPIO_Init.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_Init.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &GPIO_Init);

    hspi2.Instance               = SPI2;
    hspi2.Init.Mode              = SPI_MODE_MASTER;
    hspi2.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity       = SPI_POLARITY_LOW;    /* CPOL=0 */
    hspi2.Init.CLKPhase          = SPI_PHASE_1EDGE;     /* CPHA=0 -> Mode0 */
    hspi2.Init.NSS               = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; /* ~15.6MHz */
    hspi2.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;

    if (HAL_SPI_Init(&hspi2) != HAL_OK) {
        Error_Handler();
    }
}

/* =========================================================
 * ADC1 초기화 (8채널 스캔, 12-bit, DMA)
 * ========================================================= */
static void MX_ADC1_Init(void)
{
    __HAL_RCC_ADC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* 아날로그 GPIO 설정 */
    GPIO_InitTypeDef GPIO_Init = {0};
    GPIO_Init.Mode = GPIO_MODE_ANALOG;
    GPIO_Init.Pull = GPIO_NOPULL;

    /* PA0~PA3: 전압 채널 */
    GPIO_Init.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    HAL_GPIO_Init(GPIOA, &GPIO_Init);

    /* PC0~PC3: 전류 채널 */
    GPIO_Init.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    HAL_GPIO_Init(GPIOC, &GPIO_Init);

    /* ADC1 기본 설정 */
    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_ASYNC_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = ADC_SCAN_ENABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SEQ_CONV;
    hadc1.Init.LowPowerAutoWait      = DISABLE;
    hadc1.Init.ContinuousConvMode    = ENABLE;     /* 연속 변환 */
    hadc1.Init.NbrOfConversion       = ADC_NUM_CH; /* 8채널 */
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
    hadc1.Init.OversamplingMode      = DISABLE;    /* 소프트웨어 오버샘플 사용 */

    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }

    /* 채널 설정 (샘플 순서: V1~V4, I1~I4) */
    ADC_ChannelConfTypeDef sChConf = {0};
    sChConf.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
    sChConf.SingleDiff   = ADC_SINGLE_ENDED;
    sChConf.OffsetNumber = ADC_OFFSET_NONE;
    sChConf.Offset       = 0U;

    /* V_SENSE 채널: PA0~PA3 (INP0~INP3 or INP6,INP7 depending on variant) */
    const uint32_t v_ch[4] = {ADC_CHANNEL_0, ADC_CHANNEL_1,
                               ADC_CHANNEL_6, ADC_CHANNEL_7};
    /* I_SENSE 채널: PC0~PC3 (INP10~INP13) */
    const uint32_t i_ch[4] = {ADC_CHANNEL_10, ADC_CHANNEL_11,
                               ADC_CHANNEL_12, ADC_CHANNEL_13};

    for (uint8_t r = 0U; r < 4U; r++) {
        sChConf.Channel = v_ch[r];
        sChConf.Rank    = r + 1U;
        if (HAL_ADC_ConfigChannel(&hadc1, &sChConf) != HAL_OK) {
            Error_Handler();
        }
    }
    for (uint8_t r = 0U; r < 4U; r++) {
        sChConf.Channel = i_ch[r];
        sChConf.Rank    = r + 5U;
        if (HAL_ADC_ConfigChannel(&hadc1, &sChConf) != HAL_OK) {
            Error_Handler();
        }
    }

    /* DMA 링크 (HAL_ADC_MspInit에서 hdma_adc1 설정 필요) */
}

/* =========================================================
 * USART3 초기화 (115200, 8N1)
 * PD8: TX, PD9: RX
 * ========================================================= */
static void MX_USART3_UART_Init(void)
{
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_Init = {0};
    GPIO_Init.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_Init.Mode      = GPIO_MODE_AF_PP;
    GPIO_Init.Pull      = GPIO_NOPULL;
    GPIO_Init.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_Init.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &GPIO_Init);

    /* UART 인터럽트 설정 */
    HAL_NVIC_SetPriority(USART3_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(USART3_IRQn);

    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = 115200U;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart3) != HAL_OK) {
        Error_Handler();
    }
}

/* =========================================================
 * HAL MSP 콜백: ADC1 DMA 연결
 * STM32H5는 GPDMA(General Purpose DMA) 사용
 * ========================================================= */
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        /* GPDMA1 Channel 0 설정 */
        hdma_adc1.Instance                  = GPDMA1_Channel0;
        hdma_adc1.Init.Request              = GPDMA1_REQUEST_ADC1;
        hdma_adc1.Init.BlkHWRequest         = DMA_BREQ_SINGLE_BURST;
        hdma_adc1.Init.Direction            = DMA_PERIPH_TO_MEMORY;
        hdma_adc1.Init.SrcInc               = DMA_SINC_FIXED;
        hdma_adc1.Init.DestInc              = DMA_DINC_INCREMENTED;
        hdma_adc1.Init.SrcDataWidth         = DMA_SRC_DATAWIDTH_HALFWORD;
        hdma_adc1.Init.DestDataWidth        = DMA_DEST_DATAWIDTH_HALFWORD;
        hdma_adc1.Init.Priority             = DMA_LOW_PRIORITY_HIGH_WEIGHT;
        hdma_adc1.Init.SrcBurstLength       = 1U;
        hdma_adc1.Init.DestBurstLength      = 1U;
        hdma_adc1.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 |
                                               DMA_DEST_ALLOCATED_PORT0;
        hdma_adc1.Init.TransferEventMode    = DMA_TCEM_BLOCK_TRANSFER;
        hdma_adc1.Init.Mode                 = DMA_NORMAL;

        if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) {
            Error_Handler();
        }

        __HAL_LINKDMA(hadc, DMA_Handle, hdma_adc1);
    }
}

/* =========================================================
 * 인터럽트 핸들러
 * ========================================================= */

/* GPDMA1 Channel 0 (ADC1 DMA) */
void GPDMA1_Channel0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_adc1);
}

/* USART3 */
void USART3_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart3);
}

/* =========================================================
 * 에러 핸들러
 * ========================================================= */
void Error_Handler(void)
{
    __disable_irq();
    /* 빨간 LED 점등 후 무한 대기 */
    HAL_GPIO_WritePin(LED_RED_GPIO, LED_RED_PIN, GPIO_PIN_SET);
    while (1) { }
}

/* =========================================================
 * FreeRTOS 스택 오버플로 훅
 * ========================================================= */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    Error_Handler();
}

/* =========================================================
 * FreeRTOS 힙 할당 실패 훅
 * ========================================================= */
void vApplicationMallocFailedHook(void)
{
    Error_Handler();
}

/* =========================================================
 * assert 실패 핸들러
 * ========================================================= */
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    Error_Handler();
}
#endif
