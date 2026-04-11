/* main.c - STM32L552 4채널 PID 전압 제어 메인
 *
 * ============================================================
 * CubeMX 설정 요약 (직접 생성 시 아래와 동일하게 설정)
 * ============================================================
 * [System Core]
 *   RCC  : HSI16 ON, PLL ON (M=4, N=55, R=2) → SYSCLK=110MHz
 *   NVIC : FreeRTOS 인터럽트 우선순위 설정
 *   SYS  : Timebase Source = TIM2 (SysTick은 FreeRTOS에 양도)
 *
 * [ADC1] 연속 스캔 모드 + DMA
 *   Resolution : 12 bit
 *   Scan Mode  : Enabled
 *   Continuous : Enabled
 *   DMA        : ADC1 → DMA1 Channel1, Circular, Half-Word→Word
 *   Oversampling: Ratio=16, RightShift=4 (옵션)
 *   Rank 1  : IN5 (PA0) – CH0 Voltage,  SampleTime=47.5 cycles
 *   Rank 2  : IN6 (PA1) – CH0 Current,  SampleTime=47.5 cycles
 *   Rank 3  : IN7 (PA2) – CH1 Voltage,  SampleTime=47.5 cycles
 *   Rank 4  : IN8 (PA3) – CH1 Current,  SampleTime=47.5 cycles
 *   Rank 5  : IN1 (PC0) – CH2 Voltage,  SampleTime=47.5 cycles
 *   Rank 6  : IN2 (PC1) – CH2 Current,  SampleTime=47.5 cycles
 *   Rank 7  : IN3 (PC2) – CH3 Voltage,  SampleTime=47.5 cycles
 *   Rank 8  : IN4 (PC3) – CH3 Current,  SampleTime=47.5 cycles
 *
 * [TIM1] PWM Generation, 4채널
 *   Clock Source : Internal
 *   PSC=0, ARR=1099 → f_PWM = 110MHz/1100 ≈ 100 kHz
 *   CH1 (PA8) : PWM Generation CH1  – CH0 출력
 *   CH2 (PA9) : PWM Generation CH2  – CH1 출력
 *   CH3 (PA10): PWM Generation CH3  – CH2 출력
 *   CH4 (PA11): PWM Generation CH4  – CH3 출력
 *   MOE (Main Output Enable) : ON
 *
 * [FDCAN1] Classic CAN, 500 kbps
 *   Clock Source : PLLQ (설정: N=55, Q=4 → 55MHz)
 *   Frame Format : Classic CAN
 *   Nominal Prescaler=5, TimeSeg1=14, TimeSeg2=4, SJW=1
 *   (55MHz / 5 = 11MHz TQ clock → 1bit = 20TQ → 550kbps 근사)
 *   TX (PD1), RX (PD0)   AF9
 *   TX FIFO 모드 사용
 *
 * [IWDG]
 *   Prescaler=64, Reload=3125 → ~8s 타이아웃 (LSI≈32kHz)
 *
 * [GPIO]
 *   PC6 : LED_RED   (Output Push-Pull)
 *   PC7 : LED_GREEN (Output Push-Pull)
 *
 * [FreeRTOS] (Middleware)
 *   버전: FreeRTOS Kernel V10.x
 *   Interface: FreeRTOS native API
 *   Heap: heap_4
 * ============================================================
 */
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "channel_ctrl.h"
#include "adc_meas.h"
#include "pwm_out.h"
#include "fdcan_comm.h"
#include "app_tasks.h"

/* ==========================================================
 * HAL 핸들 정의
 * ========================================================== */
ADC_HandleTypeDef  hadc1;
DMA_HandleTypeDef  hdma_adc1;
TIM_HandleTypeDef  htim1;
FDCAN_HandleTypeDef hfdcan1;
IWDG_HandleTypeDef hiwdg;

/* ==========================================================
 * 주변 장치 초기화 함수 프로토타입 (이 파일 내부)
 * ========================================================== */
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM1_Init(void);
static void MX_FDCAN1_Init(void);
static void MX_IWDG_Init(void);

/* ==========================================================
 * main
 * ========================================================== */
int main(void)
{
    /* HAL 초기화 */
    HAL_Init();

    /* 클럭 설정: HSI16 → PLL → 110 MHz */
    SystemClock_Config();

    /* 주변 장치 초기화 */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_TIM1_Init();
    MX_FDCAN1_Init();
    MX_IWDG_Init();

    /* ADC 교정 (시작 전 필수) */
    if (ADCMeas_Calibrate() != HAL_OK) {
        Error_Handler();
    }

    /* 채널 제어 초기화 */
    ChannelCtrl_Init();

    /* FDCAN 필터 및 수신 인터럽트 설정 */
    if (FDCANComm_Init() != HAL_OK) {
        Error_Handler();
    }

    /* ADC DMA 변환 시작 */
    ADCMeas_Start();

    /* PWM 출력 시작 (초기 듀티=0) */
    PWMOut_Start();

    /* -------------------------------------------------------
     * 초기 채널 설정 예시
     *   (실제 운용 시 CAN 명령으로 런타임에 설정)
     * ------------------------------------------------------- */
    ChannelCtrl_SetVoltage(0, 5000U);   /* CH0: 5.0V */
    ChannelCtrl_SetVoltage(1, 3300U);   /* CH1: 3.3V */
    ChannelCtrl_SetVoltage(2, 9000U);   /* CH2: 9.0V */
    ChannelCtrl_SetVoltage(3, 12000U);  /* CH3: 12.0V */

    ChannelCtrl_Enable(0, 1U);
    ChannelCtrl_Enable(1, 1U);
    ChannelCtrl_Enable(2, 1U);
    ChannelCtrl_Enable(3, 1U);

    /* FreeRTOS 태스크 및 공유 객체 생성 */
    AppTasks_Init();

    /* 스케줄러 시작 (이 함수는 반환하지 않음) */
    vTaskStartScheduler();

    /* 여기에 도달하면 힙 부족 등의 오류 */
    for (;;) {}
}

/* ==========================================================
 * SystemClock_Config
 *   HSI16 → PLL → SYSCLK = 110 MHz
 *   PLLQ  = 55 MHz (FDCAN 클럭 소스)
 * ========================================================== */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* 전압 스케일 0 (최고 주파수 지원) */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0) != HAL_OK) {
        Error_Handler();
    }

    /* HSI16 + PLL 설정 */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI
                                          | RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.LSIState            = RCC_LSI_ON;  /* IWDG 용 */
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    /* HSI16(16MHz) / M=4 * N=55 / R=2 = 110 MHz */
    /* HSI16(16MHz) / M=4 * N=55 / Q=4 = 55 MHz  */
    RCC_OscInitStruct.PLL.PLLM            = 4U;
    RCC_OscInitStruct.PLL.PLLN            = 55U;
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV4;  /* 55 MHz for FDCAN */
    RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /* 버스 클럭 설정: AHB=110MHz, APB1=55MHz, APB2=110MHz */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK
                                     | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1
                                     | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    /* Flash Latency: 110MHz @ VOS0 → 5 wait states */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct,
                             FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }

    /* FDCAN 클럭 소스: PLLQ (55 MHz) */
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    PeriphClkInit.FdcanClockSelection  = RCC_FDCANCLKSOURCE_PLL;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
        Error_Handler();
    }
}

/* ==========================================================
 * MX_GPIO_Init
 * ========================================================== */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 모든 GPIO 클럭 활성화 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* LED 초기 출력 LOW */
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED_GPIO_Port,   LED_RED_Pin,   GPIO_PIN_RESET);

    /* LED 출력 설정 */
    GPIO_InitStruct.Pin   = LED_GREEN_Pin | LED_RED_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GREEN_GPIO_Port, &GPIO_InitStruct);
}

/* ==========================================================
 * MX_DMA_Init
 *   ADC1 DMA 클럭 활성화 및 IRQ 설정
 * ========================================================== */
static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* DMA1 Channel1 IRQ 설정 (ADC1) */
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

/* ==========================================================
 * MX_ADC1_Init
 *   ADC1: 8채널 연속 스캔, DMA
 * ========================================================== */
static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    __HAL_RCC_ADC_CLK_ENABLE();

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_ASYNC_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode          = ADC_SCAN_ENABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SEQ_CONV;
    hadc1.Init.LowPowerAutoWait      = DISABLE;
    hadc1.Init.ContinuousConvMode    = ENABLE;
    hadc1.Init.NbrOfConversion       = ADC_NUM_CHANNELS;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
    hadc1.Init.OversamplingMode      = ENABLE;

    hadc1.Init.Oversampling.Ratio                 = ADC_OVERSAMPLING_RATIO_16;
    hadc1.Init.Oversampling.RightBitShift         = ADC_RIGHTBITSHIFT_4;
    hadc1.Init.Oversampling.TriggeredMode         = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
    hadc1.Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;

    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }

    /* 8채널 설정 */
    const uint32_t channels[8] = {
        ADC_CHANNEL_5,  /* PA0 - CH0 전압 */
        ADC_CHANNEL_6,  /* PA1 - CH0 전류 */
        ADC_CHANNEL_7,  /* PA2 - CH1 전압 */
        ADC_CHANNEL_8,  /* PA3 - CH1 전류 */
        ADC_CHANNEL_1,  /* PC0 - CH2 전압 */
        ADC_CHANNEL_2,  /* PC1 - CH2 전류 */
        ADC_CHANNEL_3,  /* PC2 - CH3 전압 */
        ADC_CHANNEL_4,  /* PC3 - CH3 전류 */
    };

    for (uint32_t rank = 0; rank < ADC_NUM_CHANNELS; rank++) {
        sConfig.Channel      = channels[rank];
        sConfig.Rank         = rank + 1U;
        sConfig.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
        sConfig.SingleDiff   = ADC_SINGLE_ENDED;
        sConfig.OffsetNumber = ADC_OFFSET_NONE;
        sConfig.Offset       = 0U;

        if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
            Error_Handler();
        }
    }
}

/* HAL_ADC_MspInit: DMA 연결 (HAL이 호출) */
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        /* DMA 핸들 설정 */
        hdma_adc1.Instance                 = DMA1_Channel1;
        hdma_adc1.Init.Request             = DMA_REQUEST_ADC1;
        hdma_adc1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        hdma_adc1.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_adc1.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        hdma_adc1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
        hdma_adc1.Init.Mode                = DMA_CIRCULAR;
        hdma_adc1.Init.Priority            = DMA_PRIORITY_HIGH;

        if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) {
            Error_Handler();
        }

        __HAL_LINKDMA(hadc, DMA_Handle, hdma_adc1);

        /* ADC IRQ */
        HAL_NVIC_SetPriority(ADC1_IRQn, 6U, 0U);
        HAL_NVIC_EnableIRQ(ADC1_IRQn);
    }
}

/* ==========================================================
 * MX_TIM1_Init
 *   TIM1 PWM 4채널, f=100kHz, 12비트 분해능
 * ========================================================== */
static void MX_TIM1_Init(void)
{
    TIM_ClockConfigTypeDef  sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig      = {0};
    TIM_OC_InitTypeDef      sConfigOC          = {0};
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

    __HAL_RCC_TIM1_CLK_ENABLE();

    /* 기본 타이머 설정: PSC=0, ARR=1099 → 100kHz @ 110MHz */
    htim1.Instance               = TIM1;
    htim1.Init.Prescaler         = 0U;
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim1.Init.Period            = PWM_ARR;
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0U;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_Base_Init(&htim1) != HAL_OK) {
        Error_Handler();
    }

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig);

    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) {
        Error_Handler();
    }

    sMasterConfig.MasterOutputTrigger  = TIM_TRGO_RESET;
    sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    sMasterConfig.MasterSlaveMode      = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig);

    /* PWM 채널 설정 */
    sConfigOC.OCMode       = TIM_OCMODE_PWM1;
    sConfigOC.Pulse        = 0U;
    sConfigOC.OCPolarity   = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode   = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState  = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;

    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3);
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_4);

    /* Break/Deadtime: MOE 활성화 (Advanced timer 필수) */
    sBreakDeadTimeConfig.OffStateRunMode   = TIM_OSSR_DISABLE;
    sBreakDeadTimeConfig.OffStateIDLEMode  = TIM_OSSI_DISABLE;
    sBreakDeadTimeConfig.LockLevel         = TIM_LOCKLEVEL_OFF;
    sBreakDeadTimeConfig.DeadTime          = 0U;
    sBreakDeadTimeConfig.BreakState        = TIM_BREAK_DISABLE;
    sBreakDeadTimeConfig.BreakPolarity     = TIM_BREAKPOLARITY_HIGH;
    sBreakDeadTimeConfig.BreakFilter       = 0U;
    sBreakDeadTimeConfig.Break2State       = TIM_BREAK2_DISABLE;
    sBreakDeadTimeConfig.Break2Polarity    = TIM_BREAK2POLARITY_HIGH;
    sBreakDeadTimeConfig.Break2Filter      = 0U;
    sBreakDeadTimeConfig.AutomaticOutput   = TIM_AUTOMATICOUTPUT_DISABLE;
    HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig);
}

/* HAL_TIM_MspInit: GPIO AF 설정 (HAL이 호출) */
void HAL_TIM_MspInit(TIM_HandleTypeDef *htim)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (htim->Instance == TIM1) {
        __HAL_RCC_TIM1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA8~PA11: TIM1_CH1~CH4, AF1 */
        GPIO_InitStruct.Pin       = GPIO_PIN_8 | GPIO_PIN_9
                                  | GPIO_PIN_10 | GPIO_PIN_11;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

/* ==========================================================
 * MX_FDCAN1_Init
 *   Classic CAN, 500 kbps
 *   FDCAN_CLK = PLLQ = 55 MHz
 *   Prescaler=5, Seg1=14, Seg2=4, SJW=1
 *   TQ = 1/(55MHz/5) = 90.9ns, 1bit ≈ 20TQ ≈ 550kbps
 *   (정확히 500kbps를 원하면 Prescaler=11, Seg1=7, Seg2=2 시도)
 * ========================================================== */
static void MX_FDCAN1_Init(void)
{
    __HAL_RCC_FDCAN_CLK_ENABLE();

    hfdcan1.Instance                  = FDCAN1;
    hfdcan1.Init.ClockDivider         = FDCAN_CLOCK_DIV1;
    hfdcan1.Init.FrameFormat          = FDCAN_FRAME_CLASSIC;
    hfdcan1.Init.Mode                 = FDCAN_MODE_NORMAL;
    hfdcan1.Init.AutoRetransmission   = ENABLE;
    hfdcan1.Init.TransmitPause        = DISABLE;
    hfdcan1.Init.ProtocolException    = ENABLE;

    /* 비트 타이밍: 500 kbps @ 55 MHz
     *  Prescaler=11, Seg1=7, Seg2=2, SJW=1
     *  TQ = 11/55MHz ≈ 200ns
     *  bit = 1+7+2 = 10 TQ → 1/(10*200ns) = 500 kbps */
    hfdcan1.Init.NominalPrescaler     = 11U;
    hfdcan1.Init.NominalSyncJumpWidth = 1U;
    hfdcan1.Init.NominalTimeSeg1      = 7U;
    hfdcan1.Init.NominalTimeSeg2      = 2U;

    /* FD 데이터 비트 타이밍 (Classic CAN이므로 Nominal과 동일) */
    hfdcan1.Init.DataPrescaler        = 11U;
    hfdcan1.Init.DataSyncJumpWidth    = 1U;
    hfdcan1.Init.DataTimeSeg1         = 7U;
    hfdcan1.Init.DataTimeSeg2         = 2U;

    hfdcan1.Init.StdFiltersNbr        = 1U;
    hfdcan1.Init.ExtFiltersNbr        = 0U;
    hfdcan1.Init.TxFifoQueueMode      = FDCAN_TX_FIFO_OPERATION;

    if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK) {
        Error_Handler();
    }
}

/* HAL_FDCAN_MspInit: GPIO AF 설정 + NVIC */
void HAL_FDCAN_MspInit(FDCAN_HandleTypeDef *hfdcan)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hfdcan->Instance == FDCAN1) {
        __HAL_RCC_FDCAN_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();

        /* PD0: FDCAN1_RX, PD1: FDCAN1_TX, AF9 */
        GPIO_InitStruct.Pin       = GPIO_PIN_0 | GPIO_PIN_1;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF9_FDCAN1;
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

        /* FDCAN1 인터럽트: FDCAN1_IT0 (FIFO0 관련) */
        HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn,
                             configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY,
                             0U);
        HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
    }
}

/* ==========================================================
 * MX_IWDG_Init
 *   LSI ≈ 32 kHz, Prescaler=64, Reload=3999
 *   타이아웃 ≈ (3999+1) * 64 / 32000 ≈ 8.0 s
 * ========================================================== */
static void MX_IWDG_Init(void)
{
    hiwdg.Instance       = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
    hiwdg.Init.Reload    = 3999U;
    hiwdg.Init.Window    = IWDG_WINDOW_DISABLE;

    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        Error_Handler();
    }
}

/* ==========================================================
 * Error_Handler
 * ========================================================== */
void Error_Handler(void)
{
    __disable_irq();
    HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
    for (;;) {}
}

/* ==========================================================
 * assert_failed (옵션, USE_FULL_ASSERT 정의 시 사용)
 * ========================================================== */
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    Error_Handler();
}
#endif
