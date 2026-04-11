/* pid.h - 이산 PID 컨트롤러 */
#ifndef PID_H
#define PID_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ==========================================================
 * PID 컨트롤러 구조체
 *   - 미분항: 측정값 미분(Derivative on Measurement)으로
 *     세트포인트 변경 시 미분 킥(kick) 방지
 *   - 적분항: Anti-windup 클램프 적용
 * ========================================================== */
typedef struct {
    /* 게인 */
    float Kp;               /* 비례 게인 */
    float Ki;               /* 적분 게인 */
    float Kd;               /* 미분 게인 */

    /* 상태 변수 */
    float setpoint;         /* 목표값 (V) */
    float integral;         /* 적분 누산값 */
    float prev_measurement; /* 이전 측정값 (미분항 계산용) */

    /* 출력 제한 */
    float output_min;       /* 출력 하한 (0.0) */
    float output_max;       /* 출력 상한 (1.0) */

    /* 샘플링 시간 */
    float dt;               /* 샘플링 주기 (초) */

    /* 활성화 플래그 */
    uint8_t enabled;
} PID_t;

/* ==========================================================
 * 함수 프로토타입
 * ========================================================== */

/**
 * @brief PID 컨트롤러 초기화
 * @param pid   PID 구조체 포인터
 * @param Kp    비례 게인
 * @param Ki    적분 게인
 * @param Kd    미분 게인
 * @param omin  출력 최솟값 (예: 0.0f)
 * @param omax  출력 최댓값 (예: 1.0f)
 * @param dt    샘플링 주기 (초)
 */
void PID_Init(PID_t *pid, float Kp, float Ki, float Kd,
              float omin, float omax, float dt);

/**
 * @brief PID 출력 계산
 * @param pid       PID 구조체 포인터
 * @param measured  현재 측정값 (V)
 * @return 제어 출력 (0.0 ~ 1.0)
 */
float PID_Compute(PID_t *pid, float measured);

/** @brief 적분 및 이전 측정값 초기화 */
void PID_Reset(PID_t *pid);

/** @brief 목표 전압 설정 */
void PID_SetSetpoint(PID_t *pid, float setpoint);

/** @brief PID 게인 업데이트 */
void PID_SetTunings(PID_t *pid, float Kp, float Ki, float Kd);

/** @brief 컨트롤러 활성화/비활성화 (0=off, 1=on) */
void PID_SetEnabled(PID_t *pid, uint8_t enabled);

#ifdef __cplusplus
}
#endif

#endif /* PID_H */
