#include "mcp3465r_cal.h"
#include "stm32l552_flash.h"
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* BSP stubs – replace with your STM32L552 SPI/GPIO implementation     */
/* ------------------------------------------------------------------ */

void mcp3465r_cs_low(void)
{
    /* HAL_GPIO_WritePin(MCP_CS_GPIO_Port, MCP_CS_Pin, GPIO_PIN_RESET); */
}

void mcp3465r_cs_high(void)
{
    /* HAL_GPIO_WritePin(MCP_CS_GPIO_Port, MCP_CS_Pin, GPIO_PIN_SET); */
}

int mcp3465r_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    /* Example using STM32 HAL:
     *   if (tx && rx) return HAL_SPI_TransmitReceive(&hspi1, tx, rx, len, 10);
     *   if (tx)       return HAL_SPI_Transmit(&hspi1, (uint8_t*)tx, len, 10);
     *   if (rx)       return HAL_SPI_Receive(&hspi1, rx, len, 10);
     */
    (void)tx; (void)rx; (void)len;
    return 0;
}

void mcp3465r_delay_us(uint32_t us)
{
    /* Use TIM or DWT cycle counter for microsecond delay.
     * Example with SysTick (1 us tick):
     *   uint32_t start = SysTick->VAL;
     *   while ((start - SysTick->VAL) < us * (SystemCoreClock / 1000000));
     */
    (void)us;
}

/* ------------------------------------------------------------------ */
/* Application entry point                                              */
/* ------------------------------------------------------------------ */

/* Call once after power-on. Loads calibration from flash if valid,
 * otherwise runs the calibration sequence and stores the result. */
static int app_init_adc(void)
{
    printf("=== MCP3465R Calibration Init ===\n");

    if (mcp3465r_cal_is_valid()) {
        printf("[INFO] Valid calibration found in flash. Applying...\n");

        MCP3465R_CalData_t cal;
        cal_status_t st = mcp3465r_cal_read(&cal);
        if (st == CAL_OK) {
            printf("  offset = %ld (0x%06lX)\n",
                   (long)cal.offset, (unsigned long)(cal.offset & 0xFFFFFFUL));
            printf("  gain   = 0x%06lX  (%.4f x)\n",
                   (unsigned long)cal.gain,
                   (double)cal.gain / (double)MCP3465R_GAINCAL_UNITY);
        }

        st = mcp3465r_cal_load_and_apply();
        if (st != CAL_OK) {
            printf("[ERROR] Failed to apply calibration: %d\n", st);
            return -1;
        }
        printf("[INFO] Calibration applied.\n");
    } else {
        printf("[INFO] No valid calibration. Running calibration...\n");

        /* Calibrate at unity gain, OSR=256 (CONFIG1 bits for ~244 SPS).
         * vref_counts = 0x7FFFFF (full-scale) for internal 2.048V reference.
         * Adjust vref_counts if using a lower-voltage reference signal. */
        cal_status_t st = mcp3465r_calibrate(
            MCP3465R_CFG2_GAIN_1,   /* PGA gain = 1 */
            0x14U,                  /* CONFIG1: OSR=256 */
            MCP3465R_ADC_MAX_CODE   /* Expected full-scale code */
        );

        if (st != CAL_OK) {
            printf("[ERROR] Calibration failed: %d\n", st);
            return -1;
        }

        printf("[INFO] Calibration complete and saved to flash.\n");

        /* Read back and print for confirmation */
        MCP3465R_CalData_t cal;
        if (mcp3465r_cal_read(&cal) == CAL_OK) {
            printf("  offset          = %ld\n",      (long)cal.offset);
            printf("  gain            = 0x%06lX\n",  (unsigned long)cal.gain);
            printf("  raw offset mean = %ld\n",      (long)cal.raw_offset_mean);
            printf("  raw gain mean   = %ld\n",      (long)cal.raw_gain_mean);
        }
    }

    return 0;
}

/* Convert a raw ADC code to voltage in mV.
 * Vref = 2048 mV (internal reference), full-scale = 0x7FFFFF.
 * Hardware OFFSETCAL and GAINCAL are already applied by the ADC. */
static int32_t adc_to_mv(int32_t raw_code, uint32_t vref_mv)
{
    /* Use int64_t to avoid overflow for large codes */
    return (int32_t)(((int64_t)raw_code * (int64_t)vref_mv) /
                     (int64_t)MCP3465R_ADC_MAX_CODE);
}

static void app_run_measurement(void)
{
    /* Switch MUX to CH0 differential (CH0+ vs CH1-) */
    uint8_t mux = MCP3465R_MUX_CH(MCP3465R_MUX_CH0P, MCP3465R_MUX_AGND);
    mcp3465r_write_reg(MCP3465R_REG_MUX, &mux, 1);
    mcp3465r_delay_us(200);

    int32_t raw;
    if (mcp3465r_read_adc(&raw) != MCP3465R_OK) {
        printf("[ERROR] ADC read failed\n");
        return;
    }

    int32_t mv = adc_to_mv(raw, 2048U);
    printf("CH0: raw=%ld  voltage=%ld mV\n", (long)raw, (long)mv);
}

int main(void)
{
    if (app_init_adc() != 0)
        return 1;

    /* Normal measurement loop */
    for (int i = 0; i < 5; i++)
        app_run_measurement();

    /* Example: force re-calibration by erasing stored data */
    /* mcp3465r_cal_erase(); */

    return 0;
}
