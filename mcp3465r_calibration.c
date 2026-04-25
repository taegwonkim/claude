/**
 * MCP3465R Automatic Calibration Implementation
 *
 * Calibration algorithm overview:
 *
 *  ┌────────────────────────────────────────────────────────────────┐
 *  │ STEP 1 – Offset Calibration                                    │
 *  │  MUX → AGND / AGND  (differential = 0 V)                      │
 *  │  Ideal code = 0.  Any deviation is offset error.               │
 *  │  OFFSETCAL = −mean(samples)                                    │
 *  │                                                                │
 *  │ STEP 2 – Gain Calibration                                      │
 *  │  MUX → REFIN+ / AGND  (= VREF = 3.000 V)                      │
 *  │  Ideal code = 0x7FFFFF (+full scale).                          │
 *  │  GAINCAL = (0x7FFFFF / mean(samples)) × 0x800000              │
 *  │  (Q1.23 fixed-point, 0x800000 = 1.000000)                     │
 *  │                                                                │
 *  │ Hardware corrects every sample:                                │
 *  │  OUT = (raw + OFFSETCAL) × GAINCAL / 0x800000                 │
 *  └────────────────────────────────────────────────────────────────┘
 */

#include "mcp3465r_calibration.h"

#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

/* ------------------------------------------------------------------ */
/*  Private helpers                                                     */
/* ------------------------------------------------------------------ */

static inline void cs_assert  (MCP3465R_Handle_t *h)
{
    HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_RESET);
}
static inline void cs_deassert(MCP3465R_Handle_t *h)
{
    HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_SET);
}

/**
 * Acquire SPI mutex (blocks up to 100 ms) and assert CS.
 * Returns false if mutex could not be obtained.
 */
static bool bus_acquire(MCP3465R_Handle_t *h)
{
    if (xSemaphoreTake(h->spi_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
        return false;
    cs_assert(h);
    return true;
}

static void bus_release(MCP3465R_Handle_t *h)
{
    cs_deassert(h);
    xSemaphoreGive(h->spi_mutex);
}

/* ------------------------------------------------------------------ */
/*  Register access                                                     */
/* ------------------------------------------------------------------ */

/**
 * Write nbytes (1–3) of val MSB-first into register reg.
 * SPI frame: [CMD_BYTE][D2][D1][D0]
 */
MCP3465R_CalStatus_t MCP3465R_WriteReg(MCP3465R_Handle_t *h,
                                        uint8_t reg, uint32_t val, uint8_t nbytes)
{
    uint8_t buf[4];
    buf[0] = MCP3465R_CMD_BYTE(h->dev_addr, reg, MCP3465R_CMD_INCR_WRITE);

    /* Pack MSB-first into remaining bytes */
    for (int8_t i = (int8_t)(nbytes - 1); i >= 0; --i)
        buf[nbytes - (uint8_t)i] = (uint8_t)(val >> (8u * (uint8_t)i));

    if (!bus_acquire(h))
        return MCP3465R_CAL_TIMEOUT;

    HAL_StatusTypeDef rc =
        HAL_SPI_Transmit(h->hspi, buf, 1u + nbytes, MCP3465R_SPI_TIMEOUT_MS);

    bus_release(h);
    return (rc == HAL_OK) ? MCP3465R_CAL_OK : MCP3465R_CAL_SPI_ERR;
}

/**
 * Read nbytes (1–3) from register reg into *val (MSB-first reassembly).
 */
MCP3465R_CalStatus_t MCP3465R_ReadReg(MCP3465R_Handle_t *h,
                                       uint8_t reg, uint32_t *val, uint8_t nbytes)
{
    uint8_t tx[4] = {0};
    uint8_t rx[4] = {0};

    tx[0] = MCP3465R_CMD_BYTE(h->dev_addr, reg, MCP3465R_CMD_STATIC_READ);

    if (!bus_acquire(h))
        return MCP3465R_CAL_TIMEOUT;

    HAL_StatusTypeDef rc =
        HAL_SPI_TransmitReceive(h->hspi, tx, rx, 1u + nbytes,
                                MCP3465R_SPI_TIMEOUT_MS);
    bus_release(h);

    if (rc != HAL_OK)
        return MCP3465R_CAL_SPI_ERR;

    /* Reassemble MSB-first bytes (rx[0] = status byte, rx[1..n] = data) */
    uint32_t result = 0u;
    for (uint8_t i = 1u; i <= nbytes; ++i)
        result = (result << 8) | rx[i];

    *val = result;
    return MCP3465R_CAL_OK;
}

/* ------------------------------------------------------------------ */
/*  Conversion helpers                                                  */
/* ------------------------------------------------------------------ */

/**
 * Wait for DR̄DY̅ assertion by polling IRQ register bit 2.
 * Blocks up to timeout_ms.  Returns false on timeout or SPI error.
 */
static bool wait_drdy(MCP3465R_Handle_t *h, uint32_t timeout_ms)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);

    while (xTaskGetTickCount() < deadline) {
        uint32_t irq_val = 0;
        if (MCP3465R_ReadReg(h, MCP3465R_REG_IRQ, &irq_val, 1) != MCP3465R_CAL_OK)
            return false;

        /* DR̄DY̅ is bit 2; active-low → data ready when = 0 */
        if ((irq_val & (1u << 2)) == 0u)
            return true;

        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return false;
}

/**
 * Trigger a single one-shot conversion and return the raw 24-bit
 * signed ADC code.  Assumes ADC is already configured and in standby.
 *
 * One-shot mode: ADC_MODE = 11 (start), automatically returns to standby.
 */
static MCP3465R_CalStatus_t read_single_conversion(MCP3465R_Handle_t *h,
                                                    int32_t           *out)
{
    /* Send FAST START command to trigger one-shot conversion */
    if (!bus_acquire(h))
        return MCP3465R_CAL_TIMEOUT;

    uint8_t cmd = MCP3465R_FAST_START;
    HAL_StatusTypeDef rc = HAL_SPI_Transmit(h->hspi, &cmd, 1u,
                                             MCP3465R_SPI_TIMEOUT_MS);
    bus_release(h);

    if (rc != HAL_OK)
        return MCP3465R_CAL_SPI_ERR;

    /* Wait for conversion complete (DR̄DY̅ low), up to 200 ms */
    if (!wait_drdy(h, 200u))
        return MCP3465R_CAL_TIMEOUT;

    /* Read 24-bit ADCDATA register */
    uint32_t raw = 0;
    MCP3465R_CalStatus_t st =
        MCP3465R_ReadReg(h, MCP3465R_REG_ADCDATA, &raw, 3);

    if (st != MCP3465R_CAL_OK)
        return st;

    /* Sign-extend 24-bit → 32-bit */
    if (raw & 0x800000u)
        *out = (int32_t)(raw | 0xFF000000u);
    else
        *out = (int32_t)raw;

    return MCP3465R_CAL_OK;
}

/**
 * Average MCP3465R_CAL_SAMPLES conversions.
 * Returns MCP3465R_CAL_SATURATION if any sample hits ±full-scale
 * (indicates wrong MUX or power issue).
 */
static MCP3465R_CalStatus_t average_samples(MCP3465R_Handle_t *h,
                                             int32_t           *mean_out)
{
    int64_t  accum = 0;
    int32_t  sample;

    for (uint32_t i = 0u; i < MCP3465R_CAL_SAMPLES; ++i) {
        MCP3465R_CalStatus_t st = read_single_conversion(h, &sample);
        if (st != MCP3465R_CAL_OK)
            return st;

        /* Reject saturated values — they indicate a hardware problem */
        if (sample >= MCP3465R_ADC_FULLSCALE || sample <= -MCP3465R_ADC_FULLSCALE)
            return MCP3465R_CAL_SATURATION;

        accum += (int64_t)sample;
    }

    *mean_out = (int32_t)(accum / (int64_t)MCP3465R_CAL_SAMPLES);
    return MCP3465R_CAL_OK;
}

/* ------------------------------------------------------------------ */
/*  Configuration helpers                                               */
/* ------------------------------------------------------------------ */

/**
 * Write the 4 CONFIG registers once.
 * Called from Init and repeated at the start of each cal step to ensure
 * a known state.
 *
 * CONFIG0: external VREF, internal clock, standby (one-shot mode)
 * CONFIG1: OSR = 4096 (maximum oversampling for calibration accuracy)
 * CONFIG2: BOOST=1×, GAIN=1, AZ_MUX enabled (auto-zero each conversion)
 * CONFIG3: one-shot→standby, 24-bit output, GAINCAL enabled
 */
static MCP3465R_CalStatus_t apply_config(MCP3465R_Handle_t *h)
{
    uint8_t cfg0 = MCP3465R_CONFIG0_VREF_EXT |
                   MCP3465R_CONFIG0_CS_0UA   |
                   MCP3465R_CONFIG0_CLK_INT  |
                   MCP3465R_CONFIG0_ADC_STBY;

    uint8_t cfg1 = (uint8_t)(MCP3465R_CONFIG1_PRE_1 |
                              MCP3465R_CONFIG1_OSR_4096);

    uint8_t cfg2 = (uint8_t)(MCP3465R_CONFIG2_BOOST_1  |
                              MCP3465R_CONFIG2_GAIN_1   |
                              MCP3465R_CONFIG2_AZ_MUX_EN);

    uint8_t cfg3 = (uint8_t)(MCP3465R_CONFIG3_CONV_1SHOT |
                              MCP3465R_CONFIG3_FMT_24     |
                              MCP3465R_CONFIG3_EN_GAINCAL);

    MCP3465R_CalStatus_t st;
    if ((st = MCP3465R_WriteReg(h, MCP3465R_REG_CONFIG0, cfg0, 1)) != MCP3465R_CAL_OK) return st;
    if ((st = MCP3465R_WriteReg(h, MCP3465R_REG_CONFIG1, cfg1, 1)) != MCP3465R_CAL_OK) return st;
    if ((st = MCP3465R_WriteReg(h, MCP3465R_REG_CONFIG2, cfg2, 1)) != MCP3465R_CAL_OK) return st;
    if ((st = MCP3465R_WriteReg(h, MCP3465R_REG_CONFIG3, cfg3, 1)) != MCP3465R_CAL_OK) return st;
    return MCP3465R_CAL_OK;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

MCP3465R_CalStatus_t MCP3465R_Init(MCP3465R_Handle_t *h)
{
    MCP3465R_CalStatus_t st;

    /* Full device reset via fast command */
    if (!bus_acquire(h))
        return MCP3465R_CAL_TIMEOUT;

    uint8_t cmd = MCP3465R_FAST_RESET;
    HAL_SPI_Transmit(h->hspi, &cmd, 1u, MCP3465R_SPI_TIMEOUT_MS);
    bus_release(h);

    vTaskDelay(pdMS_TO_TICKS(5));   /* allow reset to complete */

    /* Reset calibration registers to identity values */
    if ((st = MCP3465R_WriteReg(h, MCP3465R_REG_OFFSETCAL, 0x000000u, 3)) != MCP3465R_CAL_OK) return st;
    if ((st = MCP3465R_WriteReg(h, MCP3465R_REG_GAINCAL,   (uint32_t)MCP3465R_GAINCAL_UNITY, 3)) != MCP3465R_CAL_OK) return st;

    /* Apply default configuration */
    if ((st = apply_config(h)) != MCP3465R_CAL_OK) return st;

    /* Normal measurement MUX: CH0+ / AGND− */
    return MCP3465R_WriteReg(h, MCP3465R_REG_MUX, MCP3465R_MUX_NORMAL, 1);
}

/* ------------------------------------------------------------------
 * Offset Calibration
 * ------------------------------------------------------------------ */
MCP3465R_CalStatus_t MCP3465R_CalibrateOffset(MCP3465R_Handle_t   *h,
                                               MCP3465R_CalResult_t *result)
{
    MCP3465R_CalStatus_t st;

    /* Reset OFFSETCAL so we measure raw offset without previous correction */
    if ((st = MCP3465R_WriteReg(h, MCP3465R_REG_OFFSETCAL, 0u, 3)) != MCP3465R_CAL_OK)
        return st;

    /* Route ADC MUX: VIN+ = AGND, VIN− = AGND (differential = 0 V) */
    if ((st = MCP3465R_WriteReg(h, MCP3465R_REG_MUX, MCP3465R_MUX_OFFSET_CAL, 1)) != MCP3465R_CAL_OK)
        return st;

    /* Use highest OSR for calibration (already set in apply_config) */
    vTaskDelay(pdMS_TO_TICKS(2));

    int32_t mean = 0;
    if ((st = average_samples(h, &mean)) != MCP3465R_CAL_OK)
        return st;

    /* OFFSETCAL = −mean  (hardware adds OFFSETCAL to every raw result) */
    int32_t offset_reg = -mean;

    /* Clamp to 24-bit signed range */
    if (offset_reg >  0x7FFFFF) offset_reg =  0x7FFFFF;
    if (offset_reg < -0x800000) offset_reg = -0x800000;

    /* Write as unsigned 24-bit (two's complement) */
    uint32_t reg_val = (uint32_t)(offset_reg & 0xFFFFFF);
    if ((st = MCP3465R_WriteReg(h, MCP3465R_REG_OFFSETCAL, reg_val, 3)) != MCP3465R_CAL_OK)
        return st;

    result->offset_raw  = mean;
    result->offset_reg  = offset_reg;
    result->offset_done = true;

    return MCP3465R_CAL_OK;
}

/* ------------------------------------------------------------------
 * Gain Calibration
 * Must be called AFTER offset calibration.
 * ------------------------------------------------------------------ */
MCP3465R_CalStatus_t MCP3465R_CalibrateGain(MCP3465R_Handle_t   *h,
                                             MCP3465R_CalResult_t *result)
{
    MCP3465R_CalStatus_t st;

    /* Reset GAINCAL to unity before measuring, to avoid compounding errors */
    if ((st = MCP3465R_WriteReg(h, MCP3465R_REG_GAINCAL,
                                (uint32_t)MCP3465R_GAINCAL_UNITY, 3)) != MCP3465R_CAL_OK)
        return st;

    /* Route MUX: VIN+ = REFIN+ (= VREF = 3.000 V), VIN− = AGND
     * Expected ideal code = +full-scale = 0x7FFFFF                       */
    if ((st = MCP3465R_WriteReg(h, MCP3465R_REG_MUX, MCP3465R_MUX_GAIN_CAL, 1)) != MCP3465R_CAL_OK)
        return st;

    vTaskDelay(pdMS_TO_TICKS(2));

    int32_t mean = 0;
    if ((st = average_samples(h, &mean)) != MCP3465R_CAL_OK)
        return st;

    if (mean == 0)
        return MCP3465R_CAL_DIVZERO;

    /*
     * GAINCAL (Q1.23):
     *   GAINCAL = (expected / measured) × 0x800000
     *
     * Use 64-bit arithmetic to avoid overflow:
     *   numerator = 0x7FFFFF × 0x800000
     */
    int64_t gain_reg64 =
        ((int64_t)MCP3465R_GAINCAL_EXPECTED * (int64_t)MCP3465R_GAINCAL_UNITY)
        / (int64_t)mean;

    /* Clamp to 24-bit unsigned range (0 .. 0xFFFFFF) */
    if (gain_reg64 < 0)         gain_reg64 = 0;
    if (gain_reg64 > 0xFFFFFF)  gain_reg64 = 0xFFFFFF;

    int32_t gain_reg = (int32_t)gain_reg64;

    if ((st = MCP3465R_WriteReg(h, MCP3465R_REG_GAINCAL,
                                (uint32_t)gain_reg, 3)) != MCP3465R_CAL_OK)
        return st;

    result->gain_raw  = mean;
    result->gain_reg  = gain_reg;
    result->gain_done = true;

    return MCP3465R_CAL_OK;
}

/* ------------------------------------------------------------------
 * Full Auto Calibration
 * ------------------------------------------------------------------ */
MCP3465R_CalStatus_t MCP3465R_AutoCalibrate(MCP3465R_Handle_t   *h,
                                             MCP3465R_CalResult_t *result)
{
    memset(result, 0, sizeof(*result));
    MCP3465R_CalStatus_t st;

    /* Ensure known config before calibration */
    if ((st = apply_config(h)) != MCP3465R_CAL_OK)
        return st;

    if ((st = MCP3465R_CalibrateOffset(h, result)) != MCP3465R_CAL_OK)
        return st;

    if ((st = MCP3465R_CalibrateGain(h, result)) != MCP3465R_CAL_OK)
        return st;

    /* Restore normal measurement channel */
    st = MCP3465R_WriteReg(h, MCP3465R_REG_MUX, MCP3465R_MUX_NORMAL, 1);

    return st;
}

/* ------------------------------------------------------------------
 * Read one calibrated sample on the normal channel (CH0/AGND)
 * ------------------------------------------------------------------ */
MCP3465R_CalStatus_t MCP3465R_ReadCalibratedSample(MCP3465R_Handle_t *h,
                                                    int32_t           *out_code)
{
    /* Ensure MUX is on normal channel */
    MCP3465R_CalStatus_t st =
        MCP3465R_WriteReg(h, MCP3465R_REG_MUX, MCP3465R_MUX_NORMAL, 1);
    if (st != MCP3465R_CAL_OK)
        return st;

    return read_single_conversion(h, out_code);
}
