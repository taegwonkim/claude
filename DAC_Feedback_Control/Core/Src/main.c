/**
 ******************************************************************************
 * @file    main.c
 * @brief   4ch DAC 피드백 제어 시스템 메인
 *
 * FreeRTOS 태스크 구조:
 * ┌─────────────────┬──────────┬────────────┬────────────────────────────────┐
 * │ 태스크           │ 우선순위  │ 주기       │ 역할                           │
 * ├─────────────────┼──────────┼────────────┼────────────────────────────────┤
 * │ UARTRxTask      │ High(6)  │ 이벤트 기반 │ UART 명령 파싱 및 채널 제어     │
 * │ ADCMeasureTask  │ AbvNrm(5)│ 2ms        │ 8ch ADC 순차 측정 + 결과 저장   │
 * │ DACControlTask  │ AbvNrm(5)│ 5ms        │ PI 피드백 제어 + DAC 출력       │
 * │ FaultDetectTask │ Normal(4)│ 20ms       │ 단락/단선 감지 및 채널 차단     │
 * │ DataTxTask      │ BlwNrm(3)│ 100ms      │ UART+FDCAN 주기 데이터 전송     │
 * └─────────────────┴──────────┴────────────┴────────────────────────────────┘
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "dac_ad5641.h"
#include "adc_mcp3465r.h"
#include "feedback_control.h"
#include "uart_comm.h"
#include "fdcan_comm.h"
#include <string.h>

/* =========================================================================
 * 전역 변수 (헤더에 extern 선언)
 * ========================================================================= */
volatile ChannelData_t g_channels[NUM_CHANNELS];

/* =========================================================================
 * HAL 핸들 (CubeMX 생성 – 여기서는 선언만)
 * ========================================================================= */
SPI_HandleTypeDef    hspi1;
SPI_HandleTypeDef    hspi2;
UART_HandleTypeDef   huart1;
FDCAN_HandleTypeDef  hfdcan1;
DMA_HandleTypeDef    hdma_usart1_tx;
DMA_HandleTypeDef    hdma_usart1_rx;

/* =========================================================================
 * FreeRTOS 객체
 * ========================================================================= */
osMutexId_t         xMutexSPI1;
osMutexId_t         xMutexSPI2;
osMutexId_t         xMutexChannelData;
osMessageQueueId_t  xQueueUARTCmd;
osMessageQueueId_t  xQueueTxData;

/* 태스크 핸들 */
static osThreadId_t s_uartRxTaskHandle;
static osThreadId_t s_adcTaskHandle;
static osThreadId_t s_dacCtrlTaskHandle;
static osThreadId_t s_faultTaskHandle;
static osThreadId_t s_dataTxTaskHandle;

/* 태스크 속성 */
static const osThreadAttr_t uartRxTask_attr = {
    .name       = "UARTRxTask",
    .stack_size = 256 * 4,
    .priority   = osPriorityHigh,
};
static const osThreadAttr_t adcTask_attr = {
    .name       = "ADCMeasureTask",
    .stack_size = 256 * 4,
    .priority   = osPriorityAboveNormal,
};
static const osThreadAttr_t dacCtrlTask_attr = {
    .name       = "DACControlTask",
    .stack_size = 256 * 4,
    .priority   = osPriorityAboveNormal,
};
static const osThreadAttr_t faultTask_attr = {
    .name       = "FaultDetectTask",
    .stack_size = 128 * 4,
    .priority   = osPriorityNormal,
};
static const osThreadAttr_t dataTxTask_attr = {
    .name       = "DataTxTask",
    .stack_size = 256 * 4,
    .priority   = osPriorityBelowNormal,
};

/* =========================================================================
 * 함수 프로토타입
 * ========================================================================= */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_FDCAN1_Init(void);

/* FreeRTOS 태스크 */
static void vTaskUARTReceive(void *argument);
static void vTaskADCMeasure(void *argument);
static void vTaskDACControl(void *argument);
static void vTaskFaultDetect(void *argument);
static void vTaskDataTransmit(void *argument);

/* =========================================================================
 * main()
 * ========================================================================= */
int main(void)
{
    /* HAL 초기화 */
    HAL_Init();
    SystemClock_Config();

    /* 주변장치 초기화 */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_SPI1_Init();
    MX_SPI2_Init();
    MX_USART1_UART_Init();
    MX_FDCAN1_Init();

    /* 전역 채널 데이터 초기화 */
    memset((void *)g_channels, 0, sizeof(g_channels));

    /* FreeRTOS 커널 초기화 */
    osKernelInitialize();

    /* ── 뮤텍스 생성 ── */
    xMutexSPI1        = osMutexNew(NULL);
    xMutexSPI2        = osMutexNew(NULL);
    xMutexChannelData = osMutexNew(NULL);

    if (xMutexSPI1 == NULL || xMutexSPI2 == NULL || xMutexChannelData == NULL) {
        Error_Handler();
    }

    /* ── 큐 생성 ── */
    xQueueUARTCmd = osMessageQueueNew(10, sizeof(UartCmd_t), NULL);
    xQueueTxData  = osMessageQueueNew(4,  sizeof(TxPacket_t), NULL);

    if (xQueueUARTCmd == NULL || xQueueTxData == NULL) {
        Error_Handler();
    }

    /* ── 태스크 생성 ── */
    s_uartRxTaskHandle  = osThreadNew(vTaskUARTReceive,  NULL, &uartRxTask_attr);
    s_adcTaskHandle     = osThreadNew(vTaskADCMeasure,   NULL, &adcTask_attr);
    s_dacCtrlTaskHandle = osThreadNew(vTaskDACControl,   NULL, &dacCtrlTask_attr);
    s_faultTaskHandle   = osThreadNew(vTaskFaultDetect,  NULL, &faultTask_attr);
    s_dataTxTaskHandle  = osThreadNew(vTaskDataTransmit, NULL, &dataTxTask_attr);

    if (!s_uartRxTaskHandle || !s_adcTaskHandle || !s_dacCtrlTaskHandle
        || !s_faultTaskHandle || !s_dataTxTaskHandle) {
        Error_Handler();
    }

    /* FreeRTOS 스케줄러 시작 (이후 반환 없음) */
    osKernelStart();

    /* 여기에 도달하면 오류 */
    while (1) {}
}

/* =========================================================================
 * FreeRTOS 태스크 구현
 * ========================================================================= */

/**
 * @brief  UARTRxTask – UART 명령 수신 및 처리
 *         우선순위: osPriorityHigh
 *         큐에서 명령을 꺼내 g_channels 업데이트
 */
static void vTaskUARTReceive(void *argument)
{
    (void)argument;

    /* 드라이버 초기화 */
    UART_Init();
    UART_SendString("\r\n=== 4CH DAC Feedback Control System ===\r\n");
    UART_SendString("Commands: SET <ch> <mV> | EN <ch> | DIS <ch> | RST [ch] | GET\r\n\r\n");

    UartCmd_t cmd;

    for (;;) {
        /* 큐에서 명령 대기 (최대 100ms) */
        if (osMessageQueueGet(xQueueUARTCmd, &cmd, NULL, 100) != osOK) {
            continue;
        }

        switch (cmd.type) {
            case CMD_SET_VOLTAGE: {
                if (cmd.channel >= NUM_CHANNELS) break;
                if (cmd.value > VREF_MV) {
                    UART_Printf("!ERR CH%u: voltage %umV > Vref %umV\r\n",
                                cmd.channel + 1, cmd.value, VREF_MV);
                    break;
                }

                if (osMutexAcquire(xMutexChannelData, 20) == osOK) {
                    uint16_t old_target = g_channels[cmd.channel].target_mv;
                    g_channels[cmd.channel].target_mv = cmd.value;
                    osMutexRelease(xMutexChannelData);

                    /* 목표값이 크게 변하면 PI 적분항 리셋 */
                    if (abs((int)cmd.value - (int)old_target) > 500) {
                        FeedbackControl_Reset(cmd.channel, cmd.value);
                    }

                    UART_Printf(">CH%u SET %umV OK\r\n", cmd.channel + 1, cmd.value);
                }
                break;
            }

            case CMD_ENABLE_CH: {
                if (cmd.channel >= NUM_CHANNELS) break;

                if (osMutexAcquire(xMutexChannelData, 20) == osOK) {
                    g_channels[cmd.channel].enabled = 1;
                    g_channels[cmd.channel].fault   = FAULT_NONE;
                    osMutexRelease(xMutexChannelData);

                    FeedbackControl_Reset(cmd.channel,
                                          g_channels[cmd.channel].target_mv);
                    UART_Printf(">CH%u ENABLED\r\n", cmd.channel + 1);
                }
                break;
            }

            case CMD_DISABLE_CH: {
                if (cmd.channel >= NUM_CHANNELS) break;

                if (osMutexAcquire(xMutexChannelData, 20) == osOK) {
                    g_channels[cmd.channel].enabled = 0;
                    osMutexRelease(xMutexChannelData);
                }

                DAC_PowerDown(cmd.channel, DAC_PD_HIZ);
                UART_Printf(">CH%u DISABLED\r\n", cmd.channel + 1);
                break;
            }

            case CMD_RESET_FAULT: {
                uint8_t start = 0, end = NUM_CHANNELS;
                if (cmd.channel < NUM_CHANNELS) {
                    start = cmd.channel;
                    end   = cmd.channel + 1;
                }
                for (uint8_t i = start; i < end; i++) {
                    if (osMutexAcquire(xMutexChannelData, 20) == osOK) {
                        g_channels[i].fault = FAULT_NONE;
                        osMutexRelease(xMutexChannelData);
                    }
                    UART_Printf(">CH%u FAULT RESET\r\n", i + 1);
                }
                break;
            }

            case CMD_GET_STATUS:
                UART_SendChannelStatus();
                break;

            default:
                break;
        }
    }
}

/**
 * @brief  ADCMeasureTask – 8ch ADC 순차 측정 (2ms 주기)
 *         측정 결과를 g_channels에 저장
 */
static void vTaskADCMeasure(void *argument)
{
    (void)argument;

    /* ADC 초기화 */
    if (ADC_Init() != HAL_OK) {
        UART_SendString("!ERR ADC Init failed\r\n");
    }

    AdcMeasurement_t meas;
    uint32_t last_wake = osKernelGetTickCount();

    for (;;) {
        /* 2ms 주기 (ADC 변환 시간 고려) */
        osDelayUntil(last_wake + 2);
        last_wake = osKernelGetTickCount();

        if (ADC_ReadAllChannels(&meas) != HAL_OK) {
            continue;
        }

        /* 측정값을 공유 데이터에 기록 */
        if (osMutexAcquire(xMutexChannelData, 5) == osOK) {
            for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
                g_channels[ch].measured_mv = meas.voltage_mv[ch];
                g_channels[ch].current_ma  = meas.current_ma[ch];
            }
            osMutexRelease(xMutexChannelData);
        }
    }
}

/**
 * @brief  DACControlTask – PI 피드백 제어 (5ms 주기)
 *         활성화된 채널에 대해 PI 제어 실행 후 DAC 출력 갱신
 */
static void vTaskDACControl(void *argument)
{
    (void)argument;

    /* DAC + 피드백 제어기 초기화 */
    DAC_Init();
    FeedbackControl_Init();

    uint32_t last_wake = osKernelGetTickCount();

    for (;;) {
        osDelayUntil(last_wake + PI_DT_MS);
        last_wake = osKernelGetTickCount();

        /* 모든 채널 PI 제어 실행 */
        FeedbackControl_RunAll();
    }
}

/**
 * @brief  FaultDetectTask – 단락/단선 감지 (20ms 주기)
 *         전류 측정값으로 SHORT/OPEN 결함 판정
 *         결함 채널은 DAC 출력 차단 (Hi-Z)
 */
static void vTaskFaultDetect(void *argument)
{
    (void)argument;

    uint32_t last_wake = osKernelGetTickCount();

    for (;;) {
        osDelayUntil(last_wake + 20);
        last_wake = osKernelGetTickCount();

        for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
            if (osMutexAcquire(xMutexChannelData, 10) != osOK) continue;

            if (!g_channels[ch].enabled) {
                osMutexRelease(xMutexChannelData);
                continue;
            }

            int16_t  curr_ma  = g_channels[ch].current_ma;
            uint16_t tgt_mv   = g_channels[ch].target_mv;
            FaultStatus_t old_fault = g_channels[ch].fault;

            osMutexRelease(xMutexChannelData);

            FaultStatus_t new_fault = FAULT_NONE;

            /* 단락 판정: 전류가 임계값 초과 */
            if (curr_ma > (int16_t)SHORT_CURRENT_THRESHOLD_MA) {
                new_fault = FAULT_SHORT;
            }
            /* 단선 판정: 목표 전압이 설정됐는데 전류가 극소 */
            else if (tgt_mv >= MIN_VOLTAGE_FOR_OPEN_MV
                     && curr_ma < (int16_t)OPEN_CURRENT_THRESHOLD_MA) {
                new_fault = FAULT_OPEN;
            }

            if (new_fault != old_fault) {
                if (osMutexAcquire(xMutexChannelData, 10) == osOK) {
                    g_channels[ch].fault = new_fault;
                    osMutexRelease(xMutexChannelData);
                }

                if (new_fault != FAULT_NONE) {
                    /* 결함 채널 DAC 출력 차단 */
                    DAC_PowerDown(ch, DAC_PD_HIZ);

                    const char *flt_str = (new_fault == FAULT_SHORT) ? "SHORT" : "OPEN";
                    UART_Printf("!FAULT CH%u %s curr=%dmA\r\n",
                                ch + 1, flt_str, curr_ma);

                    /* LED 빠르게 점멸 (간단한 표시) */
                    HAL_GPIO_TogglePin(LED_STATUS_PORT, LED_STATUS_PIN);
                }
            }
        }
    }
}

/**
 * @brief  DataTxTask – 주기적 데이터 전송 (100ms 주기)
 *         모든 채널 데이터를 UART + FDCAN으로 동시 전송
 */
static void vTaskDataTransmit(void *argument)
{
    (void)argument;

    /* FDCAN 초기화 */
    if (FDCAN_CommInit() != HAL_OK) {
        UART_SendString("!ERR FDCAN Init failed\r\n");
    }

    uint32_t last_wake = osKernelGetTickCount();
    uint32_t tx_count  = 0;

    for (;;) {
        osDelayUntil(last_wake + 100);
        last_wake = osKernelGetTickCount();
        tx_count++;

        /* ── UART 주기 전송 ── */
        UART_SendChannelStatus();

        /* ── FDCAN 전송 ── */
        FDCAN_SendVoltage();
        FDCAN_SendCurrent();

        /* 10주기(1초)마다 상태 프레임 전송 */
        if (tx_count % 10 == 0) {
            FDCAN_SendStatus();
            HAL_GPIO_TogglePin(LED_STATUS_PORT, LED_STATUS_PIN);
        }
    }
}

/* =========================================================================
 * 주변장치 초기화 (CubeMX 생성 코드 패턴 – 직접 작성)
 * ========================================================================= */

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = { 0 };
    RCC_ClkInitTypeDef clk = { 0 };

    /* 전압 범위 Range 1 (80MHz 허용) */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) {
        Error_Handler();
    }

    /* HSE + PLL → 80MHz */
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState            = RCC_HSE_ON;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM            = 1;        /* /1 */
    osc.PLL.PLLN            = 10;       /* ×10 */
    osc.PLL.PLLP            = RCC_PLLP_DIV7;
    osc.PLL.PLLQ            = RCC_PLLQ_DIV2;
    osc.PLL.PLLR            = RCC_PLLR_DIV2;   /* SYSCLK = 16×10/2 = 80MHz */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
                       | RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) Error_Handler();

    /* FDCAN 클럭: PLL1Q = 80MHz / 2 = 40MHz */
    RCC_PeriphCLKInitTypeDef pclk = { 0 };
    pclk.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    pclk.FdcanClockSelection  = RCC_FDCANCLKSOURCE_PLL;
    if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = { 0 };

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* DAC CS 핀 초기화 (High) */
    HAL_GPIO_WritePin(GPIOC,
                      DAC_CS0_PIN | DAC_CS1_PIN | DAC_CS2_PIN | DAC_CS3_PIN,
                      GPIO_PIN_SET);
    gpio.Pin   = DAC_CS0_PIN | DAC_CS1_PIN | DAC_CS2_PIN | DAC_CS3_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOC, &gpio);

    /* ADC CS 핀 (High) */
    HAL_GPIO_WritePin(ADC_CS_PORT, ADC_CS_PIN, GPIO_PIN_SET);
    gpio.Pin   = ADC_CS_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ADC_CS_PORT, &gpio);

    /* ADC IRQ 핀 (Input) */
    gpio.Pin  = ADC_IRQ_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(ADC_IRQ_PORT, &gpio);

    /* LED 상태 핀 */
    HAL_GPIO_WritePin(LED_STATUS_PORT, LED_STATUS_PIN, GPIO_PIN_RESET);
    gpio.Pin   = LED_STATUS_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_STATUS_PORT, &gpio);
}

static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* USART1 TX: DMA1 Channel 4 */
    HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);

    /* USART1 RX: DMA1 Channel 5 */
    HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
}

static void MX_SPI1_Init(void)
{
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA5=SCK, PA7=MOSI */
    GPIO_InitTypeDef gpio = { 0 };
    gpio.Pin       = GPIO_PIN_5 | GPIO_PIN_7;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &gpio);

    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_1LINE;   /* MOSI only for DAC */
    hspi1.Init.DataSize          = SPI_DATASIZE_16BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;       /* Mode 0 */
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; /* 80/8=10MHz */
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
}

static void MX_SPI2_Init(void)
{
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PB13=SCK, PB14=MISO, PB15=MOSI */
    GPIO_InitTypeDef gpio = { 0 };
    gpio.Pin       = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &gpio);

    hspi2.Instance               = SPI2;
    hspi2.Init.Mode              = SPI_MODE_MASTER;
    hspi2.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase          = SPI_PHASE_1EDGE;       /* Mode 0 */
    hspi2.Init.NSS               = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; /* 80/8=10MHz */
    hspi2.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;
    if (HAL_SPI_Init(&hspi2) != HAL_OK) Error_Handler();
}

static void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA9=TX, PA10=RX */
    GPIO_InitTypeDef gpio = { 0 };
    gpio.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* DMA TX */
    hdma_usart1_tx.Instance                 = DMA1_Channel4;
    hdma_usart1_tx.Init.Request             = DMA_REQUEST_USART1_TX;
    hdma_usart1_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_usart1_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart1_tx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart1_tx.Init.Mode                = DMA_NORMAL;
    hdma_usart1_tx.Init.Priority            = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK) Error_Handler();
    __HAL_LINKDMA(&huart1, hdmatx, hdma_usart1_tx);

    /* DMA RX */
    hdma_usart1_rx.Instance                 = DMA1_Channel5;
    hdma_usart1_rx.Init.Request             = DMA_REQUEST_USART1_RX;
    hdma_usart1_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart1_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode                = DMA_CIRCULAR;
    hdma_usart1_rx.Init.Priority            = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK) Error_Handler();
    __HAL_LINKDMA(&huart1, hdmarx, hdma_usart1_rx);

    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();

    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

static void MX_FDCAN1_Init(void)
{
    __HAL_RCC_FDCAN_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* PD0=RX, PD1=TX */
    GPIO_InitTypeDef gpio = { 0 };
    gpio.Pin       = GPIO_PIN_0 | GPIO_PIN_1;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF9_FDCAN1;
    HAL_GPIO_Init(GPIOD, &gpio);

    /*
     * FDCAN 클럭 = 40MHz (PLL1Q / 2)
     * Nominal 1Mbps: prescaler=4, Seg1=8, Seg2=1, SJW=1
     *   TQ = 1/(40MHz/4) = 100ns
     *   Bit time = (1+8+1)×100ns = 1μs → 1Mbps
     */
    hfdcan1.Instance                  = FDCAN1;
    hfdcan1.Init.ClockDivider         = FDCAN_CLOCK_DIV1;
    hfdcan1.Init.FrameFormat          = FDCAN_FRAME_FD_BRS;
    hfdcan1.Init.Mode                 = FDCAN_MODE_NORMAL;
    hfdcan1.Init.AutoRetransmission   = ENABLE;
    hfdcan1.Init.TransmitPause        = DISABLE;
    hfdcan1.Init.ProtocolException    = ENABLE;
    hfdcan1.Init.NominalPrescaler     = 4;
    hfdcan1.Init.NominalSyncJumpWidth = 1;
    hfdcan1.Init.NominalTimeSeg1      = 8;
    hfdcan1.Init.NominalTimeSeg2      = 1;
    hfdcan1.Init.DataPrescaler        = 2;
    hfdcan1.Init.DataSyncJumpWidth    = 2;
    hfdcan1.Init.DataTimeSeg1         = 8;
    hfdcan1.Init.DataTimeSeg2         = 1;
    hfdcan1.Init.StdFiltersNbr        = 1;
    hfdcan1.Init.ExtFiltersNbr        = 0;
    hfdcan1.Init.TxFifoQueueMode      = FDCAN_TX_FIFO_OPERATION;
    if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK) Error_Handler();

    HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, 7, 0);
    HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
}

/* =========================================================================
 * 오류 핸들러
 * ========================================================================= */
void Error_Handler(void)
{
    __disable_irq();
    /* LED 빠른 점멸로 오류 표시 */
    while (1) {
        HAL_GPIO_TogglePin(LED_STATUS_PORT, LED_STATUS_PIN);
        HAL_Delay(200);
    }
}

/* =========================================================================
 * 인터럽트 핸들러 (stm32l5xx_it.c에 넣어도 됨)
 * ========================================================================= */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

void DMA1_Channel4_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}

void DMA1_Channel5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
}

void FDCAN1_IT0_IRQHandler(void)
{
    HAL_FDCAN_IRQHandler(&hfdcan1);
}

void SysTick_Handler(void)
{
    HAL_IncTick();
    /* FreeRTOS tick (osSystickHandler 또는 xPortSysTickHandler) */
    if (osKernelGetState() == osKernelRunning) {
        extern void xPortSysTickHandler(void);
        xPortSysTickHandler();
    }
}
