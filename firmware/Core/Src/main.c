/**
 * @file    main.c
 * @brief   Voltage-Current Monitoring System - Main Entry Point
 *
 * MCU: STM32L552RETx
 * RTOS: FreeRTOS (CMSIS_V2)
 *
 * Peripherals:
 *   - SPI1: MCP3465R (External ADC, voltage/current measurement)
 *   - SPI2: AD5641 x4 (External DAC, 4-channel voltage output)
 *   - USART1: PC communication (115200 baud)
 *   - FDCAN1: CAN bus reporting (500 kbps)
 *   - UART4:  Debug output TX only (115200 baud, PA0)
 *
 * This file is intended for STM32CubeIDE with CubeMX code generation.
 * User code sections (USER CODE BEGIN/END) are preserved during regeneration.
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os2.h"

/* USER CODE BEGIN Includes */
#include "dac_ad5641.h"
#include "adc_mcp3465r.h"
#include "voltage_control.h"
#include "comm_handler.h"
#include "debug_uart.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef  hspi1;
SPI_HandleTypeDef  hspi2;
UART_HandleTypeDef huart1;
FDCAN_HandleTypeDef hfdcan1;
UART_HandleTypeDef huart4;

/* USER CODE BEGIN PV */
extern Comm_Handle_t g_comm;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_FDCAN1_Init(void);
static void MX_UART4_Init(void);
extern void MX_FREERTOS_Init(void);

/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/**
 * @brief  The application entry point.
 */
int main(void)
{
    /* MCU Configuration--------------------------------------------------------*/
    HAL_Init();
    SystemClock_Config();

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_SPI2_Init();
    MX_USART1_UART_Init();
    MX_FDCAN1_Init();
    MX_UART4_Init();

    /* USER CODE BEGIN 2 */

    /* Start FDCAN */
    HAL_FDCAN_Start(&hfdcan1);

    /* Initialize debug output via UART4 */
    Debug_Init(&huart4);
    Debug_Print("SYS", "System boot, peripherals initialized");

    /* USER CODE END 2 */

    /* Init scheduler */
    osKernelInitialize();

    /* Call init function for freertos objects (tasks, mutexes, queues...) */
    MX_FREERTOS_Init();

    /* Start scheduler */
    osKernelStart();

    /* We should never get here as control is now taken by the scheduler */
    while (1)
    {
    }
}

/* ========================================================================
 * System Clock Configuration
 * HSE = 8 MHz -> PLL -> SYSCLK = 110 MHz
 * ======================================================================== */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Configure the main internal regulator output voltage */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0) != HAL_OK)
    {
        Error_Handler();
    }

    /* Initializes the RCC Oscillators according to the specified parameters */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.LSEState = RCC_LSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 1;       /* HSE/1 = 8 MHz */
    RCC_OscInitStruct.PLL.PLLN = 27;      /* VCO = 8 * 27 = 216 MHz */
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;  /* Not used */
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV4;  /* PLLQ = 54 MHz (FDCAN) */
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;  /* SYSCLK = 216/2 = 108 MHz (close to 110) */

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /* Initializes the CPU, AHB and APB buses clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ========================================================================
 * SPI1 Initialization: MCP3465R (External ADC)
 * Full-Duplex Master, 8-bit, Mode 0,0, ~3.4 MHz
 * ======================================================================== */
static void MX_SPI1_Init(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;     /* CPOL = 0 */
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;         /* CPHA = 0 -> Mode 0,0 */
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;  /* ~3.4 MHz */
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;

    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ========================================================================
 * SPI2 Initialization: AD5641 x4 (External DAC)
 * Transmit Only Master, 8-bit, Mode 0,1, ~6.8 MHz
 * ======================================================================== */
static void MX_SPI2_Init(void)
{
    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;
    hspi2.Init.Direction = SPI_DIRECTION_2LINES_TXONLY;
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;     /* CPOL = 0 */
    hspi2.Init.CLKPhase = SPI_PHASE_2EDGE;         /* CPHA = 1 -> Mode 0,1 */
    hspi2.Init.NSS = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;  /* ~6.8 MHz */
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;

    if (HAL_SPI_Init(&hspi2) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ========================================================================
 * USART1 Initialization: PC Communication
 * 115200 baud, 8N1, Async
 * ======================================================================== */
static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ========================================================================
 * FDCAN1 Initialization: CAN Bus
 * Classic CAN, 500 kbps, Standard ID
 * ======================================================================== */
static void MX_FDCAN1_Init(void)
{
    hfdcan1.Instance = FDCAN1;
    hfdcan1.Init.ClockDivider = FDCAN_CLOCK_DIV1;
    hfdcan1.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
    hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
    hfdcan1.Init.AutoRetransmission = ENABLE;
    hfdcan1.Init.TransmitPause = ENABLE;
    hfdcan1.Init.ProtocolException = DISABLE;

    /* Bit timing for 500 kbps with 54 MHz FDCAN clock (PLLQ)
     * Prescaler = 6 -> Tq = 6/54MHz = 111.11ns
     * Bit Time = (1 + TimeSeg1 + TimeSeg2) = 18 Tq
     * Bit Rate = 54MHz / (6 * 18) = 500 kbps
     */
    hfdcan1.Init.NominalPrescaler = 6;
    hfdcan1.Init.NominalSyncJumpWidth = 3;
    hfdcan1.Init.NominalTimeSeg1 = 14;     /* Prop + Phase1 */
    hfdcan1.Init.NominalTimeSeg2 = 3;      /* Phase2 */

    /* Data bit timing (same as nominal for Classic CAN) */
    hfdcan1.Init.DataPrescaler = 6;
    hfdcan1.Init.DataSyncJumpWidth = 3;
    hfdcan1.Init.DataTimeSeg1 = 14;
    hfdcan1.Init.DataTimeSeg2 = 3;

    /* Message RAM configuration */
    hfdcan1.Init.StdFiltersNbr = 1;
    hfdcan1.Init.ExtFiltersNbr = 0;
    hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;

    if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
    {
        Error_Handler();
    }

    /* Configure standard filter to accept all messages (for RX if needed) */
    FDCAN_FilterTypeDef sFilterConfig;
    sFilterConfig.IdType = FDCAN_STANDARD_ID;
    sFilterConfig.FilterIndex = 0;
    sFilterConfig.FilterType = FDCAN_FILTER_MASK;
    sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    sFilterConfig.FilterID1 = 0x000;
    sFilterConfig.FilterID2 = 0x000;  /* Accept all */

    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ========================================================================
 * UART4 Initialization: Debug Output (TX Only)
 * 115200 baud, 8N1, TX only on PA0 (AF8)
 * ======================================================================== */
static void MX_UART4_Init(void)
{
    huart4.Instance = UART4;
    huart4.Init.BaudRate = 115200;
    huart4.Init.WordLength = UART_WORDLENGTH_8B;
    huart4.Init.StopBits = UART_STOPBITS_1;
    huart4.Init.Parity = UART_PARITY_NONE;
    huart4.Init.Mode = UART_MODE_TX;       /* TX only */
    huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart4.Init.OverSampling = UART_OVERSAMPLING_16;
    huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart4.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart4) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ========================================================================
 * GPIO Initialization
 * ======================================================================== */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIO clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* ---- SPI1 CS (ADC) - PA4 ---- */
    GPIO_InitStruct.Pin = ADC_CS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ADC_CS_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(ADC_CS_GPIO_Port, ADC_CS_Pin, GPIO_PIN_SET);  /* Deselect */

    /* ---- SPI2 CS (DAC x4) - PB12, PB14, PB1, PB2 ---- */
    GPIO_InitStruct.Pin = DAC_CS0_Pin;
    HAL_GPIO_Init(DAC_CS0_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(DAC_CS0_GPIO_Port, DAC_CS0_Pin, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = DAC_CS1_Pin;
    HAL_GPIO_Init(DAC_CS1_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(DAC_CS1_GPIO_Port, DAC_CS1_Pin, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = DAC_CS2_Pin;
    HAL_GPIO_Init(DAC_CS2_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(DAC_CS2_GPIO_Port, DAC_CS2_Pin, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = DAC_CS3_Pin;
    HAL_GPIO_Init(DAC_CS3_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(DAC_CS3_GPIO_Port, DAC_CS3_Pin, GPIO_PIN_SET);

    /* ---- Status LEDs - PC13, PC14 ---- */
    GPIO_InitStruct.Pin = LED_STATUS_Pin | LED_FAULT_Pin;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOC, LED_STATUS_Pin | LED_FAULT_Pin, GPIO_PIN_RESET);
}

/* ========================================================================
 * HAL Callbacks
 * ======================================================================== */

/**
 * @brief UART RX complete callback (ISR context)
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        Comm_UART_RxCallback(&g_comm);
    }
}

/**
 * @brief  This function is executed in case of error occurrence.
 */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
