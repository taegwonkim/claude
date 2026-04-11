/*
 * example_main.c - DAC80501 usage example.
 *
 * Shows:
 *   1. Binding the driver to platform SPI / delay callbacks.
 *   2. Initialising the device and verifying the DEVID.
 *   3. Configuring the gain for a 0-5 V output (internal ref x2).
 *   4. Generating a midscale output and a 100-step ramp.
 *
 * The platform_spi_xfer and platform_delay_us functions below are
 * stubs - wire them up to your MCU's SPI HAL (STM32, nRF, ESP-IDF, ...).
 */

#include "dac80501.h"

#include <stdio.h>

/* =========================================================
 *  Platform glue - replace the bodies with real HAL calls.
 * ========================================================= */

static int platform_spi_xfer(void *ctx, const uint8_t tx[3], uint8_t rx[3])
{
    (void)ctx;
    (void)tx;
    /*
     * Expected sequence on your MCU:
     *   1. CS_LOW();
     *   2. HAL_SPI_TransmitReceive(&hspi, tx, rx, 3, timeout);
     *   3. CS_HIGH();
     * Return 0 on success, non-zero on failure.
     */
    rx[0] = 0; rx[1] = 0; rx[2] = 0;
    return 0;
}

static void platform_delay_us(void *ctx, uint32_t us)
{
    (void)ctx;
    (void)us;
    /* e.g. HAL_Delay((us + 999) / 1000) on STM32 Cube. */
}

/* =========================================================
 *  Example application
 * ========================================================= */

int main(void)
{
    dac80501_t dac = {
        .spi_xfer    = platform_spi_xfer,
        .delay_us    = platform_delay_us,
        .user_ctx    = NULL,
        .vref_mv     = 2500, /* internal reference */
        .buffer_gain = 1,
    };

    if (dac80501_init(&dac) != 0) {
        printf("DAC80501: init failed\n");
        return 1;
    }
    printf("DAC80501: init ok\n");

    /* 0-5 V output: keep internal 2.5 V reference (ref_div = 1),
     * enable 2x output buffer gain. */
    if (dac80501_set_gain(&dac, /*buffer_gain=*/2,
                                /*ref_div=*/1,
                                /*internal_vref_mv=*/2500) != 0) {
        printf("DAC80501: gain config failed\n");
        return 1;
    }

    /* Midscale (~2.5 V). */
    dac80501_write_code(&dac, 0x8000);

    /* 100-step ramp across the full 0-5 V range. */
    for (int i = 0; i <= 100; ++i) {
        uint32_t mv = (uint32_t)i * 5000u / 100u;
        if (dac80501_write_mv(&dac, mv) != 0) {
            printf("DAC80501: write %u mV failed\n", (unsigned)mv);
            return 1;
        }
        platform_delay_us(NULL, 1000); /* 1 ms between steps */
    }

    /* Read back the DAC register to confirm the last value landed. */
    uint16_t code = 0;
    if (dac80501_read_reg(&dac, DAC80501_REG_DAC, &code) == 0) {
        printf("DAC80501: last code = 0x%04X\n", code);
    }

    return 0;
}
