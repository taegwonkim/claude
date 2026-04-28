/**
 * MCP3465R ADC driver implementation.
 * See mcp3465r.h for full documentation.
 */
#include "mcp3465r.h"
#include <string.h>

/* ── CS helpers ─────────────────────────────────────────────────────────── */
static inline void cs_low(void)
{
    HAL_GPIO_WritePin(MCP3465R_CS_GPIO, MCP3465R_CS_PIN, GPIO_PIN_RESET);
}

static inline void cs_high(void)
{
    HAL_GPIO_WritePin(MCP3465R_CS_GPIO, MCP3465R_CS_PIN, GPIO_PIN_SET);
}

/* ── Low-level register access ──────────────────────────────────────────── */

/**
 * Write 1-byte register (CONFIG0..CONFIG3, IRQ, MUX).
 */
static HAL_StatusTypeDef write_reg8(uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { MCP3465R_CMD(reg, MCP3465R_TYPE_IWRITE), value };
    cs_low();
    HAL_StatusTypeDef st = HAL_SPI_Transmit(&MCP3465R_SPI, tx, 2,
                                             MCP3465R_SPI_TIMEOUT_MS);
    cs_high();
    return st;
}

/**
 * Write 3-byte register (OFFSETCAL, GAINCAL, TIMER).
 * Data is transmitted MSB first.
 */
static HAL_StatusTypeDef write_reg24(uint8_t reg, uint32_t value)
{
    uint8_t tx[4] = {
        MCP3465R_CMD(reg, MCP3465R_TYPE_IWRITE),
        (uint8_t)((value >> 16) & 0xFF),
        (uint8_t)((value >>  8) & 0xFF),
        (uint8_t)( value        & 0xFF)
    };
    cs_low();
    HAL_StatusTypeDef st = HAL_SPI_Transmit(&MCP3465R_SPI, tx, 4,
                                             MCP3465R_SPI_TIMEOUT_MS);
    cs_high();
    return st;
}

/**
 * Read 1-byte register.
 */
static HAL_StatusTypeDef read_reg8(uint8_t reg, uint8_t *value)
{
    uint8_t tx = MCP3465R_CMD(reg, MCP3465R_TYPE_SREAD);
    uint8_t rx;
    cs_low();
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&MCP3465R_SPI,
                                                    &tx, &rx, 1,
                                                    MCP3465R_SPI_TIMEOUT_MS);
    /* status byte is returned in the first byte; read one more for the data */
    if (st == HAL_OK) {
        uint8_t dummy = 0xFF;
        st = HAL_SPI_TransmitReceive(&MCP3465R_SPI, &dummy, value, 1,
                                     MCP3465R_SPI_TIMEOUT_MS);
    }
    cs_high();
    (void)rx;
    return st;
}

/**
 * Read 3-byte register.
 */
static HAL_StatusTypeDef read_reg24(uint8_t reg, uint32_t *value)
{
    uint8_t tx[4] = { MCP3465R_CMD(reg, MCP3465R_TYPE_SREAD), 0xFF, 0xFF, 0xFF };
    uint8_t rx[4] = {0};
    cs_low();
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&MCP3465R_SPI, tx, rx, 4,
                                                    MCP3465R_SPI_TIMEOUT_MS);
    cs_high();
    if (st == HAL_OK) {
        /* rx[0] = status byte, rx[1..3] = register data MSB first */
        *value = ((uint32_t)rx[1] << 16) |
                 ((uint32_t)rx[2] <<  8) |
                  (uint32_t)rx[3];
    }
    return st;
}

/**
 * Send a fast command (no data bytes).
 */
static HAL_StatusTypeDef fast_cmd(uint8_t fcmd)
{
    uint8_t tx = MCP3465R_CMD(fcmd, MCP3465R_TYPE_FAST);
    cs_low();
    HAL_StatusTypeDef st = HAL_SPI_Transmit(&MCP3465R_SPI, &tx, 1,
                                             MCP3465R_SPI_TIMEOUT_MS);
    cs_high();
    return st;
}

/* ── Public API ─────────────────────────────────────────────────────────── */

HAL_StatusTypeDef MCP3465R_Reset(void)
{
    HAL_StatusTypeDef st = fast_cmd(MCP3465R_FCMD_RESET);
    HAL_Delay(1); /* tRESET ≥ 100 µs */
    return st;
}

HAL_StatusTypeDef MCP3465R_Init(void)
{
    cs_high(); /* ensure CS is deasserted */
    HAL_Delay(1);

    HAL_StatusTypeDef st;

    /* CONFIG0: external VREF, internal oscillator, continuous conversion */
    uint8_t cfg0 = MCP3465R_CFG0_VREF_EXT |
                   MCP3465R_CFG0_CLK_INT   |
                   MCP3465R_CFG0_CS_NONE   |
                   MCP3465R_CFG0_MODE_CONT;
    st = write_reg8(MCP3465R_REG_CONFIG0, cfg0);
    if (st != HAL_OK) return st;

    /* CONFIG1: prescaler ÷1, OSR=1024 → good noise for calibration */
    uint8_t cfg1 = MCP3465R_CFG1_PRE_1 | MCP3465R_CFG1_OSR_1024;
    st = write_reg8(MCP3465R_REG_CONFIG1, cfg1);
    if (st != HAL_OK) return st;

    /* CONFIG2: boost ×1, analog gain ×1, auto-zero MUX off */
    uint8_t cfg2 = MCP3465R_CFG2_BOOST_1X |
                   MCP3465R_CFG2_GAIN_1    |
                   MCP3465R_CFG2_AZMUX_OFF;
    st = write_reg8(MCP3465R_REG_CONFIG2, cfg2);
    if (st != HAL_OK) return st;

    /* CONFIG3: continuous mode, 32-bit format with CH_ID, cal disabled initially */
    uint8_t cfg3 = MCP3465R_CFG3_CONV_CONT | MCP3465R_CFG3_FMT_32CH;
    st = write_reg8(MCP3465R_REG_CONFIG3, cfg3);
    if (st != HAL_OK) return st;

    /* MUX: CH0+ vs AGND− (single-ended) */
    st = write_reg8(MCP3465R_REG_MUX, MCP3465R_MUX_CH0_GND);
    if (st != HAL_OK) return st;

    /* Reset calibration registers to defaults */
    st = write_reg24(MCP3465R_REG_OFFSETCAL, 0x000000UL);
    if (st != HAL_OK) return st;
    st = write_reg24(MCP3465R_REG_GAINCAL,   MCP3465R_GAINCAL_UNITY);
    if (st != HAL_OK) return st;

    HAL_Delay(5);
    return HAL_OK;
}

HAL_StatusTypeDef MCP3465R_SetCalibrationEnable(bool gain_en, bool offset_en)
{
    uint8_t cfg3;
    HAL_StatusTypeDef st = read_reg8(MCP3465R_REG_CONFIG3, &cfg3);
    if (st != HAL_OK) return st;

    cfg3 &= ~(MCP3465R_CFG3_EN_GCAL | MCP3465R_CFG3_EN_OCAL);
    if (gain_en)   cfg3 |= MCP3465R_CFG3_EN_GCAL;
    if (offset_en) cfg3 |= MCP3465R_CFG3_EN_OCAL;

    return write_reg8(MCP3465R_REG_CONFIG3, cfg3);
}

/**
 * Read one conversion result.
 * DATA_FORMAT=10: 32-bit word = [31:28] CH_ID | [27:24] sgn | [23:0] ADC data.
 * Returns sign-extended 24-bit value in out_code.
 */
HAL_StatusTypeDef MCP3465R_ReadRaw24(int32_t *out_code)
{
    /* Poll IRQ bit in STATUS byte by issuing a static read of ADCDATA */
    uint8_t tx[5] = { MCP3465R_CMD(MCP3465R_REG_ADCDATA, MCP3465R_TYPE_SREAD),
                      0xFF, 0xFF, 0xFF, 0xFF };
    uint8_t rx[5] = {0};

    /* Wait until DR_STATUS bit (bit 2 of status byte) is 0 (data ready) */
    uint32_t timeout = HAL_GetTick() + 500U;
    uint8_t status;
    do {
        cs_low();
        HAL_SPI_TransmitReceive(&MCP3465R_SPI, tx, rx, 1, MCP3465R_SPI_TIMEOUT_MS);
        cs_high();
        status = rx[0];
        if (HAL_GetTick() > timeout) return HAL_TIMEOUT;
    } while (status & 0x04U); /* DR_STATUS=1 means NOT ready */

    /* Now read full 4-byte ADCDATA */
    cs_low();
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&MCP3465R_SPI, tx, rx, 5,
                                                    MCP3465R_SPI_TIMEOUT_MS);
    cs_high();
    if (st != HAL_OK) return st;

    /* rx[0]=status, rx[1..4]=data MSB first */
    uint32_t raw32 = ((uint32_t)rx[1] << 24) |
                     ((uint32_t)rx[2] << 16) |
                     ((uint32_t)rx[3] <<  8) |
                      (uint32_t)rx[4];

    /* Extract lower 24-bit ADC data and sign-extend to int32 */
    uint32_t code24 = raw32 & 0x00FFFFFFU;
    if (code24 & 0x00800000U) {
        *out_code = (int32_t)(code24 | 0xFF000000U); /* negative */
    } else {
        *out_code = (int32_t)code24;
    }
    return HAL_OK;
}

HAL_StatusTypeDef MCP3465R_ReadAverage(uint16_t n_samples, int32_t *out_avg)
{
    int64_t sum = 0;
    for (uint16_t i = 0; i < n_samples; i++) {
        int32_t code;
        HAL_StatusTypeDef st = MCP3465R_ReadRaw24(&code);
        if (st != HAL_OK) return st;
        sum += code;
    }
    *out_avg = (int32_t)(sum / n_samples);
    return HAL_OK;
}

HAL_StatusTypeDef MCP3465R_WriteOffsetCal(int32_t offset_24)
{
    /* Store as 24-bit two's complement */
    uint32_t reg_val = (uint32_t)(offset_24) & 0x00FFFFFFU;
    return write_reg24(MCP3465R_REG_OFFSETCAL, reg_val);
}

HAL_StatusTypeDef MCP3465R_WriteGainCal(uint32_t gain_24)
{
    return write_reg24(MCP3465R_REG_GAINCAL, gain_24 & 0x00FFFFFFU);
}

HAL_StatusTypeDef MCP3465R_ReadOffsetCal(int32_t *offset_24)
{
    uint32_t raw;
    HAL_StatusTypeDef st = read_reg24(MCP3465R_REG_OFFSETCAL, &raw);
    if (st != HAL_OK) return st;
    /* Sign-extend 24-bit value */
    if (raw & 0x800000U)
        *offset_24 = (int32_t)(raw | 0xFF000000U);
    else
        *offset_24 = (int32_t)raw;
    return HAL_OK;
}

HAL_StatusTypeDef MCP3465R_ReadGainCal(uint32_t *gain_24)
{
    return read_reg24(MCP3465R_REG_GAINCAL, gain_24);
}

/**
 * Convert a 24-bit signed ADC code to voltage.
 *
 * In single-ended mode (CH0 vs AGND), the positive full-scale code = 2^23 - 1.
 *   V = code × VREF / (2^23 - 1)
 */
float MCP3465R_Code24ToVoltage(int32_t code, float vref)
{
    return (float)code * vref / (float)MCP3465R_FULL_SCALE_24;
}
