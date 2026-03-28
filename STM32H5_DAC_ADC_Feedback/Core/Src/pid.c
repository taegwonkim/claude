/**
 * @file    pid.c
 * @brief   Anti-windup PID 제어기 구현
 *
 * 설계 특징:
 *  - Derivative-on-Measurement: setpoint 급변 시 미분 충격(derivative kick) 방지
 *  - Clamping Anti-Windup    : 출력 포화 시 적분 항 누적 중단
 *  - 출력 클램핑             : output_min ~ output_max 범위로 제한
 */

#include "pid.h"
#include <string.h>

/* ========================================================================
 * 내부 유틸리티
 * ======================================================================== */

static inline float clampf(float val, float lo, float hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

/* ========================================================================
 * 공개 API 구현
 * ======================================================================== */

void PID_Init(PID_t *pid,
              float Kp, float Ki, float Kd,
              float out_min, float out_max,
              float dt)
{
    memset(pid, 0, sizeof(PID_t));

    pid->Kp         = Kp;
    pid->Ki         = Ki;
    pid->Kd         = Kd;
    pid->dt         = dt;
    pid->output_min = out_min;
    pid->output_max = out_max;

    /* 적분 항 anti-windup 한계: 출력 범위의 절반으로 초기화 */
    pid->integral_limit = (out_max - out_min) * 0.5f;

    pid->enabled    = true;
}

void PID_SetSetpoint(PID_t *pid, float setpoint)
{
    pid->setpoint = setpoint;
    /* setpoint 변경 시 적분 초기화는 하지 않음 (부드러운 추종 유지) */
}

float PID_Update(PID_t *pid, float measurement)
{
    float output;

    if (!pid->enabled) {
        return pid->output_min;
    }

    /* ---- 1. 오차 계산 ---- */
    float error = pid->setpoint - measurement;

    /* ---- 2. 비례 항 ---- */
    float p_term = pid->Kp * error;

    /* ---- 3. 적분 항 (Clamping Anti-Windup) ---- */
    float integral_new = pid->integral + pid->Ki * error * pid->dt;
    integral_new = clampf(integral_new, -pid->integral_limit, pid->integral_limit);
    pid->integral = integral_new;
    float i_term = pid->integral;

    /* ---- 4. 미분 항 (Derivative-on-Measurement, 미분 충격 방지) ---- */
    float d_term = -pid->Kd * (measurement - pid->prev_measurement) / pid->dt;

    /* ---- 5. 출력 합산 및 클램핑 ---- */
    output = p_term + i_term + d_term;
    output = clampf(output, pid->output_min, pid->output_max);

    /* ---- 6. Anti-Windup: 포화 상태에서 적분 누적 중단 ---- */
    float unclamped = p_term + pid->integral + d_term;
    if ((unclamped > pid->output_max && error > 0.0f) ||
        (unclamped < pid->output_min && error < 0.0f)) {
        /* 포화 + 같은 방향 오차 → 적분 되돌림 */
        pid->integral -= pid->Ki * error * pid->dt;
    }

    /* ---- 7. 상태 업데이트 ---- */
    pid->prev_error       = error;
    pid->prev_measurement = measurement;
    pid->error_v          = error;  /* 외부 모니터링용 */

    return output;
}

void PID_Reset(PID_t *pid)
{
    pid->integral         = 0.0f;
    pid->prev_error       = 0.0f;
    pid->prev_measurement = 0.0f;
}

void PID_SetEnabled(PID_t *pid, bool enable)
{
    if (!enable) {
        PID_Reset(pid);
    }
    pid->enabled = enable;
}
