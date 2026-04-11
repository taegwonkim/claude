/*
 * dac80501.c - Driver implementation for Texas Instruments DAC80501.
 */

#include "dac80501.h"

#include <stddef.h>

/* ---------- Low-level SPI helper ---------- */

static int dac80501_xfer(dac80501_t *dev,
                         uint8_t cmd,
                         uint16_t tx_data,
                         uint16_t *rx_data)
{
    uint8_t tx[3];
    uint8_t rx[3] = { 0, 0, 0 };

    tx[0] = cmd;
    tx[1] = (uint8_t)(tx_data >> 8);
    tx[2] = (uint8_t)(tx_data & 0xFF);

    int rc = dev->spi_xfer(dev->user_ctx, tx, rx);
    if (rc != 0) {
        return rc;
    }

    if (rx_data != NULL) {
        *rx_data = ((uint16_t)rx[1] << 8) | rx[2];
    }
    return 0;
}

int dac80501_write_reg(dac80501_t *dev, uint8_t reg, uint16_t value)
{
    return dac80501_xfer(dev, reg & 0x7F, value, NULL);
}

int dac80501_read_reg(dac80501_t *dev, uint8_t reg, uint16_t *value)
{
    /* A DAC80501 read requires two frames: the first latches the
     * register address, the second clocks the data out on MISO. */
    int rc = dac80501_xfer(dev, DAC80501_CMD_READ | (reg & 0x7F), 0x0000, NULL);
    if (rc != 0) {
        return rc;
    }
    return dac80501_xfer(dev, DAC80501_REG_NOOP, 0x0000, value);
}

/* ---------- High-level API ---------- */

int dac80501_soft_reset(dac80501_t *dev)
{
    int rc = dac80501_write_reg(dev, DAC80501_REG_TRIGGER,
                                DAC80501_TRIGGER_SOFT_RESET);
    if (rc != 0) {
        return rc;
    }
    /* Reset propagates in < 1 us; give it a little margin. */
    dev->delay_us(dev->user_ctx, 50);
    return 0;
}

int dac80501_init(dac80501_t *dev)
{
    if (dev == NULL ||
        dev->spi_xfer == NULL ||
        dev->delay_us == NULL) {
        return -1;
    }

    int rc = dac80501_soft_reset(dev);
    if (rc != 0) {
        return rc;
    }

    uint16_t devid = 0;
    rc = dac80501_read_reg(dev, DAC80501_REG_DEVID, &devid);
    if (rc != 0) {
        return rc;
    }
    if (devid != DAC80501_DEVID_VALUE) {
        return -2;
    }

    /* Reset defaults: internal 2.5 V reference enabled, gain = 1x,
     * so full-scale output = 2.5 V. */
    if (dev->vref_mv == 0) {
        dev->vref_mv = 2500;
    }
    if (dev->buffer_gain == 0) {
        dev->buffer_gain = 1;
    }
    return 0;
}

int dac80501_set_gain(dac80501_t *dev,
                      uint8_t  buffer_gain,
                      uint8_t  ref_div,
                      uint32_t internal_vref_mv)
{
    if (dev == NULL) {
        return -1;
    }
    if ((buffer_gain != 1 && buffer_gain != 2) ||
        (ref_div     != 1 && ref_div     != 2)) {
        return -1;
    }

    uint16_t reg = 0;
    if (buffer_gain == 2) {
        reg |= DAC80501_GAIN_BUFF_GAIN_2X;
    }
    if (ref_div == 2) {
        reg |= DAC80501_GAIN_REF_DIV_2;
    }

    int rc = dac80501_write_reg(dev, DAC80501_REG_GAIN, reg);
    if (rc != 0) {
        return rc;
    }

    /* Vref as seen by the core is internal_vref_mv / ref_div.
     * The buffer then multiplies by buffer_gain at the output. */
    dev->vref_mv     = internal_vref_mv / ref_div;
    dev->buffer_gain = buffer_gain;
    return 0;
}

int dac80501_write_code(dac80501_t *dev, uint16_t code)
{
    return dac80501_write_reg(dev, DAC80501_REG_DAC, code);
}

int dac80501_write_mv(dac80501_t *dev, uint32_t millivolts)
{
    uint32_t vout_max = dev->vref_mv * (uint32_t)dev->buffer_gain;
    if (vout_max == 0) {
        return -1;
    }
    if (millivolts > vout_max) {
        millivolts = vout_max;
    }

    /* code = round(millivolts * 65535 / vout_max)
     * Use 64-bit math to keep the full 16-bit resolution without
     * overflow on 32-bit MCUs. */
    uint64_t num = (uint64_t)millivolts * 65535u + (vout_max / 2);
    uint16_t code = (uint16_t)(num / vout_max);
    return dac80501_write_code(dev, code);
}
