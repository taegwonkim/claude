/**
 * @file    pid.h
 * @brief   PID Controller — anti-windup, derivative-on-measurement
 *
 * 채널별로 하나씩 할당하여 전압 조절에 사용.
 * 측정값에 대한 미분(derivative-on-measurement)을 적용하여
 * 설정값 변경 시 발생하는 미분 킥(derivative kick)을 방지.
 */
#ifndef INC_PID_H_
#define INC_PID_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    /* --- 이득 계수 --- */
    float Kp;               /**< 비례 이득 */
    float Ki;               /**< 적분 이득 */
    float Kd;               /**< 미분 이득 */

    /* --- 상태 변수 --- */
    float setpoint;         /**< 목표 전압 [V] */
    float integral;         /**< 적분 누적값 */
    float prev_measurement; /**< 이전 측정값 (미분-온-측정용) */

    /* --- 출력 및 적분 제한 --- */
    float output_min;       /**< 출력 하한 [V] */
    float output_max;       /**< 출력 상한 [V] */
    float integral_min;     /**< 적분 하한 (wind-up 방지) */
    float integral_max;     /**< 적분 상한 (wind-up 방지) */

    /* --- 샘플 주기 --- */
    float dt;               /**< 샘플링 주기 [s] */

    /* --- 플래그 --- */
    bool initialized;       /**< 최초 실행 여부 (미분 초기화용) */
} PID_HandleTypeDef;

/**
 * @brief PID 초기화
 * @param pid       PID 핸들
 * @param Kp        비례 이득
 * @param Ki        적분 이득
 * @param Kd        미분 이득
 * @param out_min   출력 하한 [V]
 * @param out_max   출력 상한 [V]
 * @param dt        샘플링 주기 [s]
 */
void PID_Init(PID_HandleTypeDef *pid,
              float Kp, float Ki, float Kd,
              float out_min, float out_max,
              float dt);

/**
 * @brief 목표값(설정점) 변경
 */
void PID_SetSetpoint(PID_HandleTypeDef *pid, float setpoint);

/**
 * @brief 이득 계수 런타임 변경
 */
void PID_SetGains(PID_HandleTypeDef *pid, float Kp, float Ki, float Kd);

/**
 * @brief 적분·미분 상태 초기화 (설정점 대폭 변경 시 권장)
 */
void PID_Reset(PID_HandleTypeDef *pid);

/**
 * @brief PID 출력 계산
 * @param pid         PID 핸들
 * @param measurement 현재 측정 전압 [V]
 * @return 제어 출력 [V] — output_min ~ output_max 범위로 클램핑됨
 */
float PID_Compute(PID_HandleTypeDef *pid, float measurement);

#endif /* INC_PID_H_ */
