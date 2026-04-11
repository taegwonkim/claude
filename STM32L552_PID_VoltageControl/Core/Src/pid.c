/* pid.c - 이산 PID 컨트롤러 구현 */
#include "pid.h"

/* 클램프 매크로 */
#define CLAMP(x, lo, hi) \
    ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))

/* ==========================================================
 * PID_Init
 * ========================================================== */
void PID_Init(PID_t *pid, float Kp, float Ki, float Kd,
              float omin, float omax, float dt)
{
    pid->Kp               = Kp;
    pid->Ki               = Ki;
    pid->Kd               = Kd;
    pid->setpoint         = 0.0f;
    pid->integral         = 0.0f;
    pid->prev_measurement = 0.0f;
    pid->output_min       = omin;
    pid->output_max       = omax;
    pid->dt               = dt;
    pid->enabled          = 1U;
}

/* ==========================================================
 * PID_Compute
 *
 * 알고리즘:
 *   error    = setpoint - measured
 *   integral += error * dt           (Anti-windup: 클램프)
 *   dterm    = -(measured - prev) / dt  (측정값 미분)
 *   output   = Kp*error + Ki*integral + Kd*dterm
 * ========================================================== */
float PID_Compute(PID_t *pid, float measured)
{
    if (!pid->enabled) {
        return 0.0f;
    }

    float error = pid->setpoint - measured;

    /* 적분항 갱신 */
    pid->integral += error * pid->dt;

    /* Anti-windup: 적분 누산값을 출력 범위에 맞게 클램프 */
    if (pid->Ki > 1e-9f) {
        float i_limit = pid->output_max / pid->Ki;
        pid->integral = CLAMP(pid->integral, -i_limit, i_limit);
    } else {
        pid->integral = 0.0f;
    }

    /* 미분항: 세트포인트 변경 시 킥 방지를 위해 측정값 미분 사용 */
    float d_meas = (measured - pid->prev_measurement) / pid->dt;
    pid->prev_measurement = measured;

    float output = pid->Kp * error
                 + pid->Ki * pid->integral
                 - pid->Kd * d_meas;   /* 측정값 미분은 부호 반전 */

    return CLAMP(output, pid->output_min, pid->output_max);
}

/* ==========================================================
 * PID_Reset
 * ========================================================== */
void PID_Reset(PID_t *pid)
{
    pid->integral         = 0.0f;
    pid->prev_measurement = 0.0f;
}

/* ==========================================================
 * PID_SetSetpoint
 * ========================================================== */
void PID_SetSetpoint(PID_t *pid, float setpoint)
{
    pid->setpoint = setpoint;
}

/* ==========================================================
 * PID_SetTunings
 * ========================================================== */
void PID_SetTunings(PID_t *pid, float Kp, float Ki, float Kd)
{
    pid->Kp = Kp;
    pid->Ki = Ki;
    pid->Kd = Kd;
}

/* ==========================================================
 * PID_SetEnabled
 * ========================================================== */
void PID_SetEnabled(PID_t *pid, uint8_t enabled)
{
    pid->enabled = enabled;
    if (!enabled) {
        PID_Reset(pid);
    }
}
