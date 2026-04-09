/* Core/Inc/main.h
 *
 * STM32L552R - TIM7 Polling + GPIO Control Example
 *
 * 인터럽트 없이 TIM7 Update Flag를 폴링하여 일정 주기마다 GPIO를 토글합니다.
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"

/* -----------------------------------------------------------------------
 * LED 핀 설정 (NUCLEO-L552ZE-Q 기준)
 *   - LD1 Green : PC7
 *   - LD2 Blue  : PB7
 *   다른 보드는 아래 매크로만 수정하면 됩니다.
 * ----------------------------------------------------------------------- */
#define LED_GREEN_PIN        GPIO_PIN_7
#define LED_GREEN_PORT       GPIOC

#define LED_BLUE_PIN         GPIO_PIN_7
#define LED_BLUE_PORT        GPIOB

/* -----------------------------------------------------------------------
 * TIM7 타이머 파라미터
 *
 * TIM7 클럭 소스: APB1 (기본 MSI 4 MHz)
 *
 *   Toggle 주기 = (PSC+1) × (ARR+1) / TIM7_CLK
 *              = 4000    × 500     / 4,000,000
 *              = 500 ms  (2 Hz 토글)
 *
 * 원하는 주기로 바꾸려면 아래 두 값만 조정하세요.
 *   예) 1초 주기: ARR = 1000-1  (PSC 그대로)
 *       100ms   : ARR = 100-1
 * ----------------------------------------------------------------------- */
#define TIM7_PRESCALER       (4000U - 1U)   /* 4 MHz / 4000 = 1 kHz tick */
#define TIM7_PERIOD          (500U  - 1U)   /* 1 kHz / 500  = 2 Hz (500 ms) */

/* Error handler */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
