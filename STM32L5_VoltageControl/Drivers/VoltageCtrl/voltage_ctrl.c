/**
 ******************************************************************************
 * @file    voltage_ctrl.c
 * @brief   Closed-loop PI Voltage Controller Implementation
 ******************************************************************************
 */
#include "voltage_ctrl.h"
#include "app_config.h"

/* ── Initialize ───────────────────────────────────────────────────────── */

void VoltageCtrl_InitChannel(ChannelCtrl_t *ch, float kp, float ki)
{
    ch->pi.kp = kp;
    ch->pi.ki = ki;
    ch->pi.integral = 0.0f;
    ch->pi.output = 0.0f;
    ch->pi.error_prev = 0.0f;
    ch->pi.output_min = PI_OUTPUT_MIN;
    ch->pi.output_max = PI_OUTPUT_MAX;
    ch->pi.integral_limit = PI_INTEGRAL_LIMIT;

    ch->target_mv = 0;
    ch->measured_mv = 0;
    ch->current_ma = 0;
    ch->dac_raw = 0;
    ch->state = CH_STATE_DISABLED;
    ch->fault_count = 0;
    ch->enabled = false;
}

void VoltageCtrl_Reset(ChannelCtrl_t *ch)
{
    ch->pi.integral = 0.0f;
    ch->pi.output = 0.0f;
    ch->pi.error_prev = 0.0f;
    ch->dac_raw = 0;
}

/* ── Set Target ───────────────────────────────────────────────────────── */

void VoltageCtrl_SetTarget(ChannelCtrl_t *ch, uint16_t target_mv)
{
    if (target_mv > VOLTAGE_MAX_MV)
        target_mv = VOLTAGE_MAX_MV;

    ch->target_mv = target_mv;

    /* If target changes significantly, reset integral to avoid overshoot */
    float diff = (float)target_mv - (float)ch->measured_mv;
    if (diff > 500.0f || diff < -500.0f) {
        ch->pi.integral = 0.0f;
    }
}

/* ── PI Control Loop Iteration ────────────────────────────────────────── */

uint16_t VoltageCtrl_Update(ChannelCtrl_t *ch, uint16_t measured_mv)
{
    ch->measured_mv = measured_mv;

    /* If channel is disabled or in fault, output 0 */
    if (!ch->enabled || ch->state == CH_STATE_FAULT_SHORT) {
        ch->dac_raw = 0;
        ch->pi.integral = 0.0f;
        return 0;
    }

    /* Calculate error (target - measured) */
    float error = (float)ch->target_mv - (float)measured_mv;

    /* PI controller */
    /* Proportional term */
    float p_term = ch->pi.kp * error;

    /* Integral term with anti-windup clamping */
    ch->pi.integral += ch->pi.ki * error;
    if (ch->pi.integral > ch->pi.integral_limit)
        ch->pi.integral = ch->pi.integral_limit;
    else if (ch->pi.integral < -ch->pi.integral_limit)
        ch->pi.integral = -ch->pi.integral_limit;

    /* Calculate output */
    float output = p_term + ch->pi.integral;

    /* Clamp output to valid DAC/PWM range */
    if (output > ch->pi.output_max)
        output = ch->pi.output_max;
    else if (output < ch->pi.output_min)
        output = ch->pi.output_min;

    /* Additional anti-windup: if output is saturated, stop integrating */
    if (output >= ch->pi.output_max || output <= ch->pi.output_min) {
        /* Only stop integrating in the direction of saturation */
        if ((error > 0 && output >= ch->pi.output_max) ||
            (error < 0 && output <= ch->pi.output_min)) {
            ch->pi.integral -= ch->pi.ki * error;  /* Undo last integration */
        }
    }

    ch->pi.output = output;
    ch->pi.error_prev = error;
    ch->dac_raw = (uint16_t)output;

    return ch->dac_raw;
}

/* ── Stability Check ──────────────────────────────────────────────────── */

bool VoltageCtrl_IsStable(ChannelCtrl_t *ch)
{
    if (!ch->enabled) return false;

    int32_t diff = (int32_t)ch->target_mv - (int32_t)ch->measured_mv;
    if (diff < 0) diff = -diff;

    return (uint32_t)diff <= VOLTAGE_TOLERANCE_MV;
}

/* ── Runtime Gain Adjustment ──────────────────────────────────────────── */

void VoltageCtrl_SetGains(ChannelCtrl_t *ch, float kp, float ki)
{
    ch->pi.kp = kp;
    ch->pi.ki = ki;
    /* Reset integral to avoid transient from gain change */
    ch->pi.integral = 0.0f;
}
