/**
 * @file    pid.c
 * @brief   PI/PID 제어기 구현
 */

#include "pid.h"

/* =========================================================
 * PID 초기화
 * ========================================================= */
void PID_Init(PID_t *pid, float kp, float ki, float kd,
              float out_min, float out_max, float dt)
{
    pid->kp        = kp;
    pid->ki        = ki;
    pid->kd        = kd;
    pid->integral  = 0.0f;
    pid->prev_error = 0.0f;
    pid->out_min   = out_min;
    pid->out_max   = out_max;
    pid->dt        = dt;
}

/* =========================================================
 * PID 연산
 * Anti-windup: 적분항을 출력 범위로 클리핑
 * ========================================================= */
float PID_Compute(PID_t *pid, float setpoint, float measurement)
{
    float error = setpoint - measurement;

    /* 비례항 */
    float p_term = pid->kp * error;

    /* 적분항 (Anti-windup: 출력 포화 시 적분 정지) */
    float i_term = pid->integral + pid->ki * error * pid->dt;
    /* 적분 포화 클리핑 */
    if (i_term > pid->out_max) {
        i_term = pid->out_max;
    } else if (i_term < pid->out_min) {
        i_term = pid->out_min;
    }
    pid->integral = i_term;

    /* 미분항 */
    float d_term = 0.0f;
    if (pid->dt > 0.0f) {
        d_term = pid->kd * (error - pid->prev_error) / pid->dt;
    }
    pid->prev_error = error;

    /* 출력 합산 및 포화 클리핑 */
    float output = p_term + i_term + d_term;
    if (output > pid->out_max) {
        output = pid->out_max;
    } else if (output < pid->out_min) {
        output = pid->out_min;
    }

    return output;
}

/* =========================================================
 * PID 상태 초기화
 * ========================================================= */
void PID_Reset(PID_t *pid)
{
    pid->integral   = 0.0f;
    pid->prev_error = 0.0f;
}

/* =========================================================
 * 이득 실시간 변경
 * ========================================================= */
void PID_SetTunings(PID_t *pid, float kp, float ki, float kd)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
}
