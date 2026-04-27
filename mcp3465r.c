/**
 * MCP3465R Auto-calibration Driver
 * STM32L552 + FreeRTOS
 *
 * Offset calibration  : shorts both inputs to AGND via internal MUX
 * Gain calibration    : routes REFIN+/REFIN− to MUX → reads VREF vs VREF
 *                       (ideal code = 0x7FFFFF, deviation → GAINCAL)
 *
 * Hardware calibration correction is applied automatically by the ADC
 * (CONFIG3 EN_OFFCAL=1, EN_GAINCAL=1).
 *
 * Voltage divider on VIN (mandatory for 0–5 V input with 3.0 V VREF):
 *   R1 = 2 kΩ, R2 = 3 kΩ  → ratio = 0.6  → 5 V maps to 3 V at ADC input
 */

#include "mcp3465r.h"
#include <string.h>

/* =========================================================================
 * Shared globals (used by measurement task)
 * ========================================================================= */
volatile float    g_voltage_mv = 0.0f;
volatile int32_t  g_adc_raw    = 0;
volatile bool     g_cal_valid  = false;
osMutexId_t       g_mcp_mutex  = NULL;

static MCP3465R_CalResult_t s_cal;
static volatile bool      s_init_complete = false;

/* =========================================================================
 * Internal helpers
 * ========================================================================= */

static inline void cs_assert(void)
{
    HAL_GPIO_WritePin(MCP3465R_CS_GPIO, MCP3465R_CS_PIN, GPIO_PIN_RESET);
}

static inline void cs_deassert(void)
{
    HAL_GPIO_WritePin(MCP3465R_CS_GPIO, MCP3465R_CS_PIN, GPIO_PIN_SET);
}

/* Build the SPI command byte:  [DEV_ADDR 7:6][REG 5:2][CMD_TYPE 1:0] */
static inline uint8_t build_cmd(uint8_t reg, uint8_t cmd_type)
{
    return (uint8_t)(((MCP3465R_DEV_ADDR & 0x03u) << 6)
                   | ((reg             & 0x0Fu) << 2)
                   | (cmd_type         & 0x03u));
}

/* Send 1-byte fast command */
static MCP3465R_Status_t fast_cmd(uint8_t cmd)
{
    uint8_t tx = (uint8_t)(((MCP3465R_DEV_ADDR & 0x03u) << 6) | (cmd & 0x3Fu));
    cs_assert();
    HAL_StatusTypeDef r = HAL_SPI_Transmit(&MCP3465R_SPI, &tx, 1,
                                           MCP3465R_SPI_TIMEOUT_MS);
    cs_deassert();
    return (r == HAL_OK) ? MCP_OK : MCP_ERR_SPI;
}

/**
 * Write a register: 1, 2, or 3 data bytes (MSB first).
 * SPI frame = [CMD][D2][D1][D0]
 */
static MCP3465R_Status_t reg_write(uint8_t reg, uint32_t value, uint8_t nbytes)
{
    uint8_t tx[4];
    tx[0] = build_cmd(reg, CMD_INC_WRITE);

    if (nbytes == 3) {
        tx[1] = (uint8_t)(value >> 16);
        tx[2] = (uint8_t)(value >>  8);
        tx[3] = (uint8_t)(value      );
    } else if (nbytes == 2) {
        tx[1] = (uint8_t)(value >> 8);
        tx[2] = (uint8_t)(value     );
        nbytes = 2;
    } else {
        tx[1] = (uint8_t)(value);
        nbytes = 1;
    }

    cs_assert();
    HAL_StatusTypeDef r = HAL_SPI_Transmit(&MCP3465R_SPI, tx, (uint16_t)(nbytes + 1u),
                                           MCP3465R_SPI_TIMEOUT_MS);
    cs_deassert();
    return (r == HAL_OK) ? MCP_OK : MCP_ERR_SPI;
}

/**
 * Read a register: nbytes data bytes returned in *value (MSB first).
 * SPI frame = [CMD][dummy × nbytes] → rx[0] = status, rx[1..n] = data
 */
static MCP3465R_Status_t reg_read(uint8_t reg, uint32_t *value, uint8_t nbytes)
{
    uint8_t tx[4] = {0};
    uint8_t rx[4] = {0};

    tx[0] = build_cmd(reg, CMD_STATIC_READ);

    cs_assert();
    HAL_StatusTypeDef r = HAL_SPI_TransmitReceive(&MCP3465R_SPI, tx, rx,
                                                  (uint16_t)(nbytes + 1u),
                                                  MCP3465R_SPI_TIMEOUT_MS);
    cs_deassert();
    if (r != HAL_OK) return MCP_ERR_SPI;

    *value = 0;
    for (uint8_t i = 1; i <= nbytes; i++) {
        *value = (*value << 8) | rx[i];
    }
    return MCP_OK;
}

/**
 * Read 24-bit ADC conversion result, sign-extended to int32_t.
 * STATUS byte (rx[0]) can optionally be checked for DR bit.
 */
static MCP3465R_Status_t adc_read_raw(int32_t *out)
{
    uint8_t tx[4] = {0};
    uint8_t rx[4] = {0};

    tx[0] = build_cmd(REG_ADCDATA, CMD_STATIC_READ);

    cs_assert();
    HAL_StatusTypeDef r = HAL_SPI_TransmitReceive(&MCP3465R_SPI, tx, rx, 4,
                                                  MCP3465R_SPI_TIMEOUT_MS);
    cs_deassert();
    if (r != HAL_OK) return MCP_ERR_SPI;

    /* rx[1]=MSB, rx[2], rx[3]=LSB */
    uint32_t raw = ((uint32_t)rx[1] << 16)
                 | ((uint32_t)rx[2] <<  8)
                 |  (uint32_t)rx[3];

    /* Sign-extend bit 23 to bits 31:24 */
    if (raw & 0x800000u) {
        *out = (int32_t)(raw | 0xFF000000u);
    } else {
        *out = (int32_t)raw;
    }
    return MCP_OK;
}

/**
 * Switch MUX, wait for settling and discarded samples, then average N samples.
 * Caller is responsible for setting CONFIG0 to conversion mode before calling.
 */
static MCP3465R_Status_t read_averaged(uint8_t mux_pos, uint8_t mux_neg,
                                       uint32_t n_samples, int32_t *avg_out)
{
    MCP3465R_Status_t st;

    /* Switch MUX */
    st = reg_write(REG_MUX, MUX_REG(mux_pos, mux_neg), 1);
    if (st != MCP_OK) return st;

    /* Trigger conversion */
    st = reg_write(REG_CONFIG0, CONFIG0_EXT_VREF_CONV, 1);
    if (st != MCP_OK) return st;

    /* Wait for input to settle through OSR=4096 filter */
    osDelay(CAL_SETTLE_DELAY_MS);

    /* Discard initial samples (filter fill-up) */
    int32_t dummy;
    for (uint32_t i = 0; i < CAL_DISCARD_SAMPLES; i++) {
        adc_read_raw(&dummy);
        osDelay(CAL_SAMPLE_DELAY_MS);
    }

    /* Accumulate */
    int64_t sum = 0;
    for (uint32_t i = 0; i < n_samples; i++) {
        int32_t sample = 0;
        st = adc_read_raw(&sample);
        if (st != MCP_OK) return st;
        sum += sample;
        osDelay(CAL_SAMPLE_DELAY_MS);
    }

    *avg_out = (int32_t)(sum / (int64_t)n_samples);

    /* Return to standby between calibration steps */
    reg_write(REG_CONFIG0, CONFIG0_EXT_VREF_STBY, 1);
    return MCP_OK;
}

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * MCP3465R_Init
 * Reset and configure the device for 24-bit continuous conversion.
 * Calibration registers are cleared; calibration correction is OFF until
 * MCP3465R_Calibrate() succeeds.
 */
MCP3465R_Status_t MCP3465R_Init(void)
{
    MCP3465R_Status_t st;

    cs_deassert();
    osDelay(10);

    /* Full device reset (restores all registers to default) */
    st = fast_cmd(FAST_FULL_RESET & 0x3Fu);
    if (st != MCP_OK) return st;
    osDelay(10);

    /*
     * CONFIG0 = 0x22
     *   [7]=0  : external VREF
     *   [5:4]=10: internal oscillator (~4.9 MHz)
     *   [3:2]=00: no bias current source
     *   [1:0]=10: standby mode
     */
    st = reg_write(REG_CONFIG0, 0x22u, 1);
    if (st != MCP_OK) return st;

    /*
     * CONFIG1 = CONFIG1_OSR_4096 (0x1C)
     * Use high OSR during calibration; switch to CONFIG1_OSR_256 (0x0C)
     * for normal measurement via reg_write(REG_CONFIG1, CONFIG1_OSR_256, 1)
     */
    st = reg_write(REG_CONFIG1, CONFIG1_OSR_4096, 1);
    if (st != MCP_OK) return st;

    /*
     * CONFIG2 = 0xD4
     *   [7:6]=11: BOOST 2× (max bias, lowest noise)
     *   [5:3]=010: PGA gain 1× (voltage divider already scales 5V→3V)
     *   [2]=1  : AZ_MUX enabled (auto-zero the analog MUX)
     *   [1:0]=00
     */
    st = reg_write(REG_CONFIG2, CONFIG2_BOOST2_GAIN1_AZ, 1);
    if (st != MCP_OK) return st;

    /*
     * CONFIG3 = 0xC0
     *   [7:6]=11: continuous conversion mode
     *   [5:4]=00: 24-bit data format
     *   [1]=0, [0]=0: correction OFF until calibration completes
     */
    st = reg_write(REG_CONFIG3, CONFIG3_CAL_NONE, 1);
    if (st != MCP_OK) return st;

    /* Clear calibration registers */
    st = reg_write(REG_OFFSETCAL, 0x000000u, 3);
    if (st != MCP_OK) return st;

    st = reg_write(REG_GAINCAL, GAINCAL_UNITY, 3);
    if (st != MCP_OK) return st;

    /* IRQ: disable all interrupts, DR pin active-low */
    st = reg_write(REG_IRQ, 0x03u, 1);
    if (st != MCP_OK) return st;

    memset(&s_cal, 0, sizeof(s_cal));
    return MCP_OK;
}

/* -------------------------------------------------------------------------
 * Offset calibration
 *   MUX: AGND(+) / AGND(−) → ideal output = 0
 *   OFFSETCAL = −avg  (two's complement, 24-bit)
 * --------------------------------------------------------------------- */
static MCP3465R_Status_t calibrate_offset(void)
{
    MCP3465R_Status_t st;
    int32_t avg = 0;

    /* Disable all hardware correction for a clean raw measurement */
    st = reg_write(REG_CONFIG3, CONFIG3_CAL_NONE, 1);
    if (st != MCP_OK) return st;

    st = reg_write(REG_OFFSETCAL, 0x000000u, 3);
    if (st != MCP_OK) return st;

    st = read_averaged(MUX_AGND, MUX_AGND, CAL_SAMPLES, &avg);
    if (st != MCP_OK) return st;

    s_cal.offset_raw     = avg;
    s_cal.offset_cal_reg = -avg;    /* correction = negative of measured offset */

    /* Mask to 24-bit two's complement */
    uint32_t reg_val = (uint32_t)(s_cal.offset_cal_reg) & 0xFFFFFFu;

    st = reg_write(REG_OFFSETCAL, reg_val, 3);
    if (st != MCP_OK) return st;

    /* Enable offset correction so gain calibration sees corrected data */
    st = reg_write(REG_CONFIG3, CONFIG3_CAL_OFFSET, 1);
    return st;
}

/* -------------------------------------------------------------------------
 * Gain calibration
 *   MUX: REFIN+(+) / REFIN−(−) → reads VREF/VREF differential
 *   Ideal output at full PGA range = +0x7FFFFF
 *   GAINCAL = (0x7FFFFF × 0x800000) / avg_code
 *
 *   Result clamped to ±12.5% of unity (0x700000 … 0x900000).
 * --------------------------------------------------------------------- */
static MCP3465R_Status_t calibrate_gain(void)
{
    MCP3465R_Status_t st;
    int32_t avg = 0;

    /* Reset GAINCAL to unity; offset correction is already active */
    st = reg_write(REG_GAINCAL, GAINCAL_UNITY, 3);
    if (st != MCP_OK) return st;

    /*
     * Route REFIN+/REFIN- to the differential MUX.
     * The ADC measures (VREF − 0) / VREF = 1.0 → expected code = +0x7FFFFF.
     * Any deviation is quantified and corrected by GAINCAL.
     */
    st = read_averaged(MUX_REFIN_POS, MUX_REFIN_NEG, CAL_SAMPLES, &avg);
    if (st != MCP_OK) return st;

    if (avg <= 0) {
        /* Negative or zero reading from VREF means hardware fault */
        return MCP_ERR_CAL_FAILED;
    }

    /*
     * GAINCAL_new = IDEAL × UNITY / measured
     *             = ADC_FULL_SCALE × GAINCAL_UNITY / avg
     *
     * Using int64 arithmetic to avoid 32-bit overflow
     * (0x7FFFFF × 0x800000 = ~70e12, fits in int64_t).
     */
    int64_t gaincal = ((int64_t)ADC_FULL_SCALE * (int64_t)GAINCAL_UNITY) / (int64_t)avg;

    /* Clamp: allow ±12.5% correction range */
    if (gaincal < 0x700000LL) gaincal = 0x700000LL;
    if (gaincal > 0x900000LL) gaincal = 0x900000LL;

    s_cal.gain_cal_reg = (uint32_t)gaincal;
    s_cal.gain_factor  = (float)gaincal / (float)GAINCAL_UNITY;

    st = reg_write(REG_GAINCAL, s_cal.gain_cal_reg, 3);
    if (st != MCP_OK) return st;

    /* Enable both offset + gain correction */
    st = reg_write(REG_CONFIG3, CONFIG3_CAL_BOTH, 1);
    return st;
}

/* -------------------------------------------------------------------------
 * Verification pass
 *   After writing both calibration registers and enabling them,
 *   re-measure AGND→AGND (must be near 0) and REFIN (must be near full scale).
 *   Returns MCP_ERR_CAL_FAILED if residuals are too large.
 * --------------------------------------------------------------------- */
static MCP3465R_Status_t verify_calibration(void)
{
    int32_t code_zero = 0, code_fs = 0;

    /* Offset residual check: AGND/AGND must give |code| < 100 LSB */
    MCP3465R_Status_t st = read_averaged(MUX_AGND, MUX_AGND, 16u, &code_zero);
    if (st != MCP_OK) return st;
    if (code_zero < -100 || code_zero > 100) return MCP_ERR_CAL_FAILED;

    /* Gain residual check: REFIN must give code in [0x7F8000 … 0x7FFFFF] */
    st = read_averaged(MUX_REFIN_POS, MUX_REFIN_NEG, 16u, &code_fs);
    if (st != MCP_OK) return st;
    if (code_fs < 0x7F8000L || code_fs > ADC_FULL_SCALE) return MCP_ERR_CAL_FAILED;

    return MCP_OK;
}

/**
 * MCP3465R_Calibrate
 * Runs the full automatic calibration sequence:
 *   1. Offset (AGND/AGND)
 *   2. Gain   (REFIN+/REFIN−)
 *   3. Verification
 * On success, result->valid = true and calibration is active in hardware.
 */
MCP3465R_Status_t MCP3465R_Calibrate(MCP3465R_CalResult_t *result)
{
    MCP3465R_Status_t st;

    s_cal.valid = false;

    /* Use high OSR during calibration for best SNR */
    st = reg_write(REG_CONFIG1, CONFIG1_OSR_4096, 1);
    if (st != MCP_OK) return st;

    st = calibrate_offset();
    if (st != MCP_OK) return st;

    st = calibrate_gain();
    if (st != MCP_OK) return st;

    st = verify_calibration();
    if (st != MCP_OK) return st;

    /*
     * Switch back to OSR=256 for normal measurement throughput (~1 kSPS).
     * The hardware calibration (EN_OFFCAL + EN_GAINCAL) remains active.
     */
    st = reg_write(REG_CONFIG1, CONFIG1_OSR_256, 1);
    if (st != MCP_OK) return st;

    s_cal.valid = true;

    if (result != NULL) {
        *result = s_cal;
    }
    return MCP_OK;
}

/**
 * MCP3465R_ReadRaw
 * Read one calibration-corrected 24-bit code in continuous mode.
 * Caller must ensure CONFIG0 is in conversion mode and MUX is set.
 */
MCP3465R_Status_t MCP3465R_ReadRaw(int32_t *raw_code)
{
    return adc_read_raw(raw_code);
}

/**
 * MCP3465R_ReadVoltage
 * Read one sample and convert to millivolts at the system input (0–5000 mV).
 *
 * With hardware calibration active:
 *   V_adc_mv = (raw / 0x7FFFFF) × VREF_MV
 *   V_in_mv  = V_adc_mv / VIN_DIVIDER_RATIO
 */
MCP3465R_Status_t MCP3465R_ReadVoltage(float *voltage_mv)
{
    int32_t raw = 0;
    MCP3465R_Status_t st = adc_read_raw(&raw);
    if (st != MCP_OK) return st;

    float v_adc = ((float)raw / (float)ADC_FULL_SCALE) * VREF_MV;
    float v_in  = v_adc / VIN_DIVIDER_RATIO;

    /* Clamp to physical range */
    if (v_in < 0.0f)        v_in = 0.0f;
    if (v_in > VIN_MAX_MV)  v_in = VIN_MAX_MV;

    *voltage_mv = v_in;
    return MCP_OK;
}

/* =========================================================================
 * FreeRTOS Tasks
 * ========================================================================= */

/**
 * Task_MCP3465R_Init
 * One-shot task: initialise, calibrate, then terminate.
 * Sets g_cal_valid so the measurement task can start.
 *
 * Suggested stack: 512 words  Priority: osPriorityHigh
 * Create with: osThreadNew(Task_MCP3465R_Init, NULL, &attr);
 */
void Task_MCP3465R_Init(void *arg)
{
    (void)arg;

    /* Create mutex that protects SPI bus access */
    g_mcp_mutex = osMutexNew(NULL);

    osDelay(500);   /* let power supplies and VREF stabilise */

    MCP3465R_Status_t st = MCP3465R_Init();
    if (st != MCP_OK) goto fail;

    osDelay(50);

    MCP3465R_CalResult_t result;
    st = MCP3465R_Calibrate(&result);
    if (st != MCP_OK) goto fail;

    /* Switch to normal channel: CH0+ vs AGND, continuous conversion */
    reg_write(REG_MUX, MUX_REG(MUX_CH0, MUX_AGND), 1);
    reg_write(REG_CONFIG0, CONFIG0_EXT_VREF_CONV, 1);

    g_cal_valid = true;
    s_init_complete = true;
    osThreadTerminate(osThreadGetId());
    return;

fail:
    /*
     * Calibration failed.  Fall back: run with defaults so the system
     * is not completely blind, but flag it as uncalibrated.
     */
    reg_write(REG_CONFIG3, CONFIG3_CAL_NONE, 1);
    reg_write(REG_MUX,    MUX_REG(MUX_CH0, MUX_AGND), 1);
    reg_write(REG_CONFIG0, CONFIG0_EXT_VREF_CONV, 1);
    g_cal_valid = false;
    s_init_complete = true;
    osThreadTerminate(osThreadGetId());
}

/**
 * Task_MCP3465R_Measure
 * Periodic measurement task.  Waits for calibration to be valid, then
 * samples at ~100 ms intervals and updates g_voltage_mv / g_adc_raw.
 *
 * Also performs a lightweight auto-recalibration every 10 minutes to
 * compensate for thermal drift.
 *
 * Suggested stack: 256 words  Priority: osPriorityNormal
 */
void Task_MCP3465R_Measure(void *arg)
{
    (void)arg;

    /* Wait for init task to complete (calibration may still fail). */
    while (!s_init_complete) {
        osDelay(100);
    }

    const uint32_t RECAL_INTERVAL_MS = 10u * 60u * 1000u;  /* 10 minutes */
    uint32_t       last_recal_tick   = osKernelGetTickCount();

    for (;;) {
        uint32_t now = osKernelGetTickCount();

        /* Periodic recalibration */
        if ((now - last_recal_tick) >= RECAL_INTERVAL_MS) {
            if (osMutexAcquire(g_mcp_mutex, 2000) == osOK) {
                MCP3465R_CalResult_t result;
                if (MCP3465R_Calibrate(&result) == MCP_OK) {
                    g_cal_valid = true;
                }
                /* Restore normal measurement MUX + conversion mode */
                reg_write(REG_MUX,    MUX_REG(MUX_CH0, MUX_AGND), 1);
                reg_write(REG_CONFIG0, CONFIG0_EXT_VREF_CONV, 1);
                osMutexRelease(g_mcp_mutex);
            }
            last_recal_tick = now;
        }

        /* Normal measurement */
        if (osMutexAcquire(g_mcp_mutex, 50) == osOK) {
            int32_t raw   = 0;
            float   v_mv  = 0.0f;
            if (MCP3465R_ReadRaw(&raw) == MCP_OK) {
                g_adc_raw = raw;
                float v_adc = ((float)raw / (float)ADC_FULL_SCALE) * VREF_MV;
                v_mv = v_adc / VIN_DIVIDER_RATIO;
                if (v_mv < 0.0f)       v_mv = 0.0f;
                if (v_mv > VIN_MAX_MV) v_mv = VIN_MAX_MV;
                g_voltage_mv = v_mv;
            }
            osMutexRelease(g_mcp_mutex);
        }

        osDelay(100);   /* 10 Hz measurement rate */
    }
}
