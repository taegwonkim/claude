#include "mcp3465r.h"
#include <string.h>

/* -----------------------------------------------------------------------
 * Low-level register helpers
 * ----------------------------------------------------------------------- */

static bool mcp_write_reg(uint8_t reg, const uint8_t *data, uint8_t len)
{
    uint8_t cmd = MCP3465R_CMD(MCP3465R_ADDR, reg, MCP3465R_CMD_INC_WRITE);
    uint8_t buf[9] = {0};
    buf[0] = cmd;
    for (uint8_t i = 0; i < len && i < 8; i++) buf[i + 1] = data[i];

    SPI_CS_Assert(SPI_DEVICE_MCP3465R);
    SPI_Status s = SPI_TransmitReceive(SPI_DEVICE_MCP3465R, buf, NULL, len + 1);
    SPI_CS_Deassert(SPI_DEVICE_MCP3465R);
    return (s == SPI_OK);
}

static bool mcp_read_reg(uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t cmd = MCP3465R_CMD(MCP3465R_ADDR, reg, MCP3465R_CMD_STAT_READ);
    uint8_t tx[9] = {0};
    uint8_t rx[9] = {0};
    tx[0] = cmd;

    SPI_CS_Assert(SPI_DEVICE_MCP3465R);
    SPI_Status s = SPI_TransmitReceive(SPI_DEVICE_MCP3465R, tx, rx, len + 1);
    SPI_CS_Deassert(SPI_DEVICE_MCP3465R);

    if (s != SPI_OK) return false;
    for (uint8_t i = 0; i < len; i++) data[i] = rx[i + 1];
    return true;
}

static bool mcp_fast_cmd(uint8_t fast_cmd)
{
    uint8_t cmd = MCP3465R_CMD(MCP3465R_ADDR, 0, fast_cmd);
    uint8_t rx;
    SPI_CS_Assert(SPI_DEVICE_MCP3465R);
    SPI_Status s = SPI_TransmitReceive(SPI_DEVICE_MCP3465R, &cmd, &rx, 1);
    SPI_CS_Deassert(SPI_DEVICE_MCP3465R);
    return (s == SPI_OK);
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

bool MCP3465R_Init(MCP3465R_Handle *h)
{
    memset(h, 0, sizeof(*h));

    /* Full device reset */
    if (!mcp_fast_cmd(MCP3465R_CMD_FULL_RESET)) return false;
    delay_us(500);

    /* CONFIG0: External Vref, internal clock (no CLKOUT), no current src,
     *          continuous conversion mode                                  */
    uint8_t cfg0 = MCP3465R_CONFIG0_VREF_EXT |
                   MCP3465R_CONFIG0_CLK_INT   |
                   MCP3465R_CONFIG0_CS_NONE   |
                   MCP3465R_CONFIG0_MODE_CONT;
    if (!mcp_write_reg(MCP3465R_REG_CONFIG0, &cfg0, 1)) return false;

    /* CONFIG1: OSR=4096 for best noise performance, AMCLK /1 */
    uint8_t cfg1 = MCP3465R_CONFIG1_OSR_4096 | MCP3465R_CONFIG1_AMCLK_DIV1;
    if (!mcp_write_reg(MCP3465R_REG_CONFIG1, &cfg1, 1)) return false;

    /* CONFIG2: Boost x1, PGA x1, auto-zero MUX enable */
    uint8_t cfg2 = MCP3465R_CONFIG2_BOOST_X1 |
                   MCP3465R_CONFIG2_GAIN_X1   |
                   MCP3465R_CONFIG2_AZ_MUX_EN;
    if (!mcp_write_reg(MCP3465R_REG_CONFIG2, &cfg2, 1)) return false;

    /* CONFIG3: continuous conversion, 32-bit sign-extended output,
     *          enable HW offset & gain calibration registers              */
    uint8_t cfg3 = MCP3465R_CONFIG3_CONV_MODE_CONT |
                   MCP3465R_CONFIG3_DATA_FORMAT_32  |
                   MCP3465R_CONFIG3_EN_OFFCAL        |
                   MCP3465R_CONFIG3_EN_GAINCAL;
    if (!mcp_write_reg(MCP3465R_REG_CONFIG3, &cfg3, 1)) return false;

    /* IRQ: IRQ pin active-low, push-pull */
    uint8_t irq = 0x06U;
    if (!mcp_write_reg(MCP3465R_REG_IRQ, &irq, 1)) return false;

    /* Default channel: CH0+ / AGND- */
    if (!MCP3465R_SetChannel(h, 0, 8)) return false;

    /* Initialise HW calibration registers to unity */
    if (!MCP3465R_WriteOffsetCal(h, 0))         return false;
    if (!MCP3465R_WriteGainCal(h, 0x800000U))   return false;  /* 1.0 × */

    return true;
}

bool MCP3465R_SetChannel(MCP3465R_Handle *h, uint8_t pos_ch, uint8_t neg_ch)
{
    (void)h;
    uint8_t mux = (uint8_t)((pos_ch << 4) | (neg_ch & 0x0FU));
    return mcp_write_reg(MCP3465R_REG_MUX, &mux, 1);
}

bool MCP3465R_ReadRaw(MCP3465R_Handle *h, int32_t *raw_out)
{
    (void)h;
    /* Poll ADCDATA register (4 bytes in 32-bit format) */
    const uint32_t timeout = 500000U;
    uint32_t t = timeout;

    /* Wait for data-ready: STATUS bit[2] (DR) goes low in the status byte */
    uint8_t status;
    do {
        if (!mcp_read_reg(MCP3465R_REG_ADCDATA, &status, 1)) return false;
        if (--t == 0) return false;
    } while (status & 0x04U);   /* DR bit: 0 = data ready */

    /* Read full 4-byte ADC result */
    uint8_t raw[4];
    if (!mcp_read_reg(MCP3465R_REG_ADCDATA, raw, 4)) return false;

    /* Bytes 0-3 are [status | B2 | B1 | B0]; actual 24-bit result in B2:B0 */
    int32_t val = ((int32_t)raw[1] << 16) |
                  ((int32_t)raw[2] <<  8) |
                  ((int32_t)raw[3]);

    /* Sign-extend 24-bit → 32-bit */
    if (val & 0x800000U) val |= (int32_t)0xFF000000;

    *raw_out = val;
    return true;
}

float MCP3465R_RawToMillivolts(const MCP3465R_Handle *h, int32_t raw)
{
    (void)h;
    /* V = raw / 2^23 × VREF  (bipolar, 24-bit sign-extended) */
    return (float)raw * (float)MCP3465R_VREF_MV / 8388608.0f;
}

bool MCP3465R_WriteOffsetCal(MCP3465R_Handle *h, int32_t offset_val)
{
    h->hw_cal.offset_cal = offset_val;
    uint8_t buf[3] = {
        (uint8_t)((offset_val >> 16) & 0xFF),
        (uint8_t)((offset_val >>  8) & 0xFF),
        (uint8_t)( offset_val        & 0xFF)
    };
    return mcp_write_reg(MCP3465R_REG_OFFSETCAL, buf, 3);
}

bool MCP3465R_WriteGainCal(MCP3465R_Handle *h, uint32_t gain_val)
{
    h->hw_cal.gain_cal = gain_val;
    uint8_t buf[3] = {
        (uint8_t)((gain_val >> 16) & 0xFF),
        (uint8_t)((gain_val >>  8) & 0xFF),
        (uint8_t)( gain_val        & 0xFF)
    };
    return mcp_write_reg(MCP3465R_REG_GAINCAL, buf, 3);
}

/*
 * Offset calibration register: two's-complement 24-bit.
 * offset_mv: the voltage error (in mV) that should be subtracted.
 */
int32_t MCP3465R_CalcOffsetRegVal(float offset_mv)
{
    float lsb_mv = (float)MCP3465R_VREF_MV / 8388608.0f;
    int32_t val  = (int32_t)(offset_mv / lsb_mv);
    /* Clamp to 24-bit signed range */
    if (val >  0x7FFFFF) val =  0x7FFFFF;
    if (val < -0x800000) val = -0x800000;
    return val;
}

/*
 * Gain calibration register: unsigned 24-bit, 0x800000 = 1.000000×.
 * Corrects for gain error: gain_reg = 0x800000 × (expected / measured).
 */
uint32_t MCP3465R_CalcGainRegVal(float measured_full_mv, float expected_full_mv)
{
    if (measured_full_mv < 1.0f) measured_full_mv = 1.0f;  /* guard /0 */
    float factor = expected_full_mv / measured_full_mv;
    uint32_t val = (uint32_t)(factor * 8388608.0f + 0.5f);
    if (val > 0xFFFFFFU) val = 0xFFFFFFU;
    return val;
}
