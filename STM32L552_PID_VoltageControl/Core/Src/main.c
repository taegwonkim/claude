/* main.c - STM32L552 4채널 PID 전압 제어
 *
 * ============================================================
 * CubeMX 설정 요약
 * ============================================================
 * [RCC]
 *   HSI16 ON, PLL: M=4, N=55, R=2 → SYSCLK=110MHz
 *   PLLQ=4 → 55MHz (FDCAN 클럭 소스)
 *
 * [SPI1] - AD5641 DAC × 4
 *   Mode=Master Full-Duplex, DataSize=16bit
 *   CPOL=HIGH(1), CPHA=1EDGE(0) → SPI Mode 2
 *   NSS=Software, Baud=PCLK2/8=13.75MHz
 *   SCK=PA5(AF5), MOSI=PA7(AF5)
 *   CS: PB0~PB3 (GPIO Output, init HIGH)
 *
 * [SPI2] - MCP3465R ADC × 1
 *   Mode=Master Full-Duplex, DataSize=8bit
 *   CPOL=LOW(0), CPHA=1EDGE(0) → SPI Mode 0
 *   NSS=Software, Baud=PCLK1/4=13.75MHz
 *   SCK=PB13(AF5), MISO=PB14(AF5), MOSI=PB15(AF5)
 *   CS:  PB4 (GPIO Output, init HIGH)
 *   /IRQ:PB5 (GPIO Input, Pull-up, EXTI falling edge)
 *
 * [FDCAN1]
 *   Classic CAN 500kbps, TX=PD1(AF9), RX=PD0(AF9)
 *
 * [IWDG]
 *   PSC=64, Reload=3999 → ~8s timeout
 *
 * [GPIO]
 *   PC6=LED_RED, PC7=LED_GREEN (Output PP)
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
SPI_HandleTypeDef   hspi1;
SPI_HandleTypeDef   hspi2;
FDCAN_HandleTypeDef hfdcan1;
IWDG_HandleTypeDef  hiwdg;

/* ==========================================================
 * 내부 초기화 함수 프로토타입
 * ========================================================== */
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_FDCAN1_Init(void);
static void MX_IWDG_Init(void);

/* ==========================================================
 * main
 * ========================================================== */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_SPI2_Init();
    MX_FDCAN1_Init();
    MX_IWDG_Init();

    /* 채널 제어 초기화 (PID 파라미터 설정) */
    ChannelCtrl_Init();

    /* FDCAN 필터 및 수신 인터럽트 설정 */
    if (FDCANComm_Init() != HAL_OK) {
        Error_Handler();
    }

    /* AD5641 DAC 초기화: 모든 채널 출력=0 */
    PWMOut_Start();

    /* MCP3465R ADC 초기화 및 연속 스캔 시작 */
    if (ADCMeas_Init() != HAL_OK) {
        Error_Handler();
    }

    /* 초기 채널 목표 전압 설정 (CAN 명령으로 런타임 변경 가능) */
    ChannelCtrl_SetVoltage(0,  5000U);   /* CH0: 5.0V */
    ChannelCtrl_SetVoltage(1,  3300U);   /* CH1: 3.3V */
    ChannelCtrl_SetVoltage(2,  9000U);   /* CH2: 9.0V */
    ChannelCtrl_SetVoltage(3, 12000U);   /* CH3: 12.0V */

    ChannelCtrl_Enable(0, 1U);
    ChannelCtrl_Enable(1, 1U);
    ChannelCtrl_Enable(2, 1U);
    ChannelCtrl_Enable(3, 1U);

    /* FreeRTOS 태스크 생성 및 스케줄러 시작 */
    AppTasks_Init();
    vTaskStartScheduler();

    for (;;) {}
}

/* ==========================================================
 * SystemClock_Config: HSI16 → PLL → 110 MHz
 * ========================================================== */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0) != HAL_OK) {
        Error_Handler();
    }

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI
                                          | RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.LSIState            = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    /* HSI16/4 × 55 / R=2 = 110 MHz */
    /* HSI16/4 × 55 / Q=4 =  55 MHz (FDCAN) */
    RCC_OscInitStruct.PLL.PLLM            = 4U;
    RCC_OscInitStruct.PLL.PLLN            = 55U;
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV4;
    RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;  /* 55 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;  /* 110 MHz */

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
        Error_Handler();

    /* FDCAN 클럭: PLLQ = 55 MHz */
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    PeriphClkInit.FdcanClockSelection  = RCC_FDCANCLKSOURCE_PLL;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
        Error_Handler();
}

/* ==========================================================
 * MX_GPIO_Init
 * ========================================================== */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* -------------------------------------------------------
     * DAC CS 핀: PB0~PB3 초기 HIGH (비활성)
     * ------------------------------------------------------- */
    HAL_GPIO_WritePin(GPIOB,
                      DAC_CS0_Pin | DAC_CS1_Pin | DAC_CS2_Pin | DAC_CS3_Pin,
                      GPIO_PIN_SET);

    GPIO_InitStruct.Pin   = DAC_CS0_Pin | DAC_CS1_Pin | DAC_CS2_Pin | DAC_CS3_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* -------------------------------------------------------
     * ADC CS 핀: PB4 초기 HIGH
     * ------------------------------------------------------- */
    HAL_GPIO_WritePin(ADC_CS_Port, ADC_CS_Pin, GPIO_PIN_SET);

    GPIO_InitStruct.Pin   = ADC_CS_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ADC_CS_Port, &GPIO_InitStruct);

    /* -------------------------------------------------------
     * ADC /IRQ 핀: PB5 입력, 풀업, 하강 엣지 EXTI
     * ------------------------------------------------------- */
    GPIO_InitStruct.Pin   = ADC_IRQ_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(ADC_IRQ_Port, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI5_IRQn,
                         configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY, 0U);
    HAL_NVIC_EnableIRQ(EXTI5_IRQn);

    /* -------------------------------------------------------
     * LED 핀: PC6(RED), PC7(GREEN)
     * ------------------------------------------------------- */
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port,
                      LED_GREEN_Pin | LED_RED_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin   = LED_GREEN_Pin | LED_RED_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GREEN_GPIO_Port, &GPIO_InitStruct);
}

/* ==========================================================
 * MX_SPI1_Init - AD5641 DAC
 *   DataSize=16bit, Mode2(CPOL=HIGH, CPHA=1EDGE)
 *   Baud=PCLK2/8=13.75MHz
 * ========================================================== */
static void MX_SPI1_Init(void)
{
    __HAL_RCC_SPI1_CLK_ENABLE();

    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_16BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_HIGH;   /* CPOL=1 */
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;     /* CPHA=0 → Mode 2 */
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; /* 110MHz/8=13.75MHz */
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial     = 7U;
    hspi1.Init.CRCLength         = SPI_CRC_LENGTH_DATASIZE;
    hspi1.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;

    if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
}

/* HAL_SPI_MspInit: SPI1 GPIO AF 설정 */
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hspi->Instance == SPI1) {
        __HAL_RCC_SPI1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA5=SCK, PA7=MOSI  AF5 */
        GPIO_InitStruct.Pin       = GPIO_PIN_5 | GPIO_PIN_7;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        /* MISO(PA6): 사용하지 않으나 혼선 방지를 위해 AF 설정 */
        GPIO_InitStruct.Pin       = GPIO_PIN_6;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
    else if (hspi->Instance == SPI2) {
        __HAL_RCC_SPI2_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /* PB13=SCK, PB14=MISO, PB15=MOSI  AF5 */
        GPIO_InitStruct.Pin       = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
}

/* ==========================================================
 * MX_SPI2_Init - MCP3465R ADC
 *   DataSize=8bit, Mode0(CPOL=LOW, CPHA=1EDGE)
 *   Baud=PCLK1/4=13.75MHz
 * ========================================================== */
static void MX_SPI2_Init(void)
{
    __HAL_RCC_SPI2_CLK_ENABLE();

    hspi2.Instance               = SPI2;
    hspi2.Init.Mode              = SPI_MODE_MASTER;
    hspi2.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity       = SPI_POLARITY_LOW;    /* CPOL=0 */
    hspi2.Init.CLKPhase          = SPI_PHASE_1EDGE;     /* CPHA=0 → Mode 0 */
    hspi2.Init.NSS               = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4; /* 55MHz/4=13.75MHz */
    hspi2.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial     = 7U;
    hspi2.Init.CRCLength         = SPI_CRC_LENGTH_DATASIZE;
    hspi2.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;

    if (HAL_SPI_Init(&hspi2) != HAL_OK) Error_Handler();
}

/* ==========================================================
 * MX_FDCAN1_Init: Classic CAN 500kbps, PLLQ=55MHz
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
    /* 55MHz / 11 = 5MHz TQ,  1bit=10TQ → 500kbps */
    hfdcan1.Init.NominalPrescaler     = 11U;
    hfdcan1.Init.NominalSyncJumpWidth = 1U;
    hfdcan1.Init.NominalTimeSeg1      = 7U;
    hfdcan1.Init.NominalTimeSeg2      = 2U;
    hfdcan1.Init.DataPrescaler        = 11U;
    hfdcan1.Init.DataSyncJumpWidth    = 1U;
    hfdcan1.Init.DataTimeSeg1         = 7U;
    hfdcan1.Init.DataTimeSeg2         = 2U;
    hfdcan1.Init.StdFiltersNbr        = 1U;
    hfdcan1.Init.ExtFiltersNbr        = 0U;
    hfdcan1.Init.TxFifoQueueMode      = FDCAN_TX_FIFO_OPERATION;

    if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK) Error_Handler();
}

void HAL_FDCAN_MspInit(FDCAN_HandleTypeDef *hfdcan)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (hfdcan->Instance == FDCAN1) {
        __HAL_RCC_FDCAN_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();
        GPIO_InitStruct.Pin       = GPIO_PIN_0 | GPIO_PIN_1;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF9_FDCAN1;
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
        HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn,
                             configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY, 0U);
        HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
    }
}

/* ==========================================================
 * MX_IWDG_Init: ~8s timeout
 * ========================================================== */
static void MX_IWDG_Init(void)
{
    hiwdg.Instance       = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
    hiwdg.Init.Reload    = 3999U;
    hiwdg.Init.Window    = IWDG_WINDOW_DISABLE;
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) Error_Handler();
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

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
    Error_Handler();
}
#endif
