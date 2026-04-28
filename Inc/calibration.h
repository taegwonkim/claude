/**
 * Two-point ADC calibration using DAC80501 (DAC) → MCP3465R (ADC)
 *
 * Algorithm:
 *   1. Disable MCP3465R on-chip calibration registers (raw mode).
 *   2. Output V_LOW = 0.5V via DAC, average N ADC readings → code_low.
 *   3. Output V_HIGH = 4.5V via DAC, average N ADC readings → code_high.
 *   4. Compute GAINCAL and OFFSETCAL from two-point linear fit:
 *
 *        ADCDATA = ADCraw × (GAINCAL / 2^23) + OFFSETCAL
 *
 *      Solving for the calibration constants so that ADCDATA = ideal_code:
 *        GAINCAL   = 2^23 × (ideal_high − ideal_low) / (code_high − code_low)
 *        OFFSETCAL = ideal_low  − code_low × GAINCAL / 2^23
 *
 *   5. Write GAINCAL / OFFSETCAL to MCP3465R and enable calibration.
 *   6. Verify by re-reading at both voltages and checking residual error.
 */
#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "stm32l5xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ── Calibration voltage targets ────────────────────────────────────────── */
#define CAL_V_LOW               0.5f    /* offset calibration point (V) */
#define CAL_V_HIGH              4.5f    /* gain calibration point (V)   */
#define CAL_VREF_ADC            5.0f    /* MCP3465R external VREF (V)   */

/* Number of ADC samples to average at each calibration point */
#define CAL_AVG_SAMPLES         64U

/* Acceptable residual error after calibration */
#define CAL_PASS_THRESHOLD_MV   5.0f    /* ±5 mV */

/* ── Result structure ───────────────────────────────────────────────────── */
typedef struct {
    int32_t  offsetcal;          /* written to MCP3465R OFFSETCAL register */
    uint32_t gaincal;            /* written to MCP3465R GAINCAL register   */
    float    verify_low_v;       /* measured voltage at 0.5V after cal      */
    float    verify_high_v;      /* measured voltage at 4.5V after cal      */
    float    error_low_mv;       /* residual error at low point (mV)        */
    float    error_high_mv;      /* residual error at high point (mV)       */
    bool     pass;               /* true if both residuals < threshold      */
} CalibrationResult_t;

/* ── Public API ─────────────────────────────────────────────────────────── */

/**
 * RunCalibration - execute full two-point calibration sequence.
 *
 * @param result  output structure filled with calibration values and
 *                verification measurements.  May be NULL if not needed.
 * @return HAL_OK on success; HAL_ERROR if SPI error or ADC reading fails.
 *
 * Side effects: writes GAINCAL and OFFSETCAL to MCP3465R, leaves calibration
 * enabled.  The DAC output is left at 0V after completion.
 */
HAL_StatusTypeDef RunCalibration(CalibrationResult_t *result);

#endif /* CALIBRATION_H */
