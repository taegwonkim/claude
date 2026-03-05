/**
 ******************************************************************************
 * @file    main.h
 * @brief   STM32H5 + FileX .s19 파일 스캐너 – 설정 헤더
 *
 *  대상 보드 : NUCLEO-H563ZI (STM32H563ZI, Cortex-M33, 250 MHz)
 ******************************************************************************
 */
#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h5xx_hal.h"

/* ── HAL 핸들 ───────────────────────────────────────────────────────────── */
extern SD_HandleTypeDef   hsd1;
extern UART_HandleTypeDef huart3;

/* ── NUCLEO-H563ZI LED ──────────────────────────────────────────────────── */
#define LED_GREEN_PIN       GPIO_PIN_0
#define LED_GREEN_PORT      GPIOB
#define LED_YELLOW_PIN      GPIO_PIN_4
#define LED_YELLOW_PORT     GPIOF
#define LED_RED_PIN         GPIO_PIN_4
#define LED_RED_PORT        GPIOG

/* ============================================================
 *  ★ 사용자 설정 ★
 * ============================================================ */

/** .s19 파일 탐색 시작 경로 ("/" = SD 루트) */
#define SCAN_ROOT_PATH      "/"

/** 하위 디렉터리 재귀 탐색 (1 = ON, 0 = OFF) */
#define SCAN_RECURSIVE      1U

/* ── 오류 핸들러 ────────────────────────────────────────────────────────── */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif
#endif /* MAIN_H */
