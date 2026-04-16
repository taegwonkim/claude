/**
 * @file    main.c
 * @brief   STM32L552R – 4채널 PID 전압 제어 메인 애플리케이션
 *
 * 동작 개요
 * ──────────
 * 1. SPI1(AD5641) / SPI2(MCP3465R) 초기화 (CubeMX 코드에서 생성)
 * 2. 각 채널의 목표 전압(setpoint) 설정
 * 3. SysTick 기반 10ms 제어 루프:
 *    a. MCP3465R로 4채널 전압 측정
 *    b. 각 채널 PID 계산
 *    c. AD5641로 제어 전압 출력
 *
 * ※ 이 파일은 STM32CubeIDE에서 생성된 main.c에 통합하여 사용하십시오.
 *    USER CODE BEGIN/END 주석 블록 사이에 삽입합니다.
 *
 * STM32CubeMX 설정 항목
 * ─────────────────────
 *  • SPI1 : Transmit Only Master / CPOL=1 / CPHA=1 / 16-bit / NSS=SW
 *           (AD5641: 최대 30 MHz, 일반적으로 Prescaler 4~8 사용)
 *  • SPI2 : Full-Duplex Master  / CPOL=0 / CPHA=0 /  8-bit / NSS=SW
 *           (MCP3465R: 최대 20 MHz)
 *  • GPIO  출력 (Push-Pull, No pull, Initial=High):
 *           PA4, PB0, PC0, PC1 (DAC CS×4), PB12 (ADC CS)
 *  • SysTick: 1ms (기본, HAL_GetTick() 사용)
 */

/* ── 인클루드 ─────────────────────────────────────────────────────── */
#include "main.h"

/* ── CubeMX 생성 핸들 ─────────────────────────────────────────────── */
SPI_HandleTypeDef hspi1;    /* AD5641  (SPI1) */
SPI_HandleTypeDef hspi2;    /* MCP3465R (SPI2) */

/* ── 드라이버 핸들 ────────────────────────────────────────────────── */
static AD5641_HandleTypeDef   g_dac;
static MCP3465R_HandleTypeDef g_adc;

/* ── 제어 상태 ────────────────────────────────────────────────────── */
static ControlState_t g_ctrl;

/* ── 타이밍 변수 ──────────────────────────────────────────────────── */
static uint32_t g_last_ctrl_tick = 0U;

/* ── 함수 선언 ────────────────────────────────────────────────────── */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void App_Init(void);
static void App_ControlLoop(void);

/* ================================================================== */
/*  main()                                                             */
/* ================================================================== */
int main(void)
{
    /* MCU 초기화 ─────────────────────────────────────── */
    HAL_Init();
    SystemClock_Config();

    /* 주변장치 초기화 ────────────────────────────────── */
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_SPI2_Init();

    /* 애플리케이션 초기화 ────────────────────────────── */
    App_Init();

    /* 메인 루프 ──────────────────────────────────────── */
    while (1)
    {
        App_ControlLoop();
    }
}

/* ================================================================== */
/*  애플리케이션 초기화                                                */
/* ================================================================== */
static void App_Init(void)
{
    /* ── 1. DAC 드라이버 설정 ─────────────────────── */
    g_dac.hspi        = &hspi1;
    g_dac.cs_port[0]  = DAC_CS_CH0_PORT;  g_dac.cs_pin[0] = DAC_CS_CH0_PIN;
    g_dac.cs_port[1]  = DAC_CS_CH1_PORT;  g_dac.cs_pin[1] = DAC_CS_CH1_PIN;
    g_dac.cs_port[2]  = DAC_CS_CH2_PORT;  g_dac.cs_pin[2] = DAC_CS_CH2_PIN;
    g_dac.cs_port[3]  = DAC_CS_CH3_PORT;  g_dac.cs_pin[3] = DAC_CS_CH3_PIN;
    AD5641_Init(&g_dac);

    /* ── 2. ADC 드라이버 설정 ─────────────────────── */
    g_adc.hspi    = &hspi2;
    g_adc.cs_port = ADC_CS_PORT;
    g_adc.cs_pin  = ADC_CS_PIN;
    MCP3465R_Init(&g_adc);

    /* ── 3. 목표 전압 설정 ────────────────────────── */
    g_ctrl.setpoint[0] = DEFAULT_SETPOINT_CH0;
    g_ctrl.setpoint[1] = DEFAULT_SETPOINT_CH1;
    g_ctrl.setpoint[2] = DEFAULT_SETPOINT_CH2;
    g_ctrl.setpoint[3] = DEFAULT_SETPOINT_CH3;

    /* ── 4. PID 초기화 (채널별) ───────────────────── */
    for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++)
    {
        PID_Init(&g_ctrl.pid[ch],
                 PID_KP_DEFAULT,
                 PID_KI_DEFAULT,
                 PID_KD_DEFAULT,
                 VOLTAGE_MIN,
                 VOLTAGE_MAX,
                 CTRL_SAMPLE_SEC);

        PID_SetSetpoint(&g_ctrl.pid[ch], g_ctrl.setpoint[ch]);

        g_ctrl.measured[ch] = 0.0f;
        g_ctrl.output[ch]   = g_ctrl.setpoint[ch];
        g_ctrl.error[ch]    = 0.0f;
    }

    g_last_ctrl_tick = HAL_GetTick();
}

/* ================================================================== */
/*  제어 루프 (10ms마다 실행)                                          */
/* ================================================================== */
static void App_ControlLoop(void)
{
    uint32_t now = HAL_GetTick();

    /* CTRL_SAMPLE_MS 주기 확인 */
    if ((now - g_last_ctrl_tick) < CTRL_SAMPLE_MS)
    {
        return;
    }
    g_last_ctrl_tick = now;

    /* ── 1. 4채널 전압 측정 ───────────────────────── */
    MCP3465R_ReadAllChannels(&g_adc, g_ctrl.measured);

    /* ── 2. 채널별 PID 계산 및 DAC 출력 ─────────── */
    for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++)
    {
        g_ctrl.error[ch]  = g_ctrl.setpoint[ch] - g_ctrl.measured[ch];
        g_ctrl.output[ch] = PID_Compute(&g_ctrl.pid[ch], g_ctrl.measured[ch]);
        AD5641_SetVoltage(&g_dac, ch, g_ctrl.output[ch]);
    }
}

/* ================================================================== */
/*  setpoint 외부 변경 API (UART, USB CDC 등 상위 통신에서 호출)       */
/* ================================================================== */

/**
 * @brief  런타임 중 특정 채널의 목표 전압 변경
 * @param  channel  채널 번호 (0~3)
 * @param  voltage  목표 전압 [V] (0.5~4.5 V)
 */
void App_SetChannelVoltage(uint8_t channel, float voltage)
{
    if (channel >= NUM_CHANNELS) return;
    g_ctrl.setpoint[channel] = voltage;
    PID_SetSetpoint(&g_ctrl.pid[channel], voltage);
    PID_Reset(&g_ctrl.pid[channel]);   /* 목표값 변경 시 적분항 초기화 */
}

/**
 * @brief  런타임 중 특정 채널의 PID 게인 변경
 * @param  channel  채널 번호 (0~3)
 * @param  kp, ki, kd  새 게인값
 */
void App_SetPIDGains(uint8_t channel, float kp, float ki, float kd)
{
    if (channel >= NUM_CHANNELS) return;
    g_ctrl.pid[channel].kp = kp;
    g_ctrl.pid[channel].ki = ki;
    g_ctrl.pid[channel].kd = kd;
    PID_Reset(&g_ctrl.pid[channel]);
}

/**
 * @brief  현재 제어 상태 조회 (디버깅/모니터링용)
 * @return g_ctrl 포인터
 */
const ControlState_t *App_GetState(void)
{
    return &g_ctrl;
}

/* ================================================================== */
/*  CubeMX 생성 초기화 함수 (실제 프로젝트에서는 CubeMX가 생성)        */
/*  아래 코드는 STM32L552RET6 기준 예시입니다.                         */
/* ================================================================== */

/**
 * @brief  시스템 클럭 설정
 *         MSI 4MHz → PLL → 110MHz (STM32L5 최대)
 *         실제 설정은 CubeMX에서 생성된 코드를 사용하십시오.
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef       RCC_OscInit = {0};
    RCC_ClkInitTypeDef       RCC_ClkInit = {0};

    /* MSI 오실레이터 활성화 (4 MHz) */
    RCC_OscInit.OscillatorType      = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInit.MSIState            = RCC_MSI_ON;
    RCC_OscInit.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    RCC_OscInit.MSIClockRange       = RCC_MSIRANGE_6;  /* 4 MHz */
    RCC_OscInit.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInit.PLL.PLLSource       = RCC_PLLSOURCE_MSI;
    RCC_OscInit.PLL.PLLM            = 1U;
    RCC_OscInit.PLL.PLLN            = 55U;
    RCC_OscInit.PLL.PLLP            = RCC_PLLP_DIV7;
    RCC_OscInit.PLL.PLLQ            = RCC_PLLQ_DIV2;
    RCC_OscInit.PLL.PLLR            = RCC_PLLR_DIV2;  /* PLLCLK = 110 MHz */
    HAL_RCC_OscConfig(&RCC_OscInit);

    /* 버스 클럭 설정 */
    RCC_ClkInit.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInit.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInit.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInit.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInit.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInit, FLASH_LATENCY_5);
}

/**
 * @brief  GPIO 초기화
 *         CS 핀들: Output Push-Pull, No pull, Speed=Low, 초기값=High
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_Init = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* 모든 CS 핀을 HIGH로 초기화 */
    HAL_GPIO_WritePin(GPIOA, DAC_CS_CH0_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, DAC_CS_CH1_PIN | ADC_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOC, DAC_CS_CH2_PIN | DAC_CS_CH3_PIN, GPIO_PIN_SET);

    /* PA4 - DAC CS CH0 */
    GPIO_Init.Pin   = DAC_CS_CH0_PIN;
    GPIO_Init.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_Init.Pull  = GPIO_NOPULL;
    GPIO_Init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_Init);

    /* PB0 - DAC CS CH1, PB12 - ADC CS */
    GPIO_Init.Pin = DAC_CS_CH1_PIN | ADC_CS_PIN;
    HAL_GPIO_Init(GPIOB, &GPIO_Init);

    /* PC0 - DAC CS CH2, PC1 - DAC CS CH3 */
    GPIO_Init.Pin = DAC_CS_CH2_PIN | DAC_CS_CH3_PIN;
    HAL_GPIO_Init(GPIOC, &GPIO_Init);

    /* SPI1 핀: PA5(SCK), PA7(MOSI) */
    GPIO_Init.Pin       = GPIO_PIN_5 | GPIO_PIN_7;
    GPIO_Init.Mode      = GPIO_MODE_AF_PP;
    GPIO_Init.Pull      = GPIO_NOPULL;
    GPIO_Init.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_Init.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_Init);

    /* SPI2 핀: PB13(SCK), PB14(MISO), PB15(MOSI) */
    GPIO_Init.Pin       = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_Init.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &GPIO_Init);
}

/**
 * @brief  SPI1 초기화 (AD5641 DAC)
 *         Mode 3 (CPOL=High, CPHA=2Edge), 8-bit, Transmit Only Master
 *         Prescaler=8 → 110MHz/8 = 13.75 MHz (AD5641 최대 30MHz 이내)
 */
static void MX_SPI1_Init(void)
{
    __HAL_RCC_SPI1_CLK_ENABLE();

    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_1LINE;  /* MOSI only */
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_HIGH;    /* CPOL=1 */
    hspi1.Init.CLKPhase          = SPI_PHASE_2EDGE;      /* CPHA=1 */
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;
    HAL_SPI_Init(&hspi1);
}

/**
 * @brief  SPI2 초기화 (MCP3465R ADC)
 *         Mode 0 (CPOL=Low, CPHA=1Edge), 8-bit, Full-Duplex Master
 *         Prescaler=8 → 110MHz/8 = 13.75 MHz (MCP3465R 최대 20MHz 이내)
 */
static void MX_SPI2_Init(void)
{
    __HAL_RCC_SPI2_CLK_ENABLE();

    hspi2.Instance               = SPI2;
    hspi2.Init.Mode              = SPI_MODE_MASTER;
    hspi2.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity       = SPI_POLARITY_LOW;     /* CPOL=0 */
    hspi2.Init.CLKPhase          = SPI_PHASE_1EDGE;      /* CPHA=0 */
    hspi2.Init.NSS               = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi2.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;
    HAL_SPI_Init(&hspi2);
}

/* ================================================================== */
/*  HAL 에러 핸들러                                                    */
/* ================================================================== */
void Error_Handler(void)
{
    __disable_irq();
    while (1) { /* 디버거 브레이크포인트 설정 권장 */ }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    /* 파일명/라인 번호를 UART 등으로 출력 */
    (void)file; (void)line;
}
#endif
