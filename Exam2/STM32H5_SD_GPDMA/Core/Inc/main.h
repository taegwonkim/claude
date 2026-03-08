/**
 ******************************************************************************
 * @file    main.h
 * @brief   STM32H5 SD GPDMA 예제 – 설정 헤더
 *
 *  구성:
 *    - SDMMC1  : SD 카드 (IDMA, 4-bit)
 *    - USART3  : UART 로그 (GPDMA1 Channel 0 TX)
 *    - GPDMA1  : USART3 TX 전용 (SD 와 채널 충돌 없음)
 *
 *  대상 보드: NUCLEO-H563ZI (STM32H563ZI, Cortex-M33, 250 MHz)
 ******************************************************************************
 */
#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h5xx_hal.h"

/* ── HAL 핸들 선언 ──────────────────────────────────────────────────────── */
extern SD_HandleTypeDef   hsd1;       /* sd_driver.c   에서 정의 */
extern UART_HandleTypeDef huart3;     /* uart_gpdma.c  에서 정의 */
extern DMA_HandleTypeDef  hdma_usart3_tx; /* uart_gpdma.c  에서 정의 */

/* ── LED 핀 정의 (NUCLEO-H563ZI) ────────────────────────────────────────── */
#define LED_GREEN_PIN       GPIO_PIN_0
#define LED_GREEN_PORT      GPIOB
#define LED_YELLOW_PIN      GPIO_PIN_4
#define LED_YELLOW_PORT     GPIOF
#define LED_RED_PIN         GPIO_PIN_4
#define LED_RED_PORT        GPIOG

/* ── SD 테스트 설정 ─────────────────────────────────────────────────────── */

/** 읽기/쓰기 테스트 시작 섹터 (0 = 부트 섹터 – 데이터 카드에서만 사용) */
#define SD_TEST_SECTOR      100U

/** 읽기/쓰기 테스트 섹터 수 */
#define SD_TEST_COUNT       4U

/* ── 오류 핸들러 ────────────────────────────────────────────────────────── */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif
#endif /* MAIN_H */
