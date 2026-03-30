/**
 ******************************************************************************
 * @file    fault_detect.c
 * @brief   Fault Detection Module Implementation
 ******************************************************************************
 */
#include "fault_detect.h"
#include <string.h>

/* ── Initialization ───────────────────────────────────────────────────── */

void FaultDetect_Init(FaultState_t states[NUM_CHANNELS])
{
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        states[i].current_fault = FAULT_NONE;
        states[i].short_count = 0;
        states[i].open_count = 0;
        states[i].fault_active = false;
        states[i].fault_latched = false;
    }
}

/* ── ADC to Current Conversion ────────────────────────────────────────── */

uint16_t FaultDetect_AdcToCurrent(uint16_t adc_raw)
{
    /*
     * Current sensing circuit:
     *   V_adc = I_load × R_shunt × Gain
     *   V_adc (mV) = (adc_raw / 4095) × 3300
     *   I_load (mA) = V_adc(mV) / (R_shunt(Ω) × Gain)
     *              = V_adc(mV) / (0.1 × 50)
     *              = V_adc(mV) / 5.0
     *
     * Using integer math:
     *   V_adc_mv = (adc_raw × 3300) / 4095
     *   I_load_ma = (V_adc_mv × 1000) / (SHUNT_RESISTOR_MOHM × CURRENT_AMP_GAIN)
     *             = V_adc_mv × 1000 / (100 × 50)
     *             = V_adc_mv × 1000 / 5000
     *             = V_adc_mv / 5
     */
    uint32_t v_adc_mv = ((uint32_t)adc_raw * VREF_MV) / (ADC_RESOLUTION - 1);
    uint32_t current_ma = (v_adc_mv * 1000U) /
                          ((uint32_t)SHUNT_RESISTOR_MOHM * CURRENT_AMP_GAIN);

    return (uint16_t)current_ma;
}

/* ── Fault Check ──────────────────────────────────────────────────────── */

bool FaultDetect_Check(FaultState_t *state,
                       const ChannelCtrl_t *ch_ctrl,
                       uint16_t current_ma,
                       FaultEvent_t *event)
{
    bool new_fault = false;

    /* Skip check if channel is disabled */
    if (!ch_ctrl->enabled) {
        state->short_count = 0;
        state->open_count = 0;
        return false;
    }

    /* ── Short Circuit Detection ──────────────────────────────────────── */
    if (current_ma > CURRENT_SHORT_THRESHOLD_MA) {
        state->short_count++;
        state->open_count = 0;  /* Reset open counter */

        if (state->short_count >= FAULT_DEBOUNCE_COUNT &&
            state->current_fault != FAULT_SHORT_CIRCUIT) {
            /* New short circuit fault detected */
            state->current_fault = FAULT_SHORT_CIRCUIT;
            state->fault_active = true;
            state->fault_latched = true;

            event->fault = FAULT_SHORT_CIRCUIT;
            event->measured_value = current_ma;
            new_fault = true;
        }
    }
    /* ── Open Circuit Detection ───────────────────────────────────────── */
    else if (ch_ctrl->target_mv > OPEN_DETECT_VOLTAGE_MIN_MV &&
             current_ma < CURRENT_OPEN_THRESHOLD_MA) {
        state->open_count++;
        state->short_count = 0;  /* Reset short counter */

        if (state->open_count >= FAULT_DEBOUNCE_COUNT &&
            state->current_fault != FAULT_OPEN_CIRCUIT) {
            /* New open circuit fault detected */
            state->current_fault = FAULT_OPEN_CIRCUIT;
            state->fault_active = true;
            state->fault_latched = true;

            event->fault = FAULT_OPEN_CIRCUIT;
            event->measured_value = current_ma;
            new_fault = true;
        }
    }
    /* ── Normal Operation ─────────────────────────────────────────────── */
    else {
        state->short_count = 0;
        state->open_count = 0;

        if (state->fault_active && !state->fault_latched) {
            state->current_fault = FAULT_NONE;
            state->fault_active = false;
        }
    }

    return new_fault;
}

/* ── Clear Fault ──────────────────────────────────────────────────────── */

void FaultDetect_ClearFault(FaultState_t *state)
{
    state->current_fault = FAULT_NONE;
    state->fault_active = false;
    state->fault_latched = false;
    state->short_count = 0;
    state->open_count = 0;
}

/* ── Any Fault Active ─────────────────────────────────────────────────── */

bool FaultDetect_AnyFault(FaultState_t states[NUM_CHANNELS])
{
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        if (states[i].fault_active) return true;
    }
    return false;
}
