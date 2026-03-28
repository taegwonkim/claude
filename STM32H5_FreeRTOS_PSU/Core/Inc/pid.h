/**
 * @file    pid.h
 * @brief   PI/PID 제어기 (전압 조절용)
 *
 * 전압 피드백 제어:
 *   - Setpoint : 목표 전압 (mV)
 *   - Feedback : ADC 측정 전압 (mV)
 *   - Output   : DAC 보정값 (0 ~ DAC_MAX_CODE)
 *
 * Anti-windup 처리, 출력 포화 클리핑 포함
 */

#ifndef PID_H
#define PID_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float kp;           /* 비례 이득 */
    float ki;           /* 적분 이득 */
    float kd;           /* 미분 이득 (0이면 PI 동작) */

    float integral;     /* 적분 누적값 */
    float prev_error;   /* 직전 오차 (미분용) */

    float out_min;      /* 출력 하한 */
    float out_max;      /* 출력 상한 */

    float dt;           /* 샘플 주기 (초) */
} PID_t;

/**
 * @brief PID 초기화
 * @param pid      PID 구조체 포인터
 * @param kp       비례 이득
 * @param ki       적분 이득
 * @param kd       미분 이득
 * @param out_min  출력 최솟값
 * @param out_max  출력 최댓값
 * @param dt       샘플 주기 (초)
 */
void PID_Init(PID_t *pid, float kp, float ki, float kd,
              float out_min, float out_max, float dt);

/**
 * @brief PID 연산 (매 샘플 주기마다 호출)
 * @param pid         PID 구조체 포인터
 * @param setpoint    목표값
 * @param measurement 현재 측정값
 * @return 제어 출력값
 */
float PID_Compute(PID_t *pid, float setpoint, float measurement);

/**
 * @brief PID 상태 초기화 (채널 활성화 시 호출)
 */
void PID_Reset(PID_t *pid);

/**
 * @brief 이득 실시간 변경
 */
void PID_SetTunings(PID_t *pid, float kp, float ki, float kd);

#ifdef __cplusplus
}
#endif

#endif /* PID_H */
