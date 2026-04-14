/**
 ******************************************************************************
 * @file    pid_controller.c
 * @brief   이산 PID 제어기 구현
 *
 * PID 알고리즘:
 *   e(k)     = setpoint - measurement
 *   P        = Kp × e(k)
 *   I        = Ki × Σ(e(i) × dt)          (Anti-windup 적용)
 *   D        = Kd × (e(k) - e(k-1)) / dt  (저역 통과 필터 옵션)
 *   output   = P + I + D                    (출력 제한 적용)
 *
 * Anti-Windup 전략:
 *   - 적분항 클램핑: integral_min ≤ I ≤ integral_max
 *   - 출력 포화 시 적분 동결 (back-calculation)
 *
 * 미분 킥 방지:
 *   - 설정값 변화 시 미분항 급변 방지를 위해
 *     오차 미분 대신 측정값 미분 사용 가능 (옵션)
 *
 * 미분 필터:
 *   D_filtered = alpha × D_prev + (1-alpha) × D_current
 *   alpha = 0 → 필터 없음, alpha = 0.9 → 강한 필터
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "pid_controller.h"

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  값을 지정 범위로 클램핑
 */
static float clamp(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/* Exported functions --------------------------------------------------------*/

void PID_Init(PID_Handle_t *pid, const PID_Config_t *config)
{
    if (pid == NULL || config == NULL) {
        return;
    }

    pid->kp = config->kp;
    pid->ki = config->ki;
    pid->kd = config->kd;
    pid->dt = config->dt;

    pid->out_min = config->out_min;
    pid->out_max = config->out_max;
    pid->integral_min = config->integral_min;
    pid->integral_max = config->integral_max;

    pid->d_filter_alpha = config->d_filter_alpha;

    /* 내부 상태 초기화 */
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->prev_measurement = 0.0f;
    pid->output = 0.0f;
    pid->d_filtered = 0.0f;

    pid->first_run = true;
    pid->enabled = true;
}

void PID_Reset(PID_Handle_t *pid)
{
    if (pid == NULL) {
        return;
    }

    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->prev_measurement = 0.0f;
    pid->output = 0.0f;
    pid->d_filtered = 0.0f;
    pid->first_run = true;
}

void PID_SetGains(PID_Handle_t *pid, float kp, float ki, float kd)
{
    if (pid == NULL) {
        return;
    }

    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
}

void PID_SetOutputLimits(PID_Handle_t *pid, float min, float max)
{
    if (pid == NULL) {
        return;
    }

    pid->out_min = min;
    pid->out_max = max;
}

void PID_SetEnabled(PID_Handle_t *pid, bool enable)
{
    if (pid == NULL) {
        return;
    }

    if (enable && !pid->enabled) {
        /* 활성화 시 적분항 리셋으로 급격한 출력 변화 방지 */
        PID_Reset(pid);
    }
    pid->enabled = enable;
}

float PID_Compute(PID_Handle_t *pid, float setpoint, float measurement)
{
    if (pid == NULL || !pid->enabled) {
        return 0.0f;
    }

    /* 오차 계산 */
    float error = setpoint - measurement;

    /* ---- 비례항 (P) ---- */
    float p_term = pid->kp * error;

    /* ---- 적분항 (I) with Anti-Windup ---- */
    pid->integral += error * pid->dt;

    /* 적분 클램핑 */
    pid->integral = clamp(pid->integral, pid->integral_min, pid->integral_max);

    float i_term = pid->ki * pid->integral;

    /* ---- 미분항 (D) ---- */
    float d_term = 0.0f;

    if (pid->first_run) {
        /* 첫 실행 시 미분항 = 0 (이전 값 없음) */
        pid->first_run = false;
    } else {
        if (pid->dt > 0.0f) {
            /* 오차 기반 미분 */
            float d_raw = (error - pid->prev_error) / pid->dt;

            /* 저역 통과 필터 적용 */
            if (pid->d_filter_alpha > 0.0f) {
                pid->d_filtered = pid->d_filter_alpha * pid->d_filtered
                                  + (1.0f - pid->d_filter_alpha) * d_raw;
                d_term = pid->kd * pid->d_filtered;
            } else {
                d_term = pid->kd * d_raw;
            }
        }
    }

    /* ---- 제어 출력 합산 ---- */
    float output = p_term + i_term + d_term;

    /* 출력 제한 */
    output = clamp(output, pid->out_min, pid->out_max);

    /*
     * Anti-windup: 출력이 포화 상태이면 적분 동결
     * (출력과 비포화 출력이 다르면 적분을 역산하여 보정)
     */
    float output_unsat = p_term + i_term + d_term;
    if (output != output_unsat) {
        /* 포화 발생 → 적분항에서 초과분 제거 */
        float excess = output_unsat - output;
        if (pid->ki != 0.0f) {
            pid->integral -= excess / pid->ki;
            pid->integral = clamp(pid->integral, pid->integral_min,
                                   pid->integral_max);
        }
    }

    /* 상태 저장 */
    pid->prev_error = error;
    pid->prev_measurement = measurement;
    pid->output = output;

    return output;
}

void PID_GetTerms(const PID_Handle_t *pid, float *p_term, float *i_term,
                   float *d_term)
{
    if (pid == NULL) {
        return;
    }

    if (p_term != NULL) {
        *p_term = pid->kp * pid->prev_error;
    }
    if (i_term != NULL) {
        *i_term = pid->ki * pid->integral;
    }
    if (d_term != NULL) {
        *d_term = pid->kd * pid->d_filtered;
    }
}
