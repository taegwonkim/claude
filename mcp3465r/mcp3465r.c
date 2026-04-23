/**
 * mcp3465r.c
 *
 * MCP3465R 24-bit ADC driver + automatic offset/gain calibration.
 * Configured for 0–5 V single-ended input (VIN+ = CH0, VIN- = AGND).
 *
 * Hardware connections (SPI, CS active-low):
 *   MOSI  → SDI
 *   MISO  ← SDO
 *   SCLK  → SCK
 *   GPIOx → /CS
 *   (Optional) GPIOy ← /MDAT  – not used; driver polls IRQ register instead
 *
 * External VREF wiring (REQUIRED for 5 V full-scale):
 *   REFIN+ → 5 V precision reference (e.g. REF5050 or filtered AVDD)
 *   REFIN- → AGND
 *   AVDD   = 5 V  (inputs must not exceed AVDD + 0.3 V)
 *
 * Calibration math:
 *   corrected = (raw + OFFSETCAL) × GAINCAL / 2^23
 *
 *   Offset cal:  measure raw at ΔV=0 → OFFSETCAL = –mean(raw)
 *   Gain   cal:  measure raw at ΔV=VREF/2 → GAINCAL = 2^22 × 2^23 / mean(raw)
 */

#include "mcp3465r.h"

/* -------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

static inline void cs_assert(const MCP3465R_Handle_t *dev)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
}

static inline void cs_deassert(const MCP3465R_Handle_t *dev)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
}

/* -------------------------------------------------------------------------
 * Register I/O
 * ------------------------------------------------------------------------- */

MCP3465R_Status_t MCP3465R_WriteReg(MCP3465R_Handle_t *dev, uint8_t reg,
                                     const uint8_t *data, uint8_t len)
{
    uint8_t cmd = MCP3465R_CMD_BYTE(reg, MCP3465R_CMD_INC_WRITE);
    HAL_StatusTypeDef ret;

    cs_assert(dev);
    ret = HAL_SPI_Transmit(dev->hspi, &cmd, 1, dev->timeout_ms);
    if (ret == HAL_OK)
        ret = HAL_SPI_Transmit(dev->hspi, (uint8_t *)data, len, dev->timeout_ms);
    cs_deassert(dev);

    return (ret == HAL_OK) ? MCP3465R_OK : MCP3465R_ERR_SPI;
}

MCP3465R_Status_t MCP3465R_ReadReg(MCP3465R_Handle_t *dev, uint8_t reg,
                                    uint8_t *data, uint8_t len)
{
    uint8_t cmd = MCP3465R_CMD_BYTE(reg, MCP3465R_CMD_INC_READ);
    HAL_StatusTypeDef ret;

    cs_assert(dev);
    ret = HAL_SPI_Transmit(dev->hspi, &cmd, 1, dev->timeout_ms);
    if (ret == HAL_OK)
        ret = HAL_SPI_Receive(dev->hspi, data, len, dev->timeout_ms);
    cs_deassert(dev);

    return (ret == HAL_OK) ? MCP3465R_OK : MCP3465R_ERR_SPI;
}

/* -------------------------------------------------------------------------
 * Initialisation
 * ------------------------------------------------------------------------- */

MCP3465R_Status_t MCP3465R_Init(MCP3465R_Handle_t *dev)
{
    cs_deassert(dev);   /* Ensure CS is de-asserted on entry */

    MCP3465R_Status_t st;

    /*
     * CONFIG0: external VREF (5 V on REFIN+/REFIN-), internal clock,
     *          no bias-current source on inputs, continuous conversion.
     *          Internal 2.4 V VREF is NOT used – it cannot cover 5 V FS.
     */
    uint8_t cfg0 = CONFIG0_VREF_EXT | CONFIG0_CLK_INT | CONFIG0_CS_NONE
                 | CONFIG0_MODE_CONTINUOUS;
    st = MCP3465R_WriteReg(dev, MCP3465R_REG_CONFIG0, &cfg0, 1);
    if (st != MCP3465R_OK) return st;

    /*
     * CONFIG1: PRE=1, OSR=4096 → ~10 ms/conversion at 4.9 MHz oscillator.
     * High OSR suppresses noise during calibration.
     */
    uint8_t cfg1 = CONFIG1_PRE_1 | CONFIG1_OSR_4096;
    st = MCP3465R_WriteReg(dev, MCP3465R_REG_CONFIG1, &cfg1, 1);
    if (st != MCP3465R_OK) return st;

    /*
     * CONFIG2: normal boost (1×), PGA gain = 1, auto-zero MUX enabled.
     * AZ_MUX chops offset inside the modulator for lower inherent offset.
     */
    uint8_t cfg2 = CONFIG2_BOOST_1 | CONFIG2_GAIN_1 | CONFIG2_AZ_MUX;
    st = MCP3465R_WriteReg(dev, MCP3465R_REG_CONFIG2, &cfg2, 1);
    if (st != MCP3465R_OK) return st;

    /*
     * CONFIG3: continuous conversion, 24-bit data format,
     *          calibration registers disabled until MCP3465R_AutoCalibrate().
     */
    uint8_t cfg3 = CONFIG3_CONV_CONTINUOUS | CONFIG3_DATA_24BIT;
    st = MCP3465R_WriteReg(dev, MCP3465R_REG_CONFIG3, &cfg3, 1);
    if (st != MCP3465R_OK) return st;

    /* Reset calibration registers to identity values */
    st = MCP3465R_SetOffsetCal(dev, 0);
    if (st != MCP3465R_OK) return st;

    st = MCP3465R_SetGainCal(dev, MCP3465R_GAINCAL_UNITY);
    if (st != MCP3465R_OK) return st;

    /* Default MUX: single-ended – CH0 (VIN+) vs AGND (VIN-) for 0–5 V input */
    st = MCP3465R_SetMux(dev, MUX_VIP_CH0 | MUX_VIN_AGND);
    if (st != MCP3465R_OK) return st;

    return MCP3465R_OK;
}

/* -------------------------------------------------------------------------
 * Data-ready polling
 * ------------------------------------------------------------------------- */

MCP3465R_Status_t MCP3465R_WaitDataReady(MCP3465R_Handle_t *dev, uint32_t timeout_ms)
{
    uint32_t deadline = HAL_GetTick() + timeout_ms;
    uint8_t irq;

    do {
        MCP3465R_Status_t st = MCP3465R_ReadReg(dev, MCP3465R_REG_IRQ, &irq, 1);
        if (st != MCP3465R_OK) return st;

        /* IRQ_DR_STATUS is active-low: 0 means a fresh sample is ready */
        if (!(irq & IRQ_DR_STATUS))
            return MCP3465R_OK;

        HAL_Delay(1);
    } while (HAL_GetTick() < deadline);

    return MCP3465R_ERR_TIMEOUT;
}

/* -------------------------------------------------------------------------
 * ADC read
 * ------------------------------------------------------------------------- */

MCP3465R_Status_t MCP3465R_ReadADC(MCP3465R_Handle_t *dev, int32_t *value)
{
    MCP3465R_Status_t st = MCP3465R_WaitDataReady(dev, dev->timeout_ms);
    if (st != MCP3465R_OK) return st;

    uint8_t raw[3];
    st = MCP3465R_ReadReg(dev, MCP3465R_REG_ADCDATA, raw, sizeof(raw));
    if (st != MCP3465R_OK) return st;

    /* Reconstruct 24-bit value then sign-extend to int32 */
    int32_t code = ((uint32_t)raw[0] << 16) | ((uint32_t)raw[1] << 8) | raw[2];
    if (code & 0x800000L)
        code |= (int32_t)0xFF000000;

    *value = code;
    return MCP3465R_OK;
}

/* -------------------------------------------------------------------------
 * Register convenience wrappers
 * ------------------------------------------------------------------------- */

MCP3465R_Status_t MCP3465R_SetMux(MCP3465R_Handle_t *dev, uint8_t mux)
{
    return MCP3465R_WriteReg(dev, MCP3465R_REG_MUX, &mux, 1);
}

MCP3465R_Status_t MCP3465R_SetOffsetCal(MCP3465R_Handle_t *dev, int32_t offset)
{
    uint8_t buf[3] = {
        (uint8_t)((offset >> 16) & 0xFF),
        (uint8_t)((offset >>  8) & 0xFF),
        (uint8_t)( offset        & 0xFF),
    };
    return MCP3465R_WriteReg(dev, MCP3465R_REG_OFFSETCAL, buf, 3);
}

MCP3465R_Status_t MCP3465R_SetGainCal(MCP3465R_Handle_t *dev, uint32_t gain)
{
    uint8_t buf[3] = {
        (uint8_t)((gain >> 16) & 0xFF),
        (uint8_t)((gain >>  8) & 0xFF),
        (uint8_t)( gain        & 0xFF),
    };
    return MCP3465R_WriteReg(dev, MCP3465R_REG_GAINCAL, buf, 3);
}

MCP3465R_Status_t MCP3465R_EnableCalRegisters(MCP3465R_Handle_t *dev,
                                               bool en_offset, bool en_gain)
{
    uint8_t cfg3;
    MCP3465R_Status_t st = MCP3465R_ReadReg(dev, MCP3465R_REG_CONFIG3, &cfg3, 1);
    if (st != MCP3465R_OK) return st;

    cfg3 &= (uint8_t)~(CONFIG3_EN_OFFCAL | CONFIG3_EN_GAINCAL);
    if (en_offset) cfg3 |= CONFIG3_EN_OFFCAL;
    if (en_gain)   cfg3 |= CONFIG3_EN_GAINCAL;

    return MCP3465R_WriteReg(dev, MCP3465R_REG_CONFIG3, &cfg3, 1);
}

/* -------------------------------------------------------------------------
 * Internal: collect averaged sample
 *
 * Discards `discard` warm-up conversions, then sums `count` samples.
 * Uses int64 accumulator to avoid overflow (count up to 64, codes ±8 M).
 * ------------------------------------------------------------------------- */
static MCP3465R_Status_t collect_average(MCP3465R_Handle_t *dev,
                                          uint32_t discard,
                                          uint32_t count,
                                          int32_t *avg_out)
{
    MCP3465R_Status_t st;
    int32_t sample;

    for (uint32_t i = 0; i < discard; i++) {
        st = MCP3465R_ReadADC(dev, &sample);
        if (st != MCP3465R_OK) return st;
    }

    int64_t sum = 0;
    for (uint32_t i = 0; i < count; i++) {
        st = MCP3465R_ReadADC(dev, &sample);
        if (st != MCP3465R_OK) return st;
        sum += sample;
    }

    *avg_out = (int32_t)(sum / (int64_t)count);
    return MCP3465R_OK;
}

/* -------------------------------------------------------------------------
 * Automatic two-phase calibration
 * ------------------------------------------------------------------------- */

MCP3465R_Status_t MCP3465R_AutoCalibrate(MCP3465R_Handle_t *dev,
                                          MCP3465R_CalResult_t *result)
{
    MCP3465R_Status_t st;
    MCP3465R_CalResult_t local = {0};

    /* ---- Reset to a known baseline ----------------------------------- */
    st = MCP3465R_EnableCalRegisters(dev, false, false);
    if (st != MCP3465R_OK) return st;

    st = MCP3465R_SetOffsetCal(dev, 0);
    if (st != MCP3465R_OK) return st;

    st = MCP3465R_SetGainCal(dev, MCP3465R_GAINCAL_UNITY);
    if (st != MCP3465R_OK) return st;

    /* ==================================================================
     * PHASE 1 – Offset calibration
     *
     * Route both ADC inputs to internal AGND (MUX = 0x88).
     * Ideal output code = 0.  Any non-zero mean is the offset error.
     * Correction: OFFSETCAL = –mean   (hardware: corrected = raw + OFFSETCAL)
     * ================================================================== */
    st = MCP3465R_SetMux(dev, MUX_OFFSET_CAL);
    if (st != MCP3465R_OK) return st;

    HAL_Delay(20);  /* Input RC settling + at least two conversion periods */

    int32_t offset_mean;
    st = collect_average(dev, MCP3465R_CAL_DISCARD, MCP3465R_CAL_SAMPLES, &offset_mean);
    if (st != MCP3465R_OK) return st;

    local.offset_raw    = offset_mean;
    local.offsetcal_val = -offset_mean;

    st = MCP3465R_SetOffsetCal(dev, local.offsetcal_val);
    if (st != MCP3465R_OK) return st;

    /* Activate offset correction so gain phase sees an offset-compensated result */
    st = MCP3465R_EnableCalRegisters(dev, true, false);
    if (st != MCP3465R_OK) return st;

    /* ==================================================================
     * PHASE 2 – Gain calibration
     *
     * Route VIN+ → REFIN+/2, VIN- → REFIN-/2  (MUX = 0xBC).
     * Differential input = (REFIN+ – REFIN–)/2 = VREF/2.
     * Ideal 24-bit code  = VREF/2 / VREF × 2^23 = 2^22 = 0x400000.
     *
     * GAINCAL = ideal_code × GAINCAL_UNITY / measured_code
     *         = 0x400000  × 0x800000      / measured_code
     *
     * int64 arithmetic avoids overflow (product ~2^45).
     * ================================================================== */
    st = MCP3465R_SetMux(dev, MUX_GAIN_CAL);
    if (st != MCP3465R_OK) return st;

    HAL_Delay(20);

    int32_t gain_mean;
    st = collect_average(dev, MCP3465R_CAL_DISCARD, MCP3465R_CAL_SAMPLES, &gain_mean);
    if (st != MCP3465R_OK) return st;

    local.gain_raw = gain_mean;

    /* Sanity check: measured code must be within ±15 % of ideal 0x400000 */
    int32_t expected  = MCP3465R_GAIN_CAL_EXPECTED;
    int32_t tolerance = (expected * MCP3465R_CAL_GAIN_TOL_PCT) / 100;
    if (gain_mean < (expected - tolerance) || gain_mean > (expected + tolerance))
        return MCP3465R_ERR_CAL;   /* Bad wiring or missing VREF – abort */

    int64_t gaincal64 = ((int64_t)expected * (int64_t)MCP3465R_GAINCAL_UNITY)
                        / (int64_t)gain_mean;

    /* Clamp to 24-bit unsigned range */
    if (gaincal64 <= 0 || gaincal64 > 0xFFFFFF)
        return MCP3465R_ERR_CAL;

    local.gaincal_val = (uint32_t)gaincal64;

    st = MCP3465R_SetGainCal(dev, local.gaincal_val);
    if (st != MCP3465R_OK) return st;

    /* Activate both calibration registers */
    st = MCP3465R_EnableCalRegisters(dev, true, true);
    if (st != MCP3465R_OK) return st;

    /* Restore MUX to normal single-ended input: CH0 vs AGND */
    st = MCP3465R_SetMux(dev, MUX_VIP_CH0 | MUX_VIN_AGND);
    if (st != MCP3465R_OK) return st;

    if (result)
        *result = local;

    return MCP3465R_OK;
}

/* -------------------------------------------------------------------------
 * Re-apply calibration from NVM / flash without re-measuring
 * ------------------------------------------------------------------------- */

MCP3465R_Status_t MCP3465R_ApplyStoredCal(MCP3465R_Handle_t *dev,
                                            const MCP3465R_CalResult_t *result)
{
    MCP3465R_Status_t st;

    st = MCP3465R_EnableCalRegisters(dev, false, false);
    if (st != MCP3465R_OK) return st;

    st = MCP3465R_SetOffsetCal(dev, result->offsetcal_val);
    if (st != MCP3465R_OK) return st;

    st = MCP3465R_SetGainCal(dev, result->gaincal_val);
    if (st != MCP3465R_OK) return st;

    return MCP3465R_EnableCalRegisters(dev, true, true);
}
