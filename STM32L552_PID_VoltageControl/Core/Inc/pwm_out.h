/* pwm_out.h - AD5641 외부 DAC 인터페이스 (SPI1)
 *
 * AD5641: 14비트 단채널 전압 출력 DAC (Analog Devices)
 *   - SPI Mode 2 (CPOL=1, CPHA=0), 16비트 프레임
 *   - 16비트 입력: [PD1:PD0:D13:D12:...:D0]
 *     PD1:PD0=00 → 정상 동작
 *   - VREF=3.3V → 출력 0~3.3V (외부 앰프로 0~12V 증폭)
 *
 * 채널 ↔ CS 매핑:
 *   CH0 → CS_DAC0 (PB0)
 *   CH1 → CS_DAC1 (PB1)
 *   CH2 → CS_DAC2 (PB2)
 *   CH3 → CS_DAC3 (PB3)
 *
 * 함수 이름은 기존 pwm_out.h와 동일하게 유지
 * (channel_ctrl.c 등 호출부 변경 불필요)
 */
#ifndef PWM_OUT_H
#define PWM_OUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "main.h"

/* ==========================================================
 * 함수 프로토타입
 * ========================================================== */

/** @brief SPI1 CS 핀 GPIO 초기화 (main.c의 MX_GPIO_Init에서 호출) */
void PWMOut_Start(void);   /* 이름 유지: 초기화 + 전체 채널 DAC=0 출력 */
void PWMOut_Stop(void);    /* 전체 채널 DAC=0 (안전 상태) */

/**
 * @brief 채널 제어 출력 설정 (듀티 → DAC 코드 변환 후 SPI 전송)
 * @param ch    채널 번호 (0~3)
 * @param duty  0.0f (0V) ~ 1.0f (VREF=3.3V)
 */
void PWMOut_SetDuty(uint8_t ch, float duty);

/**
 * @brief DAC 코드 직접 설정
 * @param ch   채널 번호 (0~3)
 * @param code DAC 코드 (0 ~ 16383)
 */
void PWMOut_SetCode(uint8_t ch, uint16_t code);

/** @brief 채널 출력 강제 0 (보호 동작 시) */
void PWMOut_Disable(uint8_t ch);

/** @brief 현재 듀티 반환 (0.0f ~ 1.0f) */
float PWMOut_GetDuty(uint8_t ch);

#ifdef __cplusplus
}
#endif
#endif /* PWM_OUT_H */
