#include "calibration.h"
#include <string.h>

/* -----------------------------------------------------------------------
 * Internal helper: average N ADC readings
 * ----------------------------------------------------------------------- */

bool Calibration_AverageADC(MCP3465R_Handle *adc, uint32_t samples,
                             float *avg_mv)
{
    double sum = 0.0;
    for (uint32_t i = 0; i < samples; i++) {
        int32_t raw;
        if (!MCP3465R_ReadRaw(adc, &raw)) return false;

        /* Sanity: reject ±FS clipping */
        if (raw >= 0x7FFFFEU || raw <= -0x7FFFFEU) return false;

        sum += MCP3465R_RawToMillivolts(adc, raw);
    }
    *avg_mv = (float)(sum / (double)samples);
    return true;
}

/* -----------------------------------------------------------------------
 * Main calibration routine
 * ----------------------------------------------------------------------- */

CalResult Calibration_Run(MCP3465R_Handle *adc, DAC80501_Handle *dac,
                           CalibrationReport *report)
{
    memset(report, 0, sizeof(*report));

    /* ================================================================
     * STEP 1 – Offset calibration  (DAC → 0.5 V)
     * ================================================================ */
    report->dac_offset_set_mv = CAL_OFFSET_TARGET_MV;

    /* Clear hardware calibration so we measure the raw error */
    if (!MCP3465R_WriteOffsetCal(adc, 0))        return CAL_ERR_ADC_READ;
    if (!MCP3465R_WriteGainCal(adc, 0x800000U))  return CAL_ERR_ADC_READ;

    /* Output 0.5 V on DAC */
    if (!DAC80501_SetVoltage(dac, CAL_OFFSET_TARGET_MV)) return CAL_ERR_DAC_SET;
    delay_us(CAL_SETTLE_US);

    /* Measure ADC at 0.5 V point */
    if (!Calibration_AverageADC(adc, CAL_ADC_AVERAGES, &report->adc_offset_raw_mv))
        return CAL_ERR_ADC_READ;

    /* Offset error = what we measured − what we expected */
    report->offset_error_mv = report->adc_offset_raw_mv - CAL_OFFSET_TARGET_MV;

    /* Calculate and write offset calibration register.
     * The hardware subtracts OFFSETCAL × LSB from the result, so we write
     * the negative of the error.                                          */
    report->offset_reg = MCP3465R_CalcOffsetRegVal(-report->offset_error_mv);
    if (!MCP3465R_WriteOffsetCal(adc, report->offset_reg)) return CAL_ERR_ADC_READ;

    /* ================================================================
     * STEP 2 – Gain calibration  (DAC → 4.5 V)
     * ================================================================ */
    report->dac_gain_set_mv = CAL_GAIN_TARGET_MV;

    /* Output 4.5 V on DAC */
    if (!DAC80501_SetVoltage(dac, CAL_GAIN_TARGET_MV)) return CAL_ERR_DAC_SET;
    delay_us(CAL_SETTLE_US);

    /* Measure ADC at 4.5 V point (offset correction is now active) */
    if (!Calibration_AverageADC(adc, CAL_ADC_AVERAGES, &report->adc_gain_raw_mv))
        return CAL_ERR_ADC_READ;

    /* Span comparison:
     *   measured span = gain_reading - offset_target  (offset already corrected)
     *   expected span = gain_target  - offset_target
     *
     * Because the offset cal is already applied by HW, adc_gain_raw_mv already
     * has the offset removed.  We compare the span from the corrected offset pt. */
    float span_measured = report->adc_gain_raw_mv - CAL_OFFSET_TARGET_MV;
    float span_expected = CAL_GAIN_TARGET_MV      - CAL_OFFSET_TARGET_MV;  /* 4.0 V */

    if (span_measured < 100.0f) return CAL_ERR_INVALID_READING;  /* unreasonable */

    report->gain_factor = span_expected / span_measured;
    report->gain_reg    = MCP3465R_CalcGainRegVal(span_measured, span_expected);
    if (!MCP3465R_WriteGainCal(adc, report->gain_reg)) return CAL_ERR_ADC_READ;

    /* ================================================================
     * STEP 3 – Verification pass
     * ================================================================ */

    /* Verify offset point */
    if (!DAC80501_SetVoltage(dac, CAL_OFFSET_TARGET_MV)) return CAL_ERR_DAC_SET;
    delay_us(CAL_SETTLE_US);
    if (!Calibration_AverageADC(adc, CAL_ADC_AVERAGES, &report->verify_offset_mv))
        return CAL_ERR_ADC_READ;

    float err_offset = report->verify_offset_mv - CAL_OFFSET_TARGET_MV;
    if (err_offset < -CAL_OFFSET_TOLERANCE_MV || err_offset > CAL_OFFSET_TOLERANCE_MV)
        return CAL_ERR_VERIFY_OFFSET;

    /* Verify gain point */
    if (!DAC80501_SetVoltage(dac, CAL_GAIN_TARGET_MV)) return CAL_ERR_DAC_SET;
    delay_us(CAL_SETTLE_US);
    if (!Calibration_AverageADC(adc, CAL_ADC_AVERAGES, &report->verify_gain_mv))
        return CAL_ERR_ADC_READ;

    float err_gain = report->verify_gain_mv - CAL_GAIN_TARGET_MV;
    if (err_gain < -CAL_GAIN_TOLERANCE_MV || err_gain > CAL_GAIN_TOLERANCE_MV)
        return CAL_ERR_VERIFY_GAIN;

    /* Set DAC back to 0 V after calibration */
    DAC80501_SetVoltage(dac, 0.0f);

    report->success = true;
    return CAL_OK;
}
