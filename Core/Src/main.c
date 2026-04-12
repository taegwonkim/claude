/**
 * @file    main.c
 * @brief   STM32L552R PID 전압 제어 — 메인 진입점
 *
 * STM32CubeIDE / CubeMX 자동 생성 부분(SystemClock, HAL Init, 주변장치 Init)과
 * 사용자 코드를 함께 포함합니다.
 * /* USER CODE BEGIN/END */ 영역 안에만 코드를 작성하면 CubeMX 재생성 후에도 보존됩니다.
 *
 * 주변장치 초기화 흐름:
 *   HAL_Init() → SystemClock_Config() → MX_GPIO_Init()
 *   → MX_SPI1_Init() → MX_SPI2_Init() → MX_USART1_UART_Init()
 *   → MX_FREERTOS_Init() → osKernelStart()
 */

/* USER CODE BEGIN Includes */
#include "main.h"
#include "cmsis_os2.h"
/* USER CODE END Includes */

/* ------------------------------------------------------------------ */
/*  HAL 핸들 (CubeMX 생성)                                              */
/* ------------------------------------------------------------------ */
SPI_HandleTypeDef  hspi1;
SPI_HandleTypeDef  hspi2;
UART_HandleTypeDef huart1;

/* ------------------------------------------------------------------ */
/*  내부 함수 프로토타입                                                   */
/* ------------------------------------------------------------------ */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART1_UART_Init(void);
extern void MX_FREERTOS_Init(void);  /* freertos.c에 정의 */

/* ================================================================== */
/*  main                                                               */
/* ================================================================== */
int main(void)
{
    /* HAL 라이브러리 초기화 (SysTick 설정 등) */
    HAL_Init();

    /* 시스템 클럭 설정: 110 MHz (MSI PLL) */
    SystemClock_Config();

    /* 주변장치 초기화 */
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_SPI2_Init();
    MX_USART1_UART_Init();

    /* FreeRTOS 객체 및 태스크 생성 */
    MX_FREERTOS_Init();

    /* RTOS 스케줄러 시작 (이 함수는 정상적으로는 반환하지 않음) */
    osKernelStart();

    /* 스케줄러 실패 시 */
    Error_Handler();
    while (1) {}
}

/* ================================================================== */
/*  시스템 클럭 설정                                                     */
/*  SYSCLK = 110 MHz  (MSI 4MHz → PLL → 110 MHz)                     */
/*  HCLK   = 110 MHz, APB1 = 55 MHz, APB2 = 110 MHz                  */
/* ================================================================== */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef       RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef       RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit     = {0};

    /* 전원 범위 설정: Range 0 (최고 성능, 110 MHz 지원) */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0);

    /* MSI → PLL 설정 */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState            = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.MSIClockRange       = RCC_MSIRANGE_6; /* 4 MHz */
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_MSI;
    RCC_OscInitStruct.PLL.PLLM            = 1;
    RCC_OscInitStruct.PLL.PLLN            = 55;  /* VCO = 4 × 55 = 220 MHz */
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV2;  /* unused */
    RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;  /* unused */
    RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;  /* SYSCLK = 110 MHz */
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /* 버스 클럭 분주 */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK |
                                   RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  = 110 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;     /* APB1  =  55 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;     /* APB2  = 110 MHz */
    /* Flash Latency: 110 MHz @ 1.28V → 5 wait states */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }

    /* USART1 클럭 소스: PCLK2 */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
    PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
        Error_Handler();
    }
}

/* ================================================================== */
/*  GPIO 초기화                                                          */
/* ================================================================== */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 클럭 활성화 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* ---- CS 핀 (출력, 초기 HIGH) ---- */
    /* DAC CS: PB0, PB1, PB2, PB10 */
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Pin   = CS_DAC_CH0_PIN | CS_DAC_CH1_PIN |
                            CS_DAC_CH2_PIN | CS_DAC_CH3_PIN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOB,
                      CS_DAC_CH0_PIN | CS_DAC_CH1_PIN |
                      CS_DAC_CH2_PIN | CS_DAC_CH3_PIN,
                      GPIO_PIN_SET);

    /* ADC CS: PC7 */
    GPIO_InitStruct.Pin = CS_ADC_PIN;
    HAL_GPIO_Init(CS_ADC_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(CS_ADC_PORT, CS_ADC_PIN, GPIO_PIN_SET);

    /* ---- ADC INT 핀 (입력, Pull-Up, EXTI 하강 에지) ---- */
    GPIO_InitStruct.Pin  = ADC_INT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(ADC_INT_PORT, &GPIO_InitStruct);

    /* EXTI Line0 NVIC 설정 (우선순위 5: FreeRTOS syscall 임계값보다 낮게) */
    HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

/* ================================================================== */
/*  SPI1 초기화 — AD5641 DAC (전송 전용 마스터)                          */
/* ================================================================== */
static void MX_SPI1_Init(void)
{
    /* PA5: SPI1_SCK, PA7: SPI1_MOSI */
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_1LINE;   /* 전송 전용 */
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;      /* CPOL=0 */
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;       /* CPHA=0 */
    hspi1.Init.NSS               = SPI_NSS_SOFT;          /* SW CS 제어 */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; /* ~6.9 MHz */
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        Error_Handler();
    }
}

/* ================================================================== */
/*  SPI2 초기화 — MCP3465R ADC (전이중 마스터)                           */
/* ================================================================== */
static void MX_SPI2_Init(void)
{
    /* PB13: SPI2_SCK, PB14: SPI2_MISO, PB15: SPI2_MOSI */
    hspi2.Instance               = SPI2;
    hspi2.Init.Mode              = SPI_MODE_MASTER;
    hspi2.Init.Direction         = SPI_DIRECTION_2LINES;  /* 전이중 */
    hspi2.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity       = SPI_POLARITY_LOW;      /* CPOL=0 */
    hspi2.Init.CLKPhase          = SPI_PHASE_1EDGE;       /* CPHA=0 */
    hspi2.Init.NSS               = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; /* ~3.4 MHz */
    hspi2.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    if (HAL_SPI_Init(&hspi2) != HAL_OK) {
        Error_Handler();
    }
}

/* ================================================================== */
/*  USART1 초기화 — 디버그 출력                                          */
/* ================================================================== */
static void MX_USART1_UART_Init(void)
{
    /* PA9: TX, PA10: RX */
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

/* ================================================================== */
/*  오류 핸들러                                                           */
/* ================================================================== */
void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        /* 디버거 중단점 위치 */
    }
}

/* ================================================================== */
/*  Assertion 실패 핸들러 (assert_param 활성화 시 사용)                   */
/* ================================================================== */
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    Error_Handler();
}
#endif
