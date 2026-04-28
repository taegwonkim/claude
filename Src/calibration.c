/**
 * Two-point ADC calibration implementation.
 * See calibration.h for algorithm description.
 */
#include "calibration.h"
#include "mcp3465r.h"
#include "dac80501.h"

/* Settling time after DAC voltage change before reading ADC */
#define CAL_SETTLE_MS   200U

/* ── Internal helpers ───────────────────────────────────────────────────── */

/**
 * Apply a DAC voltage, wait for settling, and average ADC readings.
 *
 * @param voltage_v  DAC target voltage
 * @param avg_code   output: averaged 24-bit ADC code
 */
static HAL_StatusTypeDef measure_at(float voltage_v, int32_t *avg_code)
{
    HAL_StatusTypeDef st;

    st = DAC80501_SetVoltage(voltage_v);
    if (st != HAL_OK) return st;

    HAL_Delay(CAL_SETTLE_MS);

    st = MCP3465R_ReadAverage(CAL_AVG_SAMPLES, avg_code);
    return st;
}

/**
 * Calculate ideal 24-bit ADC code for a given voltage.
 *   ideal_code = round(V / VREF × (2^23 − 1))
 */
static int32_t ideal_code(float voltage_v)
{
    float norm = voltage_v / CAL_VREF_ADC;
    return (int32_t)(norm * (float)MCP3465R_FULL_SCALE_24 + 0.5f);
}

/* ── Public API ─────────────────────────────────────────────────────────── */

/**
 * RunCalibration — full two-point offset + gain calibration.
 *
 * Preconditions:
 *   - MCP3465R_Init() and DAC80501_Init() must have been called successfully.
 *   - The DAC output pin must be wired to the MCP3465R CH0 input.
 *   - The MCP3465R VREF pin must be connected to a stable 5.0V reference.
 */
HAL_StatusTypeDef RunCalibration(CalibrationResult_t *result)
{
    HAL_StatusTypeDef st;

    /* ── Step 1: disable on-chip calibration to read raw ADC codes ──────── */
    st = MCP3465R_SetCalibrationEnable(false, false);
    if (st != HAL_OK) return st;

    /* Reset calibration registers to neutral values */
    st = MCP3465R_WriteOffsetCal(0);
    if (st != HAL_OK) return st;
    st = MCP3465R_WriteGainCal(MCP3465R_GAINCAL_UNITY);
    if (st != HAL_OK) return st;

    /* ── Step 2: measure raw code at V_LOW = 0.5V (offset calibration pt) ─ */
    int32_t code_low;
    st = measure_at(CAL_V_LOW, &code_low);
    if (st != HAL_OK) return st;

    /* ── Step 3: measure raw code at V_HIGH = 4.5V (gain calibration pt) ── */
    int32_t code_high;
    st = measure_at(CAL_V_HIGH, &code_high);
    if (st != HAL_OK) return st;

    /* ── Step 4: compute two-point calibration constants ────────────────── */
    /*
     * Linear ADC error model:
     *   code_measured = α × code_ideal + β
     *
     * The MCP3465R applies:
     *   ADCDATA = ADCraw × (GAINCAL / 2^23) + OFFSETCAL
     *
     * We need ADCDATA = code_ideal, so:
     *   GAINCAL   = 2^23 × (ideal_high − ideal_low) / (code_high − code_low)
     *   OFFSETCAL = ideal_low − code_low × GAINCAL / 2^23
     */
    int32_t ideal_low  = ideal_code(CAL_V_LOW);   /*  838861 for VREF=5V */
    int32_t ideal_high = ideal_code(CAL_V_HIGH);  /* 7549747 for VREF=5V */

    int32_t span_ideal    = ideal_high - ideal_low;
    int32_t span_measured = code_high  - code_low;

    if (span_measured == 0) {
        /* ADC not responding: hardware fault */
        return HAL_ERROR;
    }

    /* Use double precision to avoid overflow in intermediate products */
    double gaincal_d  = (double)MCP3465R_GAINCAL_UNITY
                        * (double)span_ideal
                        / (double)span_measured;

    double offsetcal_d = (double)ideal_low
                         - (double)code_low * gaincal_d
                           / (double)MCP3465R_GAINCAL_UNITY;

    uint32_t gaincal  = (uint32_t)(gaincal_d  + 0.5);
    int32_t  offsetcal = (int32_t)(offsetcal_d + (offsetcal_d >= 0.0 ? 0.5 : -0.5));

    /* Clamp to 24-bit register range */
    if (gaincal  > 0x00FFFFFFU) gaincal  = 0x00FFFFFFU;
    if (offsetcal > 8388607)    offsetcal =  8388607;
    if (offsetcal < -8388608)   offsetcal = -8388608;

    /* ── Step 5: write calibration registers and enable ─────────────────── */
    st = MCP3465R_WriteGainCal(gaincal);
    if (st != HAL_OK) return st;

    st = MCP3465R_WriteOffsetCal(offsetcal);
    if (st != HAL_OK) return st;

    st = MCP3465R_SetCalibrationEnable(true, true);
    if (st != HAL_OK) return st;

    /* ── Step 6: verification ────────────────────────────────────────────── */
    int32_t verify_code_low, verify_code_high;

    st = measure_at(CAL_V_LOW, &verify_code_low);
    if (st != HAL_OK) return st;

    st = measure_at(CAL_V_HIGH, &verify_code_high);
    if (st != HAL_OK) return st;

    float v_low_measured  = MCP3465R_Code24ToVoltage(verify_code_low,  CAL_VREF_ADC);
    float v_high_measured = MCP3465R_Code24ToVoltage(verify_code_high, CAL_VREF_ADC);
    float err_low_mv  = (v_low_measured  - CAL_V_LOW)  * 1000.0f;
    float err_high_mv = (v_high_measured - CAL_V_HIGH) * 1000.0f;

    /* ── Return DAC to 0V ────────────────────────────────────────────────── */
    DAC80501_SetVoltage(0.0f);

    /* ── Fill result ─────────────────────────────────────────────────────── */
    if (result != NULL) {
        result->offsetcal      = offsetcal;
        result->gaincal        = gaincal;
        result->verify_low_v   = v_low_measured;
        result->verify_high_v  = v_high_measured;
        result->error_low_mv   = err_low_mv;
        result->error_high_mv  = err_high_mv;

        float abs_low  = err_low_mv  < 0.0f ? -err_low_mv  : err_low_mv;
        float abs_high = err_high_mv < 0.0f ? -err_high_mv : err_high_mv;
        result->pass = (abs_low  < CAL_PASS_THRESHOLD_MV) &&
                       (abs_high < CAL_PASS_THRESHOLD_MV);
    }

    return HAL_OK;
}
