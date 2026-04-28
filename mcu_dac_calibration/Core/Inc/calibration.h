#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <stdint.h>
#include <stdbool.h>
#include "mcp3465r.h"
#include "dac80501.h"

/* -----------------------------------------------------------------------
 * Calibration algorithm
 *
 * Step 1 – Offset calibration
 *   DAC → 0.5 V  →  ADC reads V_offset_measured
 *   Expected: 0.5 V (V_offset_expected)
 *   offset_error_mv = V_offset_measured - V_offset_expected
 *   → write OFFSETCAL = -offset_error_mv (cancels the error)
 *
 * Step 2 – Gain calibration
 *   DAC → 4.5 V  →  ADC reads V_gain_measured
 *   Expected: 4.5 V (V_gain_expected)
 *   span_measured = V_gain_measured - V_offset_measured
 *   span_expected = V_gain_expected  - V_offset_expected
 *   gain_correction = span_expected / span_measured
 *   → write GAINCAL = 0x800000 × gain_correction
 * ----------------------------------------------------------------------- */

#define CAL_OFFSET_TARGET_MV    500.0f   /* 0.5 V */
#define CAL_GAIN_TARGET_MV     4500.0f   /* 4.5 V */
#define CAL_SETTLE_US          50000U    /* 50 ms DAC settling time */
#define CAL_ADC_AVERAGES        32U      /* average this many ADC readings */
#define CAL_MAX_RETRIES          3U

/* Allowable error after calibration */
#define CAL_OFFSET_TOLERANCE_MV  2.0f   /* ±2 mV */
#define CAL_GAIN_TOLERANCE_MV    5.0f   /* ±5 mV on full-span */

typedef enum {
    CAL_OK = 0,
    CAL_ERR_DAC_INIT,
    CAL_ERR_ADC_INIT,
    CAL_ERR_DAC_SET,
    CAL_ERR_ADC_READ,
    CAL_ERR_VERIFY_OFFSET,
    CAL_ERR_VERIFY_GAIN,
    CAL_ERR_INVALID_READING
} CalResult;

typedef struct {
    float    dac_offset_set_mv;     /* DAC output for offset step */
    float    adc_offset_raw_mv;     /* ADC reading (before cal) */
    float    offset_error_mv;       /* calculated offset error */
    int32_t  offset_reg;            /* value written to OFFSETCAL */

    float    dac_gain_set_mv;       /* DAC output for gain step */
    float    adc_gain_raw_mv;       /* ADC reading (before cal) */
    float    gain_factor;           /* calculated gain correction */
    uint32_t gain_reg;              /* value written to GAINCAL */

    float    verify_offset_mv;      /* ADC reading after calibration (offset pt) */
    float    verify_gain_mv;        /* ADC reading after calibration (gain pt) */
    bool     success;
} CalibrationReport;

CalResult Calibration_Run(MCP3465R_Handle *adc, DAC80501_Handle *dac,
                           CalibrationReport *report);

/* Helper: read multiple ADC samples and return the average in mV */
bool Calibration_AverageADC(MCP3465R_Handle *adc, uint32_t samples,
                             float *avg_mv);

/* Print report over UART (optional, implement in main.c) */
void Calibration_PrintReport(const CalibrationReport *r);

#endif /* CALIBRATION_H */
