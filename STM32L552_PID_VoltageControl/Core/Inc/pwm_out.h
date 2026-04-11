/* pwm_out.h - TIM1 4채널 PWM 출력 인터페이스 */
#ifndef PWM_OUT_H
#define PWM_OUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "main.h"

/* ==========================================================
 * PWM 채널 ↔ TIM1 채널 매핑
 *
 *  채널  TIM1 채널  핀    설명
 *  ----  ---------  ---   ---------
 *   0    CH1        PA8   Buck1 PWM
 *   1    CH2        PA9   Buck2 PWM
 *   2    CH3        PA10  Buck3 PWM
 *   3    CH4        PA11  Buck4 PWM
 * ========================================================== */

/* ==========================================================
 * 함수 프로토타입
 * ========================================================== */

/** @brief TIM1 PWM 채널 4개 모두 시작 */
void PWMOut_Start(void);

/** @brief TIM1 PWM 채널 4개 모두 정지 (출력 0) */
void PWMOut_Stop(void);

/**
 * @brief 특정 채널 PWM 듀티 설정
 * @param ch    채널 번호 (0~3)
 * @param duty  듀티 비율 (0.0f ~ 1.0f)
 */
void PWMOut_SetDuty(uint8_t ch, float duty);

/**
 * @brief 특정 채널 PWM 듀티를 CCR 원시 값으로 설정
 * @param ch   채널 번호 (0~3)
 * @param ccr  CCR 값 (0 ~ PWM_ARR)
 */
void PWMOut_SetCCR(uint8_t ch, uint32_t ccr);

/**
 * @brief 채널 PWM 강제로 0 설정 (보호 동작 시)
 * @param ch  채널 번호 (0~3)
 */
void PWMOut_Disable(uint8_t ch);

/**
 * @brief 현재 듀티 반환 (0.0f ~ 1.0f)
 * @param ch  채널 번호 (0~3)
 */
float PWMOut_GetDuty(uint8_t ch);

#ifdef __cplusplus
}
#endif

#endif /* PWM_OUT_H */
