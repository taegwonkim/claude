/* USER CODE BEGIN Header */
/* MCP3465R calibration example for STM32L552RCT6
 * SPI1: PA5(SCK) PA6(MISO) PA7(MOSI)  CS: PA4
 * USART1: PA9(TX) PA10(RX) 115200 bps  (ST-Link VCP)
 */
/* USER CODE END Header */

#include "main.h"
#include "mcp3465r_cal.h"
#include <stdio.h>
#include <string.h>

/* USER CODE BEGIN PV */
SPI_HandleTypeDef  hspi1;
UART_HandleTypeDef huart1;
/* USER CODE END PV */

/* ── Forward declarations ─────────────────────────────────────────────────── */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void DWT_Init(void);

/* USER CODE BEGIN PFP */
static int  app_init_adc(void);
static void app_run_measurement(void);
static int32_t adc_to_mv(int32_t raw, uint32_t vref_mv);
/* USER CODE END PFP */

/* ── Retarget printf → USART1 ────────────────────────────────────────────── */
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
    return ch;
}

/* ── main ─────────────────────────────────────────────────────────────────── */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    DWT_Init();

    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_USART1_UART_Init();

    /* USER CODE BEGIN 2 */
    printf("\r\n=== MCP3465R + STM32L552RCT6 ===\r\n");

    if (app_init_adc() != 0) {
        printf("[FATAL] ADC init failed\r\n");
        Error_Handler();
    }
    /* USER CODE END 2 */

    /* USER CODE BEGIN WHILE */
    while (1) {
        app_run_measurement();
        HAL_Delay(500);
    }
    /* USER CODE END WHILE */
}

/* USER CODE BEGIN 4 */

static int app_init_adc(void)
{
    if (mcp3465r_cal_is_valid()) {
        printf("[INFO] Calibration found in flash. Applying...\r\n");

        MCP3465R_CalData_t cal;
        if (mcp3465r_cal_read(&cal) == CAL_OK) {
            printf("  offset = %ld\r\n",         (long)cal.offset);
            printf("  gain   = 0x%06lX (%.5fx)\r\n",
                   (unsigned long)cal.gain,
                   (double)cal.gain / (double)MCP3465R_GAINCAL_UNITY);
        }

        cal_status_t st = mcp3465r_cal_load_and_apply();
        if (st != CAL_OK) {
            printf("[ERROR] Apply failed: %d\r\n", st);
            return -1;
        }
        printf("[INFO] Calibration applied.\r\n");

    } else {
        printf("[INFO] No calibration found. Running calibration...\r\n");
        printf("       Ensure VIN+/VIN- are shorted for offset step,\r\n");
        printf("       and REFIN+/REFIN- carry a known reference signal.\r\n");

        /* PGA=1, OSR=256 (CONFIG1=0x14), full-scale reference = 0x7FFFFF */
        cal_status_t st = mcp3465r_calibrate(
            MCP3465R_CFG2_GAIN_1,
            0x14U,
            MCP3465R_ADC_MAX_CODE
        );
        if (st != CAL_OK) {
            printf("[ERROR] Calibration failed: %d\r\n", st);
            return -1;
        }

        MCP3465R_CalData_t cal;
        if (mcp3465r_cal_read(&cal) == CAL_OK) {
            printf("[INFO] Calibration saved:\r\n");
            printf("  offset          = %ld\r\n",     (long)cal.offset);
            printf("  gain            = 0x%06lX\r\n", (unsigned long)cal.gain);
            printf("  raw offset mean = %ld\r\n",     (long)cal.raw_offset_mean);
            printf("  raw gain mean   = %ld\r\n",     (long)cal.raw_gain_mean);
        }
    }
    return 0;
}

static int32_t adc_to_mv(int32_t raw, uint32_t vref_mv)
{
    return (int32_t)(((int64_t)raw * (int64_t)vref_mv) /
                     (int64_t)MCP3465R_ADC_MAX_CODE);
}

static void app_run_measurement(void)
{
    /* Route CH0+ single-ended vs AGND */
    uint8_t mux = MCP3465R_MUX_CH(MCP3465R_MUX_CH0P, MCP3465R_MUX_AGND);
    mcp3465r_write_reg(MCP3465R_REG_MUX, &mux, 1);
    HAL_Delay(1);

    int32_t raw;
    if (mcp3465r_read_adc(&raw) != MCP3465R_OK) {
        printf("[WARN] ADC read error\r\n");
        return;
    }
    int32_t mv = adc_to_mv(raw, 2048U);
    printf("CH0: raw=%8ld  %5ld mV\r\n", (long)raw, (long)mv);
}
/* USER CODE END 4 */

/* ── Clock configuration ────────────────────────────────────────────────────
 * HSI16 → PLL → 110 MHz
 * PLLM=2, PLLN=55, PLLR=4  (VCO = 440 MHz, SYSCLK = 110 MHz)
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* Enable the internal voltage regulator to allow 110 MHz operation */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState       = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM       = 2;
    osc.PLL.PLLN       = 55;
    osc.PLL.PLLP       = RCC_PLLP_DIV7;
    osc.PLL.PLLQ       = RCC_PLLQ_DIV2;
    osc.PLL.PLLR       = RCC_PLLR_DIV4;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        Error_Handler();

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    /* 5 flash wait states required for 110 MHz at VOS0 */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK)
        Error_Handler();
}

/* ── SPI1 init ──────────────────────────────────────────────────────────────
 * Full-duplex master, Mode 0 (CPOL=0 CPHA=0), 8-bit MSB, NSS software
 * Clock = PCLK2/8 = 110/8 ≈ 13.75 MHz
 */
static void MX_SPI1_Init(void)
{
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial     = 7;
    hspi1.Init.CRCLength         = SPI_CRC_LENGTH_DATASIZE;
    hspi1.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
        Error_Handler();
}

/* ── USART1 init ─────────────────────────────────────────────────────────── */
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
    if (HAL_UART_Init(&huart1) != HAL_OK)
        Error_Handler();
}

/* ── GPIO init ──────────────────────────────────────────────────────────────
 * PA4: MCP_CS → output high (de-asserted)
 * PC13: MCP_IRQ → input with pull-up
 * SPI1 and USART1 alternate function pins are configured in HAL_SPI_MspInit
 * and HAL_UART_MspInit (in stm32l5xx_hal_msp.c).
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* CS default high (de-assert before SPI init) */
    HAL_GPIO_WritePin(MCP_CS_GPIO_Port, MCP_CS_Pin, GPIO_PIN_SET);

    gpio.Pin   = MCP_CS_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MCP_CS_GPIO_Port, &gpio);

    gpio.Pin  = MCP_IRQ_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(MCP_IRQ_GPIO_Port, &gpio);
}

/* ── DWT cycle counter (for microsecond delays) ─────────────────────────── */
static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT       = 0;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;
}

/* ── Error handler ───────────────────────────────────────────────────────── */
void Error_Handler(void)
{
    __disable_irq();
    while (1) { /* blink or assert */ }
}
