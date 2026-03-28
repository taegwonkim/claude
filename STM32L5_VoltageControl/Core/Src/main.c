/* =========================================================
 * main.c - STM32L5 FreeRTOS 전압 제어 시스템
 *
 * STM32CubeIDE + CubeMX 기반 프로젝트 메인 파일
 * HAL + FreeRTOS(CMSIS-OS v2) 사용
 * =========================================================*/

/* USER CODE BEGIN Includes */
#include "main.h"
#include "cmsis_os2.h"
#include "voltage_control.h"
#include "uart_protocol.h"
/* USER CODE END Includes */

/* ----- HAL 핸들 (CubeMX 생성) ----- */
SPI_HandleTypeDef  hspi1;
TIM_HandleTypeDef  htim2;
UART_HandleTypeDef huart1;
DMA_HandleTypeDef  hdma_usart1_rx;
DMA_HandleTypeDef  hdma_usart1_tx;

/* ----- 함수 선언 ----- */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM2_Init(void);

/* FreeRTOS 초기화 (freertos.c에서 구현) */
extern void MX_FREERTOS_Init(void);

/* =========================================================
 * main()
 * =========================================================*/
int main(void)
{
    /* MCU 초기화 */
    HAL_Init();
    SystemClock_Config();

    /* 주변장치 초기화 */
    MX_GPIO_Init();
    MX_DMA_Init();       /* DMA는 UART 전에 반드시 초기화 */
    MX_USART1_UART_Init();
    MX_SPI1_Init();
    MX_TIM2_Init();

    /* FreeRTOS 커널 초기화 */
    osKernelInitialize();

    /* FreeRTOS 객체 생성 (freertos.c) */
    MX_FREERTOS_Init();

    /* 스케줄러 시작 (이후로 돌아오지 않음) */
    osKernelStart();

    /* 여기에 도달하면 에러 */
    Error_Handler();
    while (1) {}
}

/* =========================================================
 * 시스템 클럭 설정
 * HSI16 → PLL → SYSCLK 80MHz
 * =========================================================*/
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* 전원 설정: 80MHz에서는 Range 1 (1.2V) 필요 */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* HSI16 + PLL 설정 */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM            = 1;    /* 16MHz / 1 = 16MHz */
    RCC_OscInitStruct.PLL.PLLN            = 10;   /* 16MHz × 10 = 160MHz */
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2; /* 160MHz / 2 = 80MHz */

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /* 버스 클럭 설정 */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK
                                     | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1
                                     | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK = 80MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;     /* APB1 = 80MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;     /* APB2 = 80MHz */

    /* Flash 대기 사이클: 80MHz에서 4 WS 필요 */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
        Error_Handler();
    }
}

/* =========================================================
 * GPIO 초기화
 * =========================================================*/
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 클럭 활성화 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* ----- MCP3465R CS 핀 (PB6, 출력, 초기 HIGH) ----- */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); /* CS 비활성화 */
    GPIO_InitStruct.Pin   = GPIO_PIN_6;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* ----- MCP3465R IRQ 핀 (PB7, 입력, EXTI, 하강엣지) ----- */
    GPIO_InitStruct.Pin   = GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_IT_FALLING; /* 하강엣지에서 인터럽트 */
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* EXTI7 NVIC 설정 */
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

    /* ----- 상태 LED (PC7, 출력) ----- */
    GPIO_InitStruct.Pin   = GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
}

/* =========================================================
 * DMA 초기화 (UART TX/RX DMA)
 * 반드시 UART 초기화 전에 호출!
 * =========================================================*/
static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* DMA1 Channel1: USART1_RX */
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    /* DMA1 Channel2: USART1_TX */
    HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
}

/* =========================================================
 * USART1 초기화 (115200, 8N1, DMA)
 * =========================================================*/
static void MX_USART1_UART_Init(void)
{
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

    /* NVIC */
    HAL_NVIC_SetPriority(USART1_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* =========================================================
 * HAL_UART_MspInit - UART GPIO + DMA 연결 (HAL 콜백)
 * =========================================================*/
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (huart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA9 = TX, PA10 = RX */
        GPIO_InitStruct.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* DMA RX */
        hdma_usart1_rx.Instance                 = DMA1_Channel1;
        hdma_usart1_rx.Init.Request             = DMA_REQUEST_USART1_RX;
        hdma_usart1_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        hdma_usart1_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_usart1_rx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_usart1_rx.Init.Mode                = DMA_CIRCULAR; /* 원형 버퍼 */
        hdma_usart1_rx.Init.Priority            = DMA_PRIORITY_MEDIUM;
        HAL_DMA_Init(&hdma_usart1_rx);
        __HAL_LINKDMA(huart, hdmarx, hdma_usart1_rx);

        /* DMA TX */
        hdma_usart1_tx.Instance                 = DMA1_Channel2;
        hdma_usart1_tx.Init.Request             = DMA_REQUEST_USART1_TX;
        hdma_usart1_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
        hdma_usart1_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_usart1_tx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_usart1_tx.Init.Mode                = DMA_NORMAL;
        hdma_usart1_tx.Init.Priority            = DMA_PRIORITY_LOW;
        HAL_DMA_Init(&hdma_usart1_tx);
        __HAL_LINKDMA(huart, hdmatx, hdma_usart1_tx);
    }
}

/* =========================================================
 * SPI1 초기화 (MCP3465R: Mode 0,0 / 10MHz)
 * =========================================================*/
static void MX_SPI1_Init(void)
{
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;   /* CPOL = 0 */
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;    /* CPHA = 0 → Mode 0 */
    hspi1.Init.NSS               = SPI_NSS_SOFT;       /* 소프트웨어 CS 제어 */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; /* 80MHz/8 = 10MHz */
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;

    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(SPI1_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(SPI1_IRQn);
}

/* =========================================================
 * HAL_SPI_MspInit - SPI GPIO 설정 (HAL 콜백)
 * =========================================================*/
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hspi->Instance == SPI1) {
        __HAL_RCC_SPI1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA5=SCK, PA6=MISO, PA7=MOSI (AF5) */
        GPIO_InitStruct.Pin       = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

/* =========================================================
 * TIM2 초기화 (PWM 4채널, 19.5kHz)
 * ARR = 4095, PSC = 0 → f = 80MHz / 4096 ≈ 19.5kHz
 * =========================================================*/
static void MX_TIM2_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 0;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 4095; /* ARR = 4095 → 12비트 분해능 */
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) {
        Error_Handler();
    }

    /* PWM 채널 설정 (4채널 동일) */
    sConfigOC.OCMode       = TIM_OCMODE_PWM1;
    sConfigOC.Pulse        = 0;    /* 초기 듀티 = 0 */
    sConfigOC.OCPolarity   = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode   = TIM_OCFAST_DISABLE;
    sConfigOC.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCIdleState  = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;

    HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3);
    HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4);
}

/* =========================================================
 * HAL_TIM_PWM_MspInit - TIM2 GPIO 설정 (HAL 콜백)
 * =========================================================*/
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (htim->Instance == TIM2) {
        __HAL_RCC_TIM2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA0=CH1, PA1=CH2, PA2=CH3, PA3=CH4 (AF1) */
        GPIO_InitStruct.Pin       = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

/* =========================================================
 * 에러 핸들러
 * =========================================================*/
void Error_Handler(void)
{
    __disable_irq();
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET); /* LED 점등 */
    while (1) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_7);
        HAL_Delay(200);
    }
}

/* =========================================================
 * Assertion 핸들러 (HAL Assert 활성화 시)
 * =========================================================*/
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    Error_Handler();
}
#endif
