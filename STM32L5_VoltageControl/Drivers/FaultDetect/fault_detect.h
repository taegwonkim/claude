/**
 ******************************************************************************
 * @file    fault_detect.h
 * @brief   Fault Detection Module - Short Circuit & Open Circuit Detection
 ******************************************************************************
 *
 * Detection Method:
 *   - Short Circuit: Load current exceeds threshold (e.g., > 500mA)
 *   - Open Circuit:  Output voltage set but load current near zero (< 1mA)
 *   - Debouncing:    N consecutive fault readings before triggering alarm
 *
 * Current Sensing:
 *   - Shunt resistor (0.1Ω) in series with load
 *   - Current sense amplifier (INA180, gain=50)
 *   - ADC reads amplifier output: V_adc = I_load × R_shunt × Gain
 *   - I_load (mA) = (ADC_mV / Gain) / R_shunt_Ω × 1000
 *
 ******************************************************************************
 */
#ifndef __FAULT_DETECT_H
#define __FAULT_DETECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_types.h"
#include "app_config.h"

/* ── Per-Channel Fault State ──────────────────────────────────────────── */
typedef struct {
    FaultType_t current_fault;
    uint8_t     short_count;       /* Consecutive short-circuit readings */
    uint8_t     open_count;        /* Consecutive open-circuit readings */
    bool        fault_active;
    bool        fault_latched;     /* Stays true until manually cleared */
} FaultState_t;

/* ── API ──────────────────────────────────────────────────────────────── */

/**
 * @brief Initialize fault detection for all channels
 */
void FaultDetect_Init(FaultState_t states[NUM_CHANNELS]);

/**
 * @brief Convert raw ADC reading to current in milliamps
 * @param adc_raw  12-bit ADC raw value from internal ADC
 * @return Current in milliamps
 */
uint16_t FaultDetect_AdcToCurrent(uint16_t adc_raw);

/**
 * @brief Check a single channel for faults
 * @param state        Fault state for this channel
 * @param ch_ctrl      Channel control data (target, measured, etc.)
 * @param current_ma   Measured load current (mA)
 * @param event        Output: fault event if fault detected
 * @return true if a NEW fault event was generated
 */
bool FaultDetect_Check(FaultState_t *state,
                       const ChannelCtrl_t *ch_ctrl,
                       uint16_t current_ma,
                       FaultEvent_t *event);

/**
 * @brief Clear a latched fault for a channel
 */
void FaultDetect_ClearFault(FaultState_t *state);

/**
 * @brief Check if any channel has an active fault
 */
bool FaultDetect_AnyFault(FaultState_t states[NUM_CHANNELS]);

#ifdef __cplusplus
}
#endif

#endif /* __FAULT_DETECT_H */
