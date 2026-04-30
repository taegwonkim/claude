#include "mcp3465r_cal.h"
#include "stm32l552_flash.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* CRC-32 (ISO 3309 / Ethernet polynomial 0xEDB88320)                  */
/* ------------------------------------------------------------------ */

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    static const uint32_t TABLE[16] = {
        0x00000000UL, 0x1DB71064UL, 0x3B6E20C8UL, 0x26D930ACUL,
        0x76DC4190UL, 0x6B6B51F4UL, 0x4DB26158UL, 0x5005713CUL,
        0xEDB88320UL, 0xF00F9344UL, 0xD6D6A3E8UL, 0xCB61B38CUL,
        0x9B64C2B0UL, 0x86D3D2D4UL, 0xA00AE278UL, 0xBDBDF21CUL,
    };

    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 4) ^ TABLE[(crc ^ data[i])        & 0x0FU];
        crc = (crc >> 4) ^ TABLE[(crc ^ (data[i] >> 4)) & 0x0FU];
    }
    return ~crc;
}

static uint32_t cal_crc(const MCP3465R_CalData_t *c)
{
    /* CRC covers all fields except the crc32 field at the end */
    size_t covered = offsetof(MCP3465R_CalData_t, crc32);
    return crc32_update(0xFFFFFFFFUL, (const uint8_t *)c, covered);
}

/* ------------------------------------------------------------------ */
/* MCP3465R low-level register helpers                                  */
/* ------------------------------------------------------------------ */

mcp3465r_status_t mcp3465r_write_reg(uint8_t reg, const uint8_t *data, size_t len)
{
    uint8_t cmd = mcp3465r_cmd(reg, MCP3465R_CMD_IWRITE);
    mcp3465r_cs_low();
    int rc = mcp3465r_spi_transfer(&cmd, NULL, 1);
    if (rc == 0)
        rc = mcp3465r_spi_transfer(data, NULL, len);
    mcp3465r_cs_high();
    return rc ? MCP3465R_ERR_SPI : MCP3465R_OK;
}

mcp3465r_status_t mcp3465r_read_reg(uint8_t reg, uint8_t *data, size_t len)
{
    uint8_t cmd = mcp3465r_cmd(reg, MCP3465R_CMD_IREAD);
    mcp3465r_cs_low();
    int rc = mcp3465r_spi_transfer(&cmd, NULL, 1);
    if (rc == 0)
        rc = mcp3465r_spi_transfer(NULL, data, len);
    mcp3465r_cs_high();
    return rc ? MCP3465R_ERR_SPI : MCP3465R_OK;
}

mcp3465r_status_t mcp3465r_fast_cmd(uint8_t cmd)
{
    mcp3465r_cs_low();
    int rc = mcp3465r_spi_transfer(&cmd, NULL, 1);
    mcp3465r_cs_high();
    return rc ? MCP3465R_ERR_SPI : MCP3465R_OK;
}

/* Read 32-bit ADCDATA register → sign-extend to int32 (24-bit result). */
mcp3465r_status_t mcp3465r_read_adc(int32_t *out)
{
    uint8_t buf[4] = {0};
    mcp3465r_status_t st = mcp3465r_read_reg(MCP3465R_REG_ADCDATA, buf, 4);
    if (st != MCP3465R_OK)
        return st;

    /* CONFIG3 FMT=0b10: 32-bit sign-extended, bits[31:24]=sign extension */
    int32_t raw = (int32_t)(((uint32_t)buf[0] << 24) |
                             ((uint32_t)buf[1] << 16) |
                             ((uint32_t)buf[2] <<  8) |
                              (uint32_t)buf[3]);
    /* Discard the upper 8 sign-extension bits → keep 24-bit value */
    *out = raw >> 8;
    return MCP3465R_OK;
}

/* Write a 24-bit calibration register (OFFSETCAL or GAINCAL).
 * The MCP3465R stores these as 3 bytes, MSB first. */
mcp3465r_status_t mcp3465r_write_cal_reg(uint8_t reg, int32_t value)
{
    uint8_t buf[3];
    uint32_t v = (uint32_t)(value & 0xFFFFFF);
    buf[0] = (uint8_t)(v >> 16);
    buf[1] = (uint8_t)(v >>  8);
    buf[2] = (uint8_t)(v);
    return mcp3465r_write_reg(reg, buf, 3);
}

mcp3465r_status_t mcp3465r_read_cal_reg(uint8_t reg, int32_t *value)
{
    uint8_t buf[3] = {0};
    mcp3465r_status_t st = mcp3465r_read_reg(reg, buf, 3);
    if (st != MCP3465R_OK)
        return st;

    uint32_t raw = ((uint32_t)buf[0] << 16) |
                   ((uint32_t)buf[1] <<  8) |
                    (uint32_t)buf[2];
    /* Sign-extend from 24-bit */
    if (raw & 0x800000UL)
        *value = (int32_t)(raw | 0xFF000000UL);
    else
        *value = (int32_t)raw;
    return MCP3465R_OK;
}

mcp3465r_status_t mcp3465r_apply_offset(int32_t offset)
{
    return mcp3465r_write_cal_reg(MCP3465R_REG_OFFSETCAL, offset);
}

mcp3465r_status_t mcp3465r_apply_gain(uint32_t gain)
{
    return mcp3465r_write_cal_reg(MCP3465R_REG_GAINCAL, (int32_t)gain);
}

/* ------------------------------------------------------------------ */
/* ADC averaging helper                                                 */
/* ------------------------------------------------------------------ */

/* Collect `count` conversions and return the integer mean.
 * The MCP3465R asserts /IRQ (or sets bit in IRQ reg) when data is ready.
 * Here we poll the IRQ register DR_STATUS bit instead. */
static mcp3465r_status_t adc_average(uint32_t count, int32_t *mean)
{
    int64_t sum = 0;

    for (uint32_t i = 0; i < count; i++) {
        /* Poll data-ready: IRQ register bit 2 (DR_STATUS) goes 0 when ready */
        uint32_t timeout = 100000U;
        uint8_t irq;
        do {
            if (mcp3465r_read_reg(MCP3465R_REG_IRQ, &irq, 1) != MCP3465R_OK)
                return MCP3465R_ERR_SPI;
            if (--timeout == 0)
                return MCP3465R_ERR_TIMEOUT;
        } while (irq & (1U << 2));  /* DR_STATUS=0 means new data */

        int32_t sample;
        if (mcp3465r_read_adc(&sample) != MCP3465R_OK)
            return MCP3465R_ERR_SPI;

        sum += sample;
    }

    *mean = (int32_t)(sum / (int64_t)count);
    return MCP3465R_OK;
}

/* ------------------------------------------------------------------ */
/* Flash persistence                                                    */
/* ------------------------------------------------------------------ */

static cal_status_t cal_save(const MCP3465R_CalData_t *cal)
{
    /* Write primary copy at offset 0 and redundant copy at offset PAGE_SIZE */
    uint8_t buf[MCP3465R_CAL_PADDED_SIZE];
    memset(buf, 0xFF, sizeof(buf));
    memcpy(buf, cal, sizeof(MCP3465R_CalData_t));

    flash_status_t st = flash_write_user_data(MCP3465R_CAL_FLASH_OFFSET,
                                               buf, sizeof(buf));
    if (st != FLASH_OK)
        return CAL_ERR_FLASH;

    /* Redundant copy one page later */
    st = flash_write_user_data(MCP3465R_CAL_COPY2_OFFSET, buf, sizeof(buf));
    return (st == FLASH_OK) ? CAL_OK : CAL_ERR_FLASH;
}

static cal_status_t cal_load(MCP3465R_CalData_t *out, uint32_t flash_offset)
{
    flash_status_t st = flash_read_user_data(flash_offset, out,
                                              sizeof(MCP3465R_CalData_t));
    if (st != FLASH_OK)
        return CAL_ERR_FLASH;

    if (out->magic != MCP3465R_CAL_MAGIC)
        return CAL_ERR_MAGIC;

    if (out->crc32 != cal_crc(out))
        return CAL_ERR_CRC;

    return CAL_OK;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

cal_status_t mcp3465r_cal_read(MCP3465R_CalData_t *out)
{
    if (!out)
        return CAL_ERR_PARAM;

    /* Try primary copy first, fall back to redundant */
    cal_status_t st = cal_load(out, MCP3465R_CAL_FLASH_OFFSET);
    if (st != CAL_OK)
        st = cal_load(out, MCP3465R_CAL_COPY2_OFFSET);

    return st;
}

bool mcp3465r_cal_is_valid(void)
{
    MCP3465R_CalData_t cal;
    return mcp3465r_cal_read(&cal) == CAL_OK;
}

cal_status_t mcp3465r_cal_apply(const MCP3465R_CalData_t *cal)
{
    if (!cal)
        return CAL_ERR_PARAM;

    if (mcp3465r_apply_offset(cal->offset) != MCP3465R_OK)
        return CAL_ERR_ADC;

    if (mcp3465r_apply_gain(cal->gain) != MCP3465R_OK)
        return CAL_ERR_ADC;

    return CAL_OK;
}

cal_status_t mcp3465r_cal_load_and_apply(void)
{
    MCP3465R_CalData_t cal;

    cal_status_t st = mcp3465r_cal_read(&cal);
    if (st != CAL_OK)
        return st;

    return mcp3465r_cal_apply(&cal);
}

cal_status_t mcp3465r_cal_erase(void)
{
    /* flash_write_user_data erases the pages it touches */
    uint8_t dummy[MCP3465R_CAL_PADDED_SIZE];
    memset(dummy, 0xFF, sizeof(dummy));

    flash_status_t st = flash_write_user_data(MCP3465R_CAL_FLASH_OFFSET,
                                               dummy, sizeof(dummy));
    return (st == FLASH_OK) ? CAL_OK : CAL_ERR_FLASH;
}

cal_status_t mcp3465r_calibrate(uint8_t pga_gain_cfg,
                                uint8_t osr_cfg,
                                int32_t vref_counts)
{
    mcp3465r_status_t adc_st;
    MCP3465R_CalData_t cal = {0};

    /* ---- Step 1: Configure ADC for calibration ---- */

    /* Reset to known state */
    mcp3465r_fast_cmd(MCP3465R_FASTCMD_RESET);
    mcp3465r_delay_us(200);

    uint8_t cfg0 = MCP3465R_CFG0_VREF_INT |
                   MCP3465R_CFG0_CLK_INT  |
                   MCP3465R_CFG0_CS_NONE  |
                   MCP3465R_CFG0_MODE_CONV;
    if (mcp3465r_write_reg(MCP3465R_REG_CONFIG0, &cfg0, 1) != MCP3465R_OK)
        return CAL_ERR_ADC;

    if (mcp3465r_write_reg(MCP3465R_REG_CONFIG1, &osr_cfg, 1) != MCP3465R_OK)
        return CAL_ERR_ADC;

    /* Enable auto-zero MUX alongside requested gain */
    uint8_t cfg2 = pga_gain_cfg | MCP3465R_CFG2_AZ_MUX | MCP3465R_CFG2_BOOST_X2;
    if (mcp3465r_write_reg(MCP3465R_REG_CONFIG2, &cfg2, 1) != MCP3465R_OK)
        return CAL_ERR_ADC;

    uint8_t cfg3 = MCP3465R_CFG3_FMT_32B_S | MCP3465R_CFG3_CONV_CONT;
    if (mcp3465r_write_reg(MCP3465R_REG_CONFIG3, &cfg3, 1) != MCP3465R_OK)
        return CAL_ERR_ADC;

    /* Clear any previous hardware calibration */
    mcp3465r_apply_offset(0);
    mcp3465r_apply_gain(MCP3465R_GAINCAL_UNITY);

    /* ---- Step 2: Offset calibration (AGND → AGND) ---- */

    uint8_t mux = MCP3465R_MUX_CH(MCP3465R_MUX_AGND, MCP3465R_MUX_AGND);
    if (mcp3465r_write_reg(MCP3465R_REG_MUX, &mux, 1) != MCP3465R_OK)
        return CAL_ERR_ADC;

    mcp3465r_delay_us(500); /* Let the MUX settle */

    int32_t offset_mean;
    adc_st = adc_average(MCP3465R_CAL_AVG_COUNT, &offset_mean);
    if (adc_st != MCP3465R_OK)
        return CAL_ERR_ADC;

    /* OFFSETCAL is subtracted by hardware: set to -mean so output → 0 */
    int32_t offset_cal = -offset_mean;
    if (mcp3465r_apply_offset(offset_cal) != MCP3465R_OK)
        return CAL_ERR_ADC;

    /* ---- Step 3: Gain calibration (REFIN+ → REFIN-) ---- */

    mux = MCP3465R_MUX_CH(MCP3465R_MUX_REFIN_P, MCP3465R_MUX_REFIN_N);
    if (mcp3465r_write_reg(MCP3465R_REG_MUX, &mux, 1) != MCP3465R_OK)
        return CAL_ERR_ADC;

    mcp3465r_delay_us(500);

    int32_t gain_mean;
    adc_st = adc_average(MCP3465R_CAL_AVG_COUNT, &gain_mean);
    if (adc_st != MCP3465R_OK)
        return CAL_ERR_ADC;

    /* GAINCAL = 0x800000 × expected / measured
     * Use 64-bit intermediate to avoid overflow. */
    uint32_t gain_cal;
    if (gain_mean <= 0) {
        gain_cal = MCP3465R_GAINCAL_UNITY; /* Guard: can't compute gain */
    } else {
        gain_cal = (uint32_t)(((int64_t)vref_counts *
                                (int64_t)MCP3465R_GAINCAL_UNITY) /
                               (int64_t)gain_mean);
        /* Clamp to a ±12.5% range to reject bad measurements */
        const uint32_t GAIN_MAX = (uint32_t)(MCP3465R_GAINCAL_UNITY * 9 / 8);
        const uint32_t GAIN_MIN = (uint32_t)(MCP3465R_GAINCAL_UNITY * 7 / 8);
        if (gain_cal > GAIN_MAX) gain_cal = GAIN_MAX;
        if (gain_cal < GAIN_MIN) gain_cal = GAIN_MIN;
    }

    if (mcp3465r_apply_gain(gain_cal) != MCP3465R_OK)
        return CAL_ERR_ADC;

    /* ---- Step 4: Pack and persist to flash ---- */

    cal.magic           = MCP3465R_CAL_MAGIC;
    cal.version         = MCP3465R_CAL_VERSION;
    cal.pga_gain        = pga_gain_cfg;
    cal.osr             = osr_cfg;
    cal.offset          = offset_cal;
    cal.gain            = gain_cal;
    cal.raw_offset_mean = offset_mean;
    cal.raw_gain_mean   = gain_mean;
    cal.crc32           = cal_crc(&cal);

    return cal_save(&cal);
}
