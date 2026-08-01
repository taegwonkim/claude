/**
 * main.c
 *
 * CubeMX-style project entry point for the STM32L562 FreeRTOS wireless
 * bridge application. Structure (function names/order, USER CODE
 * markers) mirrors what STM32CubeMX generates so this file can be
 * diffed against / merged with your own CubeMX output once you
 * configure the same peripherals (see README.md "STM32CubeMX 설정
 * 체크리스트").
 *
 * IMPORTANT - two sections in this file are best-effort reference
 * values, NOT guaranteed to be bit-exact for your specific board/silicon
 * revision; let CubeMX regenerate them and prefer its output:
 *   1) SystemClock_Config()  - PLL dividers / Flash latency / HSI48+CRS
 *      setup for the USB 48MHz clock
 *   2) MX_DMA_Init()         - exact DMA channel + DMAMUX request macro
 *      for USART3_RX (CubeMX auto-assigns these)
 *
 * USB CDC: this file only calls MX_USB_DEVICE_Init() - the function
 * itself, plus usb_device.c/usbd_conf.c/usbd_desc.c, are ST/CubeMX
 * middleware generated automatically once you add the USB_DEVICE
 * (Communication Device Class) middleware in CubeMX and are NOT
 * included in this repo (exact PCD instance/IRQ names are family- and
 * CubeMX-version-specific, so hand-authoring them here would be
 * unreliable). The one file you DO need to edit by hand is
 * USB_DEVICE/App/usbd_cdc_if.c's CDC_Receive_FS() - see README.md.
 * Everything else (GPIO AF mapping, UART/SPI parameters, task wiring)
 * follows the pin table in README.md and standard STM32 HAL usage.
 */
#include "main.h"
#include "app_config.h"
#include "freertos.h"
#include "cmsis_os2.h"
#include "usb_device.h"

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
SPI_HandleTypeDef  hspi1;
DMA_HandleTypeDef   hdma_usart3_rx;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_SPI1_Init(void);

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_USART3_UART_Init();
    MX_SPI1_Init();
    MX_USB_DEVICE_Init();   /* USB CDC-ACM virtual COM port, parallel PC config channel */

    /* USER CODE BEGIN 2 */
    /* USER CODE END 2 */

    /* App_Init()/App_CreateTasks() run inside MX_FREERTOS_Init() (see
     * freertos.c) so that FreeRTOS kernel objects (mutexes, semaphores,
     * queues, tasks) are only created after osKernelInitialize(). */
    osKernelInitialize();
    MX_FREERTOS_Init();
    osKernelStart();

    /* osKernelStart() does not return under normal operation. */
    for (;;) {
    }
}

/**
 * System clock: HSI16 -> PLL -> SYSCLK 80 MHz, Range 1 (no boost mode).
 * Also enables HSI48 + CRS (trimmed against the USB SOF signal) as the
 * USB 48MHz clock source, so no external crystal is needed for USB.
 * VERIFY against STM32CubeMX's "Clock Configuration" tab for your exact
 * part/board before relying on this in production.
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    RCC_CRSInitTypeDef RCC_CRSInitStruct = {0};

    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSI48;
    RCC_OscInitStruct.HSIState       = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.HSI48State      = RCC_HSI48_ON;   /* USB 48MHz source */
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM       = 4;   /* 16MHz / 4   =   4 MHz VCO in  */
    RCC_OscInitStruct.PLL.PLLN       = 40;  /*  4MHz * 40  = 160 MHz VCO out */
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ       = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR       = RCC_PLLR_DIV2; /* 160MHz / 2 = 80 MHz SYSCLK */
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                 | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
        Error_Handler();
    }

    /* Route USB to HSI48 (not the main PLL) */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
    PeriphClkInit.UsbClockSelection    = RCC_USBCLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
        Error_Handler();
    }

    /* Clock Recovery System: trims HSI48 against USB Start-Of-Frame
     * pulses so it meets the USB spec's +/-0.25% clock accuracy
     * requirement without an external crystal. */
    __HAL_RCC_CRS_CLK_ENABLE();
    RCC_CRSInitStruct.Prescaler = RCC_CRS_SYNC_DIV1;
    RCC_CRSInitStruct.Source = RCC_CRS_SYNC_SOURCE_USB;
    RCC_CRSInitStruct.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
    RCC_CRSInitStruct.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000, 1000);
    RCC_CRSInitStruct.ErrorLimitValue = 34;
    RCC_CRSInitStruct.HSI48CalibrationValue = 32;
    HAL_RCCEx_CRSConfig(&RCC_CRSInitStruct);
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* EEPROM (W25Q40CL) chip-select, idle high */
    HAL_GPIO_WritePin(EEPROM_CS_GPIO_Port, EEPROM_CS_Pin, GPIO_PIN_SET);
    GPIO_InitStruct.Pin   = EEPROM_CS_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(EEPROM_CS_GPIO_Port, &GPIO_InitStruct);

    /* Cyclone IV trigger input: falling edge EXTI, pull-up (idle high) */
    GPIO_InitStruct.Pin  = FPGA_TRIGGER_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(FPGA_TRIGGER_GPIO_Port, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(FPGA_TRIGGER_EXTI_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(FPGA_TRIGGER_EXTI_IRQn);

    /* USART1: PA9=TX, PA10=RX (AF7) */
    GPIO_InitStruct.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART2: PA2=TX, PA3=RX (AF7) */
    GPIO_InitStruct.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* SPI1: PA5=SCK, PA6=MISO, PA7=MOSI (AF5) */
    GPIO_InitStruct.Pin       = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART3: PB10=TX, PB11=RX (AF7) */
    GPIO_InitStruct.Pin       = GPIO_PIN_10 | GPIO_PIN_11;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/**
 * DMA1 channel for USART3 RX (used by HAL_UARTEx_ReceiveToIdle_DMA in
 * app_fpga_if.c). The channel number / DMAMUX request macro below is a
 * common assignment on STM32L5, but CubeMX may pick a different free
 * channel - trust CubeMX's generated MX_DMA_Init() over this one.
 */
static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

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
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
    HAL_NVIC_SetPriority(USART1_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
    HAL_NVIC_SetPriority(USART2_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

static void MX_USART3_UART_Init(void)
{
    huart3.Instance = USART3;
    huart3.Init.BaudRate = 921600;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;

    /* Link the RX DMA channel so app_fpga_if.c can use
     * HAL_UARTEx_ReceiveToIdle_DMA(). */
    hdma_usart3_rx.Instance = DMA1_Channel1;
    hdma_usart3_rx.Init.Request = DMA_REQUEST_USART3_RX; /* verify vs CubeMX */
    hdma_usart3_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart3_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart3_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart3_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart3_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart3_rx.Init.Mode = DMA_NORMAL;
    hdma_usart3_rx.Init.Priority = DMA_PRIORITY_MEDIUM;
    if (HAL_DMA_Init(&hdma_usart3_rx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(&huart3, hdmarx, hdma_usart3_rx);

    if (HAL_UART_Init(&huart3) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK) {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(USART3_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
}

static void MX_SPI1_Init(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;   /* W25Q40CL SPI Mode 0 */
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;               /* CS driven manually, see app_eeprom.c */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; /* conservative, well under 104MHz max */
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 7;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}
