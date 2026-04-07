/**
 * @file    main.h
 * @brief   전역 헤더 (CubeMX 생성 형식)
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"

/* ────────────────────────────────────────────────────────────
 *  오류 핸들러
 * ──────────────────────────────────────────────────────────── */
void Error_Handler(void);

/* ────────────────────────────────────────────────────────────
 *  핀 별칭 (CubeMX 생성)
 * ──────────────────────────────────────────────────────────── */
#define LED_GREEN_Pin        GPIO_PIN_5
#define LED_GREEN_GPIO_Port  GPIOA

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
