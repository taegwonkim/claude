/**
 * @file main.h
 * @brief STM32L5 FreeRTOS - 5 LED Toggle Tasks
 *
 * Target : STM32L552/562
 * SysTick: TIM6 (HAL_SYSTICK -> TIM6 기반 오버라이드)
 * LEDs   : PB0, PB1, PB2, PB3, PB4 (5개)
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"

/* ---------------------------------------------------------------
 * LED 핀 정의 (GPIOB 사용, 보드에 맞게 수정)
 * --------------------------------------------------------------- */
#define LED1_PIN    GPIO_PIN_0   /* PB0 */
#define LED2_PIN    GPIO_PIN_1   /* PB1 */
#define LED3_PIN    GPIO_PIN_2   /* PB2 */
#define LED4_PIN    GPIO_PIN_3   /* PB3 */
#define LED5_PIN    GPIO_PIN_4   /* PB4 */
#define LED_PORT    GPIOB

/* ---------------------------------------------------------------
 * FreeRTOS 태스크 파라미터
 * --------------------------------------------------------------- */
#define LED_TASK_STACK_SIZE  128   /* words */
#define LED_TASK_PRIORITY    2

/* 각 LED 토글 주기 (ms) */
#define LED1_PERIOD_MS   500
#define LED2_PERIOD_MS   300
#define LED3_PERIOD_MS   700
#define LED4_PERIOD_MS   1000
#define LED5_PERIOD_MS   200

/* ---------------------------------------------------------------
 * 함수 선언
 * --------------------------------------------------------------- */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
