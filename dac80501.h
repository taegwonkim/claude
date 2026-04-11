/*
 * dac80501.h - Driver for Texas Instruments DAC80501 (SPI variant)
 *
 * The DAC80501 is a 16-bit, single-channel, buffered voltage-output DAC
 * with an internal 2.5 V reference. Communication is via a 24-bit SPI
 * frame: 1 command byte (R/W + 7-bit address) + 2 data bytes (MSB first).
 *
 * SPI mode 1 or 2 (CPOL=0/CPHA=1 or CPOL=1/CPHA=0), up to 50 MHz.
 */

#ifndef DAC80501_H
#define DAC80501_H

#include <stdint.h>
#include <stdbool.h>

/* ---------- Register map ---------- */
#define DAC80501_REG_NOOP     0x00
#define DAC80501_REG_DEVID    0x01
#define DAC80501_REG_SYNC     0x02
#define DAC80501_REG_CONFIG   0x03
#define DAC80501_REG_GAIN     0x04
#define DAC80501_REG_TRIGGER  0x05
#define DAC80501_REG_STATUS   0x07
#define DAC80501_REG_DAC      0x08

/* Read bit for the command byte */
#define DAC80501_CMD_READ     0x80

/* ---------- CONFIG register bits ---------- */
#define DAC80501_CONFIG_REF_PWDWN   (1u << 8)  /* 1 = disable internal ref  */
#define DAC80501_CONFIG_DAC_PWDWN   (1u << 0)  /* 1 = power down DAC output */

/* ---------- GAIN register bits ---------- */
#define DAC80501_GAIN_BUFF_GAIN_2X  (1u << 0)  /* 1 = output buffer gain 2x */
#define DAC80501_GAIN_REF_DIV_2     (1u << 8)  /* 1 = reference divided /2  */

/* ---------- TRIGGER register bits ---------- */
#define DAC80501_TRIGGER_LDAC       (1u << 4)  /* software LDAC trigger     */
#define DAC80501_TRIGGER_SOFT_RESET (0x000Au)  /* write to issue reset      */

/* ---------- SYNC register bits ---------- */
#define DAC80501_SYNC_DAC_SYNC_EN   (1u << 0)  /* 1 = synchronous update    */

/* ---------- Expected device ID ---------- */
#define DAC80501_DEVID_VALUE        0x0115u    /* bits [15:0] after read    */

/*
 * Platform callbacks. The caller provides these so the driver is
 * portable across MCUs. All three must be non-NULL.
 *
 *   spi_xfer : full-duplex 3-byte transfer, CS managed by the callback.
 *   delay_us : blocking microsecond delay (used around reset).
 *   user_ctx : opaque pointer passed back to the callbacks.
 */
typedef struct {
    int  (*spi_xfer)(void *user_ctx, const uint8_t tx[3], uint8_t rx[3]);
    void (*delay_us)(void *user_ctx, uint32_t us);
    void  *user_ctx;

    /* Reference voltage actually seen by the core, in millivolts.
     * For the internal 2.5 V reference with gain = 1x this is 2500. */
    uint32_t vref_mv;
    /* Output buffer gain setting that was programmed (1 or 2). */
    uint8_t  buffer_gain;
} dac80501_t;

/* ---------- High-level API ---------- */

/* Verify communication and reset the device to a known state.
 * Returns 0 on success, negative on SPI or identification error. */
int dac80501_init(dac80501_t *dev);

/* Raw 16-bit register access. */
int dac80501_write_reg(dac80501_t *dev, uint8_t reg, uint16_t value);
int dac80501_read_reg (dac80501_t *dev, uint8_t reg, uint16_t *value);

/* Write a raw 16-bit code to the DAC data register. */
int dac80501_write_code(dac80501_t *dev, uint16_t code);

/* Write a voltage in millivolts. The value is clamped to [0, Vout_max]
 * where Vout_max = vref_mv * buffer_gain. */
int dac80501_write_mv(dac80501_t *dev, uint32_t millivolts);

/* Configure output buffer gain (1 or 2) and reference divider (1 or 2).
 * Updates dev->vref_mv / dev->buffer_gain so the mv helper stays
 * consistent. Pass 2500 for internal_vref_mv when using the internal
 * reference. */
int dac80501_set_gain(dac80501_t *dev,
                      uint8_t  buffer_gain,
                      uint8_t  ref_div,
                      uint32_t internal_vref_mv);

/* Issue a software reset. */
int dac80501_soft_reset(dac80501_t *dev);

#endif /* DAC80501_H */
