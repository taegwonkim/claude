#include "dac80501.h"
#include <string.h>

/* -----------------------------------------------------------------------
 * SPI frame helpers (24-bit: cmd[7:0] | data[15:8] | data[7:0])
 * ----------------------------------------------------------------------- */

static bool dac_write_reg(uint8_t reg, uint16_t data)
{
    uint8_t tx[3] = {
        (uint8_t)(DAC80501_CMD_WRITE | (reg & 0x7FU)),
        (uint8_t)((data >> 8) & 0xFFU),
        (uint8_t)( data       & 0xFFU)
    };
    SPI_CS_Assert(SPI_DEVICE_DAC80501);
    SPI_Status s = SPI_TransmitReceive(SPI_DEVICE_DAC80501, tx, NULL, 3);
    SPI_CS_Deassert(SPI_DEVICE_DAC80501);
    return (s == SPI_OK);
}

static bool dac_read_reg(uint8_t reg, uint16_t *out)
{
    uint8_t tx[3] = {
        (uint8_t)(DAC80501_CMD_READ | (reg & 0x7FU)),
        0x00U, 0x00U
    };
    uint8_t rx[3] = {0};

    SPI_CS_Assert(SPI_DEVICE_DAC80501);
    SPI_Status s = SPI_TransmitReceive(SPI_DEVICE_DAC80501, tx, rx, 3);
    SPI_CS_Deassert(SPI_DEVICE_DAC80501);

    if (s != SPI_OK) return false;
    *out = (uint16_t)((rx[1] << 8) | rx[2]);
    return true;
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

bool DAC80501_SoftReset(void)
{
    return dac_write_reg(DAC80501_REG_TRIGGER, DAC80501_TRIGGER_SOFT_RESET);
}

bool DAC80501_Init(DAC80501_Handle *h)
{
    memset(h, 0, sizeof(*h));

    if (!DAC80501_SoftReset()) return false;
    delay_us(1000);  /* tPOR max = 1 ms */

    /* CONFIG: keep internal VREF powered on */
    if (!dac_write_reg(DAC80501_REG_CONFIG, 0x0000U)) return false;

    /* GAIN: BUFF_GAIN = x2 (REF_DIV = 0, so Vout_max = VREF × 2 = 5V) */
    uint16_t gain = DAC80501_GAIN_BUFF_GAIN_X2;
    if (!dac_write_reg(DAC80501_REG_GAIN, gain)) return false;

    /* Power-on the DAC output to 0 V */
    if (!DAC80501_SetCode(h, 0x0000U)) return false;

    return true;
}

bool DAC80501_SetCode(DAC80501_Handle *h, uint16_t code)
{
    if (!dac_write_reg(DAC80501_REG_DAC, code)) return false;
    h->current_code = code;
    h->vout_mv = DAC80501_CodeToVolts(code);
    return true;
}

bool DAC80501_SetVoltage(DAC80501_Handle *h, float target_mv)
{
    uint16_t code = DAC80501_VoltsToCode(target_mv);
    return DAC80501_SetCode(h, code);
}

uint16_t DAC80501_VoltsToCode(float mv)
{
    if (mv < 0.0f) mv = 0.0f;
    if (mv > (float)DAC80501_VOUT_MAX_MV) mv = (float)DAC80501_VOUT_MAX_MV;
    uint32_t code = (uint32_t)((mv / (float)DAC80501_VOUT_MAX_MV) *
                               (float)(DAC80501_RESOLUTION - 1U) + 0.5f);
    if (code > 0xFFFFU) code = 0xFFFFU;
    return (uint16_t)code;
}

float DAC80501_CodeToVolts(uint16_t code)
{
    return ((float)code / (float)(DAC80501_RESOLUTION - 1U)) *
            (float)DAC80501_VOUT_MAX_MV;
}
