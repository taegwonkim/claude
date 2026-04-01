/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  *                   Voltage Feedback Current Monitor System
  *
  *  STM32L552RET6 + FreeRTOS
  *  - 4채널 전압 출력 (AD5641 DAC × 4, SPI2)
  *  - 4채널 전압/전류 측정 (MCP3465R ADC × 4, SPI1)
  *  - PI 피드백 제어로 출력 전압 정밀 유지
  *  - 단락/단선 감지 (전류 모니터링)
  *  - UART1 + FDCAN1 주기적 보고
  *  - UART4 디버그 출력
  *  - SYSCLK = 100MHz (HSE 8MHz + PLL)
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os2.h"
#include <string.h>

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef   hspi1;    /* ADC - MCP3465R */
SPI_HandleTypeDef   hspi2;    /* DAC - AD5641 */
UART_HandleTypeDef  huart1;   /* PC Communication */
UART_HandleTypeDef  huart4;   /* Debug Port */
FDCAN_HandleTypeDef hfdcan1;  /* CAN Bus */
DMA_HandleTypeDef   hdma_usart1_rx;
DMA_HandleTypeDef   hdma_usart1_tx;
TIM_HandleTypeDef   htim6;    /* HAL Timebase (not SysTick) */

/* External functions --------------------------------------------------------*/
extern void MX_FREERTOS_Init(void);
extern void UART1_IDLE_Callback(void);

/* Private function prototypes -----------------------------------------------*/
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_UART4_Init(void);
static void MX_FDCAN1_Init(void);

/* -------------------------------------------------------------------------- */
/*                              Main Function                                 */
/* -------------------------------------------------------------------------- */

int main(void)
{
    /* MCU Configuration */
    HAL_Init();

    /* Configure the system clock to 100 MHz */
    SystemClock_Config();

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_DMA_Init();     /* DMA must be initialized before UART */
    MX_SPI1_Init();
    MX_SPI2_Init();
    MX_USART1_UART_Init();
    MX_UART4_Init();
    MX_FDCAN1_Init();

    /* Start FDCAN */
    HAL_FDCAN_Start(&hfdcan1);

    /* Init FreeRTOS kernel and tasks */
    osKernelInitialize();
    MX_FREERTOS_Init();

    /* Start scheduler - should never return */
    osKernelStart();

    /* We should never get here as control is now taken by the scheduler */
    while (1) {
    }
}

/* -------------------------------------------------------------------------- */
/*                       System Clock Configuration                           */
/* -------------------------------------------------------------------------- */

/**
  * @brief  System Clock Configuration
  *         HSE = 8 MHz → PLL → SYSCLK = 100 MHz
  *         AHB = 100 MHz, APB1 = 100 MHz, APB2 = 100 MHz
  */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Configure the main internal regulator output voltage */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0) != HAL_OK) {
        Error_Handler();
    }

    /* Configure HSE and PLL */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 1;    /* /1: 8 MHz */
    RCC_OscInitStruct.PLL.PLLN       = 25;   /* ×25: 200 MHz VCO */
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ       = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR       = RCC_PLLR_DIV2;  /* /2: 100 MHz */
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /* Configure clock buses */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK   | RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;     /* 100 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;       /* 100 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;       /* 100 MHz */

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
        Error_Handler();
    }
}

/* -------------------------------------------------------------------------- */
/*                       Peripheral Initialization                            */
/* -------------------------------------------------------------------------- */

/**
  * @brief  SPI1 Initialization - ADC (MCP3465R)
  *         Mode 0 (CPOL=0, CPHA=0), 8-bit, MSB first
  *         Prescaler = 16 → 100MHz/16 = 6.25MHz
  */
static void MX_SPI1_Init(void)
{
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;     /* CPOL = 0 */
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;      /* CPHA = 0 */
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;

    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief  SPI2 Initialization - DAC (AD5641)
  *         Mode 3 (CPOL=1, CPHA=1), 8-bit, MSB first
  *         Prescaler = 8 → 100MHz/8 = 12.5MHz
  */
static void MX_SPI2_Init(void)
{
    hspi2.Instance               = SPI2;
    hspi2.Init.Mode              = SPI_MODE_MASTER;
    hspi2.Init.Direction         = SPI_DIRECTION_1LINE;  /* TX only */
    hspi2.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity       = SPI_POLARITY_HIGH;    /* CPOL = 1 */
    hspi2.Init.CLKPhase          = SPI_PHASE_2EDGE;      /* CPHA = 1 */
    hspi2.Init.NSS               = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi2.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;

    if (HAL_SPI_Init(&hspi2) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief  USART1 Initialization - PC Communication
  *         115200 baud, 8N1
  */
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
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief  UART4 Initialization - Debug Port
  *         115200 baud, 8N1
  */
static void MX_UART4_Init(void)
{
    huart4.Instance          = UART4;
    huart4.Init.BaudRate     = 115200;
    huart4.Init.WordLength   = UART_WORDLENGTH_8B;
    huart4.Init.StopBits     = UART_STOPBITS_1;
    huart4.Init.Parity       = UART_PARITY_NONE;
    huart4.Init.Mode         = UART_MODE_TX_RX;
    huart4.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart4.Init.OverSampling = UART_OVERSAMPLING_16;
    huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart4.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart4) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief  FDCAN1 Initialization
  *         Classic CAN, 1 Mbps
  *         Prescaler=10, TimeSeg1=7, TimeSeg2=2, SJW=1
  *         Bit Rate = 100MHz / 10 / (7+2+1) = 1 Mbps
  */
static void MX_FDCAN1_Init(void)
{
    hfdcan1.Instance                  = FDCAN1;
    hfdcan1.Init.ClockDivider         = FDCAN_CLOCK_DIV1;
    hfdcan1.Init.FrameFormat          = FDCAN_FRAME_CLASSIC;
    hfdcan1.Init.Mode                 = FDCAN_MODE_NORMAL;
    hfdcan1.Init.AutoRetransmission   = ENABLE;
    hfdcan1.Init.TransmitPause        = ENABLE;
    hfdcan1.Init.ProtocolException    = DISABLE;
    hfdcan1.Init.NominalPrescaler     = 10;
    hfdcan1.Init.NominalSyncJumpWidth = 1;
    hfdcan1.Init.NominalTimeSeg1      = 7;
    hfdcan1.Init.NominalTimeSeg2      = 2;
    hfdcan1.Init.StdFiltersNbr        = 1;
    hfdcan1.Init.ExtFiltersNbr        = 0;
    hfdcan1.Init.TxFifoQueueMode      = FDCAN_TX_FIFO_OPERATION;

    if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK) {
        Error_Handler();
    }

    /* Configure RX filter - accept all standard IDs for now */
    FDCAN_FilterTypeDef filter;
    filter.IdType       = FDCAN_STANDARD_ID;
    filter.FilterIndex  = 0;
    filter.FilterType   = FDCAN_FILTER_MASK;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1    = 0x000;
    filter.FilterID2    = 0x000;   /* Accept all */

    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &filter) != HAL_OK) {
        Error_Handler();
    }

    /* Reject non-matching frames */
    if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1,
            FDCAN_REJECT, FDCAN_REJECT,
            FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief  DMA Initialization
  */
static void MX_DMA_Init(void)
{
    /* DMA controller clock enable */
    __HAL_RCC_DMAMUX1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* DMA interrupt init */
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
}

/**
  * @brief  GPIO Initialization
  *         - SPI CS pins (PC0~PC7) as output, initial HIGH
  *         - LED pins (PB4, PB5) as output, initial LOW
  */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIO clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();  /* HSE pins */

    /* --- ADC CS Pins (PC0~PC3) - Initial HIGH (deselected) --- */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3,
                      GPIO_PIN_SET);

    GPIO_InitStruct.Pin   = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* --- DAC CS Pins (PC4~PC7) - Initial HIGH (deselected) --- */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7,
                      GPIO_PIN_SET);

    GPIO_InitStruct.Pin   = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* --- LED Pins (PB4=STATUS, PB5=ERROR) - Initial LOW (OFF) --- */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin   = GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* -------------------------------------------------------------------------- */
/*                         Interrupt Handlers                                 */
/* -------------------------------------------------------------------------- */

/**
  * @brief  USART1 IRQ Handler - processes IDLE line for DMA RX
  */
void USART1_IRQHandler(void)
{
    /* Check for IDLE line interrupt (end of message) */
    UART1_IDLE_Callback();

    /* HAL handler for other UART interrupts */
    HAL_UART_IRQHandler(&huart1);
}

/**
  * @brief  DMA1 Channel 1 IRQ Handler (USART1 RX DMA)
  */
void DMA1_Channel1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(huart1.hdmarx);
}

/**
  * @brief  DMA1 Channel 2 IRQ Handler (USART1 TX DMA)
  */
void DMA1_Channel2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(huart1.hdmatx);
}

/* -------------------------------------------------------------------------- */
/*                           Error Handler                                    */
/* -------------------------------------------------------------------------- */

void Error_Handler(void)
{
    __disable_irq();

    /* Turn on error LED */
    HAL_GPIO_WritePin(LED_ERROR_GPIO_Port, LED_ERROR_Pin, GPIO_PIN_SET);

    while (1) {
        /* Stay in error state */
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    /* User can add own assert handling */
}
#endif
