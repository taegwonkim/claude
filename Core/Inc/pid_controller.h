/**
 ******************************************************************************
 * @file    pid_controller.h
 * @brief   이산 PID 제어기 헤더
 *
 * 특성:
 *   - 이산 시간 PID (Proportional-Integral-Derivative)
 *   - Anti-windup (적분 클램핑)
 *   - 미분 킥 방지 (측정값 기반 미분)
 *   - 출력 제한
 *   - 저역 통과 필터된 미분항 (옵션)
 *
 * PID 공식:
 *   u(k) = Kp × e(k) + Ki × Σe(i)×dt + Kd × (e(k) - e(k-1)) / dt
 *
 *   여기서:
 *     e(k) = setpoint - measurement
 *     dt   = 샘플링 주기
 ******************************************************************************
 */

#ifndef __PID_CONTROLLER_H
#define __PID_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Types ---------------------------------------------------------------------*/

/**
 * @brief PID 제어기 구조체
 */
typedef struct {
    /* PID 게인 */
    float kp;               /**< 비례 게인 */
    float ki;               /**< 적분 게인 */
    float kd;               /**< 미분 게인 */

    /* 샘플링 시간 */
    float dt;               /**< 샘플링 주기 (초) */

    /* 출력 제한 */
    float out_min;           /**< 최소 출력 값 */
    float out_max;           /**< 최대 출력 값 */

    /* 적분 제한 (Anti-windup) */
    float integral_min;      /**< 적분항 최소 값 */
    float integral_max;      /**< 적분항 최대 값 */

    /* 내부 상태 */
    float integral;          /**< 적분 누적 값 */
    float prev_error;        /**< 이전 오차 값 */
    float prev_measurement;  /**< 이전 측정 값 (미분 킥 방지용) */
    float output;            /**< 현재 출력 값 */

    /* 미분 필터 계수 (0.0 ~ 1.0, 0=필터 없음) */
    float d_filter_alpha;    /**< 미분항 저역 통과 필터 계수 */
    float d_filtered;        /**< 필터된 미분항 */

    /* 상태 플래그 */
    bool  first_run;         /**< 첫 실행 여부 */
    bool  enabled;           /**< 제어기 활성화 여부 */
} PID_Handle_t;

/**
 * @brief PID 초기화 파라미터 구조체
 */
typedef struct {
    float kp;               /**< 비례 게인 */
    float ki;               /**< 적분 게인 */
    float kd;               /**< 미분 게인 */
    float dt;               /**< 샘플링 주기 (초) */
    float out_min;           /**< 최소 출력 */
    float out_max;           /**< 최대 출력 */
    float integral_min;      /**< 적분 최소 */
    float integral_max;      /**< 적분 최대 */
    float d_filter_alpha;    /**< 미분 필터 계수 */
} PID_Config_t;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  PID 제어기 초기화
 * @param  pid: PID 핸들 포인터
 * @param  config: 초기화 설정 포인터
 */
void PID_Init(PID_Handle_t *pid, const PID_Config_t *config);

/**
 * @brief  PID 제어기 리셋 (내부 상태 초기화)
 * @param  pid: PID 핸들 포인터
 */
void PID_Reset(PID_Handle_t *pid);

/**
 * @brief  PID 게인 업데이트 (런타임 튜닝)
 * @param  pid: PID 핸들 포인터
 * @param  kp: 새 비례 게인
 * @param  ki: 새 적분 게인
 * @param  kd: 새 미분 게인
 */
void PID_SetGains(PID_Handle_t *pid, float kp, float ki, float kd);

/**
 * @brief  출력 제한 설정
 * @param  pid: PID 핸들 포인터
 * @param  min: 최소 출력
 * @param  max: 최대 출력
 */
void PID_SetOutputLimits(PID_Handle_t *pid, float min, float max);

/**
 * @brief  PID 제어기 활성화/비활성화
 * @param  pid: PID 핸들 포인터
 * @param  enable: true=활성화, false=비활성화
 */
void PID_SetEnabled(PID_Handle_t *pid, bool enable);

/**
 * @brief  PID 연산 수행
 * @param  pid: PID 핸들 포인터
 * @param  setpoint: 목표 값
 * @param  measurement: 현재 측정 값
 * @retval 제어 출력 값
 */
float PID_Compute(PID_Handle_t *pid, float setpoint, float measurement);

/**
 * @brief  현재 PID 상태 조회
 * @param  pid: PID 핸들 포인터
 * @param  p_term: 비례항 저장 포인터 (NULL 가능)
 * @param  i_term: 적분항 저장 포인터 (NULL 가능)
 * @param  d_term: 미분항 저장 포인터 (NULL 가능)
 */
void PID_GetTerms(const PID_Handle_t *pid, float *p_term, float *i_term,
                   float *d_term);

#ifdef __cplusplus
}
#endif

#endif /* __PID_CONTROLLER_H */
