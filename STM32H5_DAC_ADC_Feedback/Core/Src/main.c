/**
 * @file    main.c
 * @brief   STM32H5 DAC/ADC 피드백 전압 제어 메인 진입점
 *
 * 클럭 구성 (STM32H563, 예시):
 *   HSE = 25 MHz → PLL1 → SYSCLK = 250 MHz
 *   PCLK1 = 125 MHz (APB1: TIM3, UART3)
 *   PCLK2 = 125 MHz (APB2: ADC1)
 *   DAC, ADC 클럭: PCLK2 / prescaler
 *
 * 핀 배치:
 *   PA0  : ADC1_IN0  (채널 1 피드백)
 *   PA1  : ADC1_IN1  (채널 2 피드백)
 *   PA2  : ADC1_IN2  (채널 3 피드백)
 *   PA3  : ADC1_IN3  (채널 4 피드백)
 *   PA4  : DAC1_OUT1 (채널 1 출력)
 *   PA5  : DAC1_OUT2 (채널 2 출력)
 *   PA6  : TIM3_CH1  (채널 3 출력 PWM)
 *   PA7  : TIM3_CH2  (채널 4 출력 PWM)
 *   PD8  : USART3_TX (디버그)
 *   PD9  : USART3_RX (디버그)
 */

#include "main.h"
#include "voltage_ctrl.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* ======================================================================== */
/*  전역 주변장치 핸들                                                        */
/* ======================================================================== */

DAC_HandleTypeDef  hdac1;
ADC_HandleTypeDef  hadc1;
TIM_HandleTypeDef  htim3;
UART_HandleTypeDef huart3;

/* ======================================================================== */
/*  FreeRTOS 공유 객체                                                        */
/* ======================================================================== */

SemaphoreHandle_t xADCDoneSem;
SemaphoreHandle_t xCtrlMutex;

/* ADC DMA 버퍼 (4채널, 캐시 정렬을 위해 32바이트 정렬) */
__attribute__((aligned(32)))
static uint16_t s_adc_buf[4];

/* ======================================================================== */
/*  FreeRTOS 태스크 선언 (freertos_tasks.c)                                  */
/* ======================================================================== */

extern void vADCTask(void *pvParameters);
extern void vControlTask(void *pvParameters);
extern void vMonitorTask(void *pvParameters);

/* ======================================================================== */
/*  내부 함수 선언                                                            */
/* ======================================================================== */

static void SystemClock_Config(void);
static void MX_DAC1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);

/* ======================================================================== */
/*  main()                                                                   */
/* ======================================================================== */

int main(void)
{
    /* ---- HAL 기본 초기화 ---- */
    HAL_Init();
    SystemClock_Config();

    /* ---- GPIO / DMA 초기화 ---- */
    MX_GPIO_Init();
    MX_DMA_Init();

    /* ---- 주변장치 초기화 ---- */
    MX_USART3_UART_Init();
    MX_DAC1_Init();
    MX_TIM3_Init();
    MX_ADC1_Init();

    /* ---- 전압 제어 레이어 초기화 ---- */
    VCtrl_Init();

    /* ---- FreeRTOS 공유 객체 생성 ---- */
    xADCDoneSem = xSemaphoreCreateBinary();
    xCtrlMutex  = xSemaphoreCreateMutex();
    if (xADCDoneSem == NULL || xCtrlMutex == NULL) {
        Error_Handler();
    }

    /* ---- FreeRTOS 태스크 생성 ---- */
    BaseType_t ret;

    ret = xTaskCreate(vADCTask,     "ADC",     STACK_ADC,     s_adc_buf, TASK_PRIO_ADC,     NULL);
    if (ret != pdPASS) Error_Handler();

    ret = xTaskCreate(vControlTask, "CTRL",    STACK_CONTROL, NULL,      TASK_PRIO_CONTROL, NULL);
    if (ret != pdPASS) Error_Handler();

    ret = xTaskCreate(vMonitorTask, "MON",     STACK_MONITOR, NULL,      TASK_PRIO_MONITOR, NULL);
    if (ret != pdPASS) Error_Handler();

    /* ---- 스케줄러 시작 ---- */
    vTaskStartScheduler();

    /* 여기에 도달하면 스택/힙 부족 */
    Error_Handler();
    while (1) {}
}

/* ======================================================================== */
/*  SystemClock_Config                                                        */
/*  HSE 25 MHz → PLL1 → SYSCLK 250 MHz                                      */
/* ======================================================================== */

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* 전압 스케일: VOS0 (최대 성능) */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    /* HSE=25MHz, M=5→5MHz, N=100→500MHz, P=2→250MHz */
    osc.PLL.PLLM = 5;
    osc.PLL.PLLN = 100;
    osc.PLL.PLLP = 2;
    osc.PLL.PLLQ = 2;
    osc.PLL.PLLR = 2;
    osc.PLL.PLLRGE    = RCC_PLL1VCIRANGE_1;
    osc.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    osc.PLL.PLLFRACN  = 0;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2 |
                         RCC_CLOCKTYPE_PCLK3;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    clk.APB3CLKDivider = RCC_HCLK_DIV2;

    /* Flash 지연: SYSCLK 250 MHz → WS=5 */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) Error_Handler();
}

/* ======================================================================== */
/*  GPIO                                                                      */
/* ======================================================================== */

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    /* 아날로그 핀(PA0~PA7)은 ADC/DAC 초기화 시 자동 구성 */
}

/* ======================================================================== */
/*  DMA (ADC1 → s_adc_buf)                                                   */
/* ======================================================================== */

static void MX_DMA_Init(void)
{
    /* STM32H5: GPDMA1 사용 */
    __HAL_RCC_GPDMA1_CLK_ENABLE();

    /* DMA 인터럽트 우선순위 설정 (FreeRTOS 관리 범위 내) */
    HAL_NVIC_SetPriority(GPDMA1_Channel0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);
}

/* ======================================================================== */
/*  DAC1 초기화 (CH1=PA4, CH2=PA5)                                           */
/* ======================================================================== */

static void MX_DAC1_Init(void)
{
    __HAL_RCC_DAC1_CLK_ENABLE();

    hdac1.Instance = DAC1;
    if (HAL_DAC_Init(&hdac1) != HAL_OK) Error_Handler();

    DAC_ChannelConfTypeDef cfg = {0};
    cfg.DAC_HighFrequency        = DAC_HIGH_FREQUENCY_INTERFACE_MODE_AUTOMATIC;
    cfg.DAC_DMADoubleDataMode    = DISABLE;
    cfg.DAC_SignedFormat         = DISABLE;
    cfg.DAC_Trigger              = DAC_TRIGGER_NONE;
    cfg.DAC_OutputBuffer         = DAC_OUTPUTBUFFER_ENABLE;
    cfg.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_EXTERNAL;
    cfg.DAC_UserTrimming         = DAC_TRIMMING_FACTORY;

    if (HAL_DAC_ConfigChannel(&hdac1, &cfg, DAC_CHANNEL_1) != HAL_OK) Error_Handler();
    if (HAL_DAC_ConfigChannel(&hdac1, &cfg, DAC_CHANNEL_2) != HAL_OK) Error_Handler();
}

/* ======================================================================== */
/*  ADC1 초기화 (PA0~PA3, 4채널 DMA 스캔)                                   */
/* ======================================================================== */

static void MX_ADC1_Init(void)
{
    __HAL_RCC_ADC_CLK_ENABLE();

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_ASYNC_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = ADC_SCAN_ENABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SEQ_CONV;
    hadc1.Init.LowPowerAutoWait      = DISABLE;
    hadc1.Init.ContinuousConvMode    = ENABLE;   /* 연속 변환 */
    hadc1.Init.NbrOfConversion       = 4;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
    hadc1.Init.OversamplingMode      = ENABLE;

    /* 오버샘플링: 16x → 유효 비트 향상 */
    hadc1.Init.Oversampling.Ratio                 = ADC_OVERSAMPLING_RATIO_16;
    hadc1.Init.Oversampling.RightBitShift         = ADC_RIGHTBITSHIFT_4;
    hadc1.Init.Oversampling.TriggeredMode         = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
    hadc1.Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;

    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();

    /* 채널 설정 */
    ADC_ChannelConfTypeDef ch_cfg = {0};
    ch_cfg.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
    ch_cfg.SingleDiff   = ADC_SINGLE_ENDED;
    ch_cfg.OffsetNumber = ADC_OFFSET_NONE;
    ch_cfg.Offset       = 0;

    const uint32_t channels[4] = {
        ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3
    };
    for (uint8_t i = 0; i < 4; i++) {
        ch_cfg.Channel = channels[i];
        ch_cfg.Rank    = ADC_REGULAR_RANK_1 + i;
        if (HAL_ADC_ConfigChannel(&hadc1, &ch_cfg) != HAL_OK) Error_Handler();
    }

    /* ADC 캘리브레이션 */
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK) Error_Handler();

    /* DMA 모드로 변환 시작 */
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_adc_buf, 4) != HAL_OK) Error_Handler();
}

/* ======================================================================== */
/*  TIM3 PWM 초기화 (CH1=PA6, CH2=PA7, ARR=4095, 12-bit 등가)               */
/* ======================================================================== */

static void MX_TIM3_Init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();

    /*
     * PWM 주파수 계산 (PCLK1 = 125 MHz):
     *   Fout = PCLK1 / ((PSC+1) * (ARR+1))
     *        = 125e6 / (31 * 4096) ≈ 977 Hz
     * RC 필터 fc << 977 Hz → 예: R=10kΩ, C=100µF → fc ≈ 0.16 Hz (충분한 평탄화)
     * 실용적으로는 R=1kΩ, C=10µF → fc ≈ 15.9 Hz 권장
     */
    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 30;   /* PSC = 30 → TIM_CLK = 125M/31 ≈ 4.03 MHz */
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 4095; /* ARR = 4095 → 12-bit 해상도 */
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) Error_Handler();

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode       = TIM_OCMODE_PWM1;
    oc.Pulse        = 2047;  /* 초기 듀티: ~50% */
    oc.OCPolarity   = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode   = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_2) != HAL_OK) Error_Handler();

    /* 핀 설정: PA6 = AF2(TIM3_CH1), PA7 = AF2(TIM3_CH2) */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOA, &gpio);
}

/* ======================================================================== */
/*  USART3 초기화 (PD8=TX, PD9=RX, 115200 baud)                             */
/* ======================================================================== */

static void MX_USART3_UART_Init(void)
{
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &gpio);

    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = 115200;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart3) != HAL_OK) Error_Handler();
}

/* ======================================================================== */
/*  HAL ADC 변환 완료 콜백 (DMA 전송 완료 시 호출)                           */
/* ======================================================================== */

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) return;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xADCDoneSem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ======================================================================== */
/*  오류 핸들러                                                               */
/* ======================================================================== */

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        /* 디버거로 오류 지점 확인 */
    }
}

/* ======================================================================== */
/*  FreeRTOS 스택 오버플로 훅                                                 */
/* ======================================================================== */

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    Error_Handler();
}

/* ======================================================================== */
/*  FreeRTOS 메모리 할당 실패 훅                                              */
/* ======================================================================== */

void vApplicationMallocFailedHook(void)
{
    Error_Handler();
}
