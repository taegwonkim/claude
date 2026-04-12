/**
 * @file    pid.c
 * @brief   PID 제어기 구현
 *
 * 특징:
 *  - 미분-온-측정(Derivative-on-Measurement): 설정값 급변 시 미분 킥 방지
 *  - 조건부 적분(Conditional Integration): 출력이 포화 상태일 때 적분 정지
 *  - 출력 클램핑: output_min ~ output_max 범위 제한
 */
#include "pid.h"

/* ------------------------------------------------------------------ */
/*  내부 유틸                                                            */
/* ------------------------------------------------------------------ */
static inline float fclampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ------------------------------------------------------------------ */
/*  공개 API                                                            */
/* ------------------------------------------------------------------ */

void PID_Init(PID_HandleTypeDef *pid,
              float Kp, float Ki, float Kd,
              float out_min, float out_max,
              float dt)
{
    pid->Kp = Kp;
    pid->Ki = Ki;
    pid->Kd = Kd;
    pid->output_min   = out_min;
    pid->output_max   = out_max;
    pid->integral_min = out_min;
    pid->integral_max = out_max;
    pid->dt           = dt;

    pid->setpoint         = 0.0f;
    pid->integral         = 0.0f;
    pid->prev_measurement = 0.0f;
    pid->initialized      = false;
}

void PID_SetSetpoint(PID_HandleTypeDef *pid, float setpoint)
{
    pid->setpoint = setpoint;
}

void PID_SetGains(PID_HandleTypeDef *pid, float Kp, float Ki, float Kd)
{
    pid->Kp = Kp;
    pid->Ki = Ki;
    pid->Kd = Kd;
}

void PID_Reset(PID_HandleTypeDef *pid)
{
    pid->integral         = 0.0f;
    pid->prev_measurement = 0.0f;
    pid->initialized      = false;
}

float PID_Compute(PID_HandleTypeDef *pid, float measurement)
{
    float error = pid->setpoint - measurement;

    /* --- 비례항 --- */
    float p_term = pid->Kp * error;

    /* --- 적분항 (조건부 적분: 포화 방지용 클램핑) --- */
    float new_integral = pid->integral + error * pid->dt;
    new_integral = fclampf(new_integral, pid->integral_min / (pid->Ki + 1e-9f),
                                         pid->integral_max / (pid->Ki + 1e-9f));
    pid->integral = new_integral;
    float i_term = pid->Ki * pid->integral;

    /* --- 미분항 — 측정값 기반 (derivative-on-measurement) ---
     *  측정값의 음의 변화율을 사용 = -(dM/dt)
     *  오차 기반 미분 대신 측정 기반을 사용해 설정값 계단 변화 시
     *  발생하는 미분 킥(spike)을 방지 */
    float d_term = 0.0f;
    if (pid->initialized) {
        float d_measurement = (measurement - pid->prev_measurement) / pid->dt;
        d_term = -pid->Kd * d_measurement;
    } else {
        pid->initialized = true;
    }
    pid->prev_measurement = measurement;

    /* --- 출력 합산 및 클램핑 --- */
    float output = p_term + i_term + d_term;
    output = fclampf(output, pid->output_min, pid->output_max);

    /* --- Back-calculation anti-windup:
     *  출력이 포화되면 적분 누적을 되돌림 --- */
    float raw = p_term + i_term + d_term; /* 클램핑 전 값 */
    if (raw > pid->output_max || raw < pid->output_min) {
        /* 포화 상태 — 적분을 현재 스텝만큼 되돌림 */
        pid->integral -= error * pid->dt;
    }

    return output;
}
