/**
 ******************************************************************************
 * @file    voltage_ctrl.h
 * @brief   Closed-loop PI Voltage Controller
 ******************************************************************************
 * Implements a PI controller per channel that adjusts DAC/PWM output to
 * match the target voltage based on MCP3465R feedback.
 ******************************************************************************
 */
#ifndef __VOLTAGE_CTRL_H
#define __VOLTAGE_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_types.h"
#include "voltage_output.h"

/* ── API ──────────────────────────────────────────────────────────────── */

/**
 * @brief Initialize PI controller for a channel
 */
void VoltageCtrl_InitChannel(ChannelCtrl_t *ch, float kp, float ki);

/**
 * @brief Reset PI controller (clear integral, output)
 */
void VoltageCtrl_Reset(ChannelCtrl_t *ch);

/**
 * @brief Set target voltage for a channel
 * @param ch       Channel control struct
 * @param target_mv Target voltage in mV
 */
void VoltageCtrl_SetTarget(ChannelCtrl_t *ch, uint16_t target_mv);

/**
 * @brief Execute one PI control iteration
 * @param ch             Channel control struct
 * @param measured_mv    Current measured voltage from ADC (mV)
 * @return New raw DAC/PWM value (0-4095)
 */
uint16_t VoltageCtrl_Update(ChannelCtrl_t *ch, uint16_t measured_mv);

/**
 * @brief Check if voltage is within tolerance
 */
bool VoltageCtrl_IsStable(ChannelCtrl_t *ch);

/**
 * @brief Update PI gains at runtime
 */
void VoltageCtrl_SetGains(ChannelCtrl_t *ch, float kp, float ki);

#ifdef __cplusplus
}
#endif

#endif /* __VOLTAGE_CTRL_H */
