/**
 * @file    pid.c
 * @brief   PID Controller 구현
 *
 * Derivative-on-Measurement 방식을 사용하여 setpoint 급변 시
 * 미분 킥(derivative kick)을 방지합니다.
 * 적분 와인드업은 출력 클램프 + 적분 클램프 이중 방지를 적용합니다.
 */
#include "pid.h"

/* ------------------------------------------------------------------ */
/*  내부 유틸                                                           */
/* ------------------------------------------------------------------ */
static float clamp(float val, float lo, float hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

/* ------------------------------------------------------------------ */
/*  공개 API                                                            */
/* ------------------------------------------------------------------ */

void PID_Init(PID_TypeDef *pid,
              float kp, float ki, float kd,
              float output_min, float output_max,
              float dt)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;

    pid->output_min = output_min;
    pid->output_max = output_max;

    /* 적분 와인드업 범위: 출력 범위와 동일하게 초기화 */
    pid->integral_min = output_min;
    pid->integral_max = output_max;

    pid->dt = dt;

    pid->setpoint        = (output_min + output_max) * 0.5f;
    pid->integral        = 0.0f;
    pid->prev_error      = 0.0f;
    pid->prev_measurement = 0.0f;
}

void PID_SetSetpoint(PID_TypeDef *pid, float setpoint)
{
    pid->setpoint = clamp(setpoint, VOLTAGE_MIN, VOLTAGE_MAX);
}

float PID_Compute(PID_TypeDef *pid, float measurement)
{
    float error      = pid->setpoint - measurement;
    float derivative = 0.0f;

    /* 비례항 */
    float p_term = pid->kp * error;

    /* 적분항 (사다리꼴 적분, anti-windup 클램프) */
    pid->integral += pid->ki * error * pid->dt;
    pid->integral  = clamp(pid->integral, pid->integral_min, pid->integral_max);
    float i_term   = pid->integral;

    /* 미분항 - Derivative on Measurement (setpoint 변경 시 킥 방지) */
    derivative = -(measurement - pid->prev_measurement) / pid->dt;
    float d_term = pid->kd * derivative;

    /* 출력 계산 및 클램프 */
    float output = p_term + i_term + d_term;
    output = clamp(output, pid->output_min, pid->output_max);

    /* 상태 업데이트 */
    pid->prev_error       = error;
    pid->prev_measurement = measurement;

    return output;
}

void PID_Reset(PID_TypeDef *pid)
{
    pid->integral        = 0.0f;
    pid->prev_error      = 0.0f;
    pid->prev_measurement = 0.0f;
}
