#ifndef DAC80501_H
#define DAC80501_H

#include <stdint.h>
#include <stdbool.h>
#include "spi_hal.h"

/* -----------------------------------------------------------------------
 * DAC80501 – 16-bit SPI DAC
 * SPI frame: 24-bit (3 bytes): [CMD(8) | DATA_MSB(8) | DATA_LSB(8)]
 *
 * Vout = (DAC_CODE / 65536) × VREF × GAIN
 * Default VREF = internal 2.5 V (or external).
 * With GAIN = 2×: Vout max = 5.0 V (matched to MCP3465R VREF).
 * ----------------------------------------------------------------------- */

/* Register addresses (7-bit, [6:0]) */
#define DAC80501_REG_NOOP       0x00U
#define DAC80501_REG_DEVID      0x01U
#define DAC80501_REG_SYNC       0x02U
#define DAC80501_REG_CONFIG     0x03U
#define DAC80501_REG_GAIN       0x04U
#define DAC80501_REG_TRIGGER    0x05U
#define DAC80501_REG_STATUS     0x07U
#define DAC80501_REG_DAC        0x08U

/* CMD byte bits */
#define DAC80501_CMD_WRITE      0x00U   /* bit[7] = 0: write */
#define DAC80501_CMD_READ       0x80U   /* bit[7] = 1: read  */

/* CONFIG register bits */
#define DAC80501_CONFIG_REF_PWDWN   (1U << 0)  /* power-down internal VREF */
#define DAC80501_CONFIG_DAC_PWDWN   (1U << 8)  /* power-down DAC */

/* GAIN register bits */
#define DAC80501_GAIN_BUFF_GAIN_X2  (1U << 0)  /* output buffer gain = 2 */
#define DAC80501_GAIN_REF_DIV_2     (1U << 8)  /* VREF internally /2 */

/* TRIGGER register */
#define DAC80501_TRIGGER_SOFT_RESET 0x000AU
#define DAC80501_TRIGGER_LDAC       (1U << 4)

/* VREF and output range */
#define DAC80501_VREF_INTERNAL_MV   2500U   /* 2.500 V internal reference */
#define DAC80501_VREF_GAIN          2U      /* output buffer gain x2 */
#define DAC80501_VOUT_MAX_MV        5000U   /* 2500 × 2 = 5000 mV */
#define DAC80501_RESOLUTION         65536U  /* 2^16 */

typedef struct {
    uint16_t current_code;  /* last DAC code written */
    float    vout_mv;       /* corresponding output voltage */
} DAC80501_Handle;

/* Public API */
bool     DAC80501_Init(DAC80501_Handle *h);
bool     DAC80501_SetCode(DAC80501_Handle *h, uint16_t code);
bool     DAC80501_SetVoltage(DAC80501_Handle *h, float target_mv);
uint16_t DAC80501_VoltsToCode(float mv);
float    DAC80501_CodeToVolts(uint16_t code);
bool     DAC80501_SoftReset(void);

#endif /* DAC80501_H */
