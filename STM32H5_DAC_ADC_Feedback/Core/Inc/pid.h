/**
 * @file    pid.h
 * @brief   Generic PID Controller for STM32H5 DAC/ADC feedback regulation
 *
 * Anti-windup 및 출력 클램핑이 적용된 표준 PID 제어기.
 * 각 DAC 채널에 독립적으로 인스턴스화하여 사용.
 */

#ifndef PID_H
#define PID_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief PID 제어기 상태 구조체
 *
 * 각 채널마다 독립적으로 할당하여 사용한다.
 */
typedef struct {
    /* ---- 튜닝 파라미터 ---- */
    float Kp;               /**< 비례 이득 */
    float Ki;               /**< 적분 이득 */
    float Kd;               /**< 미분 이득 */
    float dt;               /**< 샘플링 주기 [s] */

    /* ---- 목표값 ---- */
    float setpoint;         /**< 목표 전압 [V] */

    /* ---- 내부 상태 ---- */
    float integral;         /**< 적분 누적값 */
    float prev_error;       /**< 이전 오차 (미분 항 계산용) */
    float prev_measurement; /**< 이전 측정값 (Derivative-on-measurement) */

    /* ---- 출력 제한 ---- */
    float output_min;       /**< 최소 출력값 (DAC raw: 0.0f) */
    float output_max;       /**< 최대 출력값 (12-bit DAC: 4095.0f) */

    /* ---- Anti-windup 제한 ---- */
    float integral_limit;   /**< 적분 항 포화 방지 한계값 */

    /* ---- 상태 플래그 ---- */
    bool  enabled;          /**< 제어기 활성화 여부 */
} PID_t;

/* ---- API ---- */

/**
 * @brief  PID 제어기를 초기화한다.
 * @param  pid          PID 구조체 포인터
 * @param  Kp           비례 이득
 * @param  Ki           적분 이득
 * @param  Kd           미분 이득
 * @param  out_min      출력 하한 (보통 0.0)
 * @param  out_max      출력 상한 (12-bit DAC: 4095.0)
 * @param  dt           샘플링 주기 [s]
 */
void PID_Init(PID_t *pid,
              float Kp, float Ki, float Kd,
              float out_min, float out_max,
              float dt);

/**
 * @brief  목표 전압(setpoint)을 볼트 단위로 설정한다.
 * @param  pid       PID 구조체 포인터
 * @param  setpoint  목표 전압 [V]
 */
void PID_SetSetpoint(PID_t *pid, float setpoint);

/**
 * @brief  PID 연산을 수행하고 DAC raw 출력값(0~4095)을 반환한다.
 *
 * Derivative-on-measurement 방식으로 설정값 변경 시 미분 충격(kick)을 억제.
 *
 * @param  pid          PID 구조체 포인터
 * @param  measurement  ADC 피드백 측정값 [V]
 * @return float        DAC에 적용할 raw 출력값 (0.0 ~ 4095.0)
 */
float PID_Update(PID_t *pid, float measurement);

/**
 * @brief  PID 내부 상태(적분, 이전오차)를 초기화한다.
 * @param  pid  PID 구조체 포인터
 */
void PID_Reset(PID_t *pid);

/**
 * @brief  PID 제어기를 활성화/비활성화한다.
 * @param  pid      PID 구조체 포인터
 * @param  enable   true: 활성화, false: 비활성화(출력 0 유지)
 */
void PID_SetEnabled(PID_t *pid, bool enable);

#endif /* PID_H */
