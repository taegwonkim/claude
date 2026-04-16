/**
 * @file    pid.h
 * @brief   PID Controller - 4채널 전압 제어용
 *
 * 각 채널별로 독립적인 PID 인스턴스를 사용합니다.
 * - 입력(측정값): MCP3465R로 읽은 전압 [V]
 * - 출력(제어값): AD5641에 쓸 전압 설정값 [V]
 * - 범위: 0.5V ~ 4.5V
 */
#ifndef PID_H
#define PID_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** 전압 제어 범위 */
#define VOLTAGE_MIN     0.5f
#define VOLTAGE_MAX     4.5f

/**
 * @brief PID 컨트롤러 인스턴스
 */
typedef struct
{
    /* 튜닝 파라미터 */
    float kp;               /**< 비례 게인 */
    float ki;               /**< 적분 게인 */
    float kd;               /**< 미분 게인 */

    /* 상태 변수 */
    float setpoint;         /**< 목표 전압 [V] */
    float integral;         /**< 적분 누산값 */
    float prev_error;       /**< 이전 오차 (미분용) */
    float prev_measurement; /**< 이전 측정값 (Derivative-on-Measurement) */

    /* 출력 제한 */
    float output_min;       /**< 출력 하한 [V] */
    float output_max;       /**< 출력 상한 [V] */

    /* Anti-windup 제한 */
    float integral_min;     /**< 적분 하한 */
    float integral_max;     /**< 적분 상한 */

    /* 샘플링 */
    float dt;               /**< 샘플 주기 [s] */
} PID_TypeDef;

/**
 * @brief  PID 컨트롤러 초기화
 * @param  pid        PID 인스턴스 포인터
 * @param  kp         비례 게인
 * @param  ki         적분 게인
 * @param  kd         미분 게인
 * @param  output_min 출력 하한 [V]
 * @param  output_max 출력 상한 [V]
 * @param  dt         샘플 주기 [s]
 */
void PID_Init(PID_TypeDef *pid,
              float kp, float ki, float kd,
              float output_min, float output_max,
              float dt);

/**
 * @brief  목표 전압 설정
 * @param  pid       PID 인스턴스 포인터
 * @param  setpoint  목표 전압 [V] (0.5V ~ 4.5V 클램프)
 */
void PID_SetSetpoint(PID_TypeDef *pid, float setpoint);

/**
 * @brief  PID 제어량 계산 (Derivative-on-Measurement 방식)
 * @param  pid          PID 인스턴스 포인터
 * @param  measurement  현재 측정 전압 [V]
 * @return 제어 출력 전압 [V]
 */
float PID_Compute(PID_TypeDef *pid, float measurement);

/**
 * @brief  PID 상태 초기화 (setpoint/게인은 유지)
 * @param  pid  PID 인스턴스 포인터
 */
void PID_Reset(PID_TypeDef *pid);

#ifdef __cplusplus
}
#endif

#endif /* PID_H */
