/**
 ******************************************************************************
 * @file    main.h
 * @brief   STM32H5 SD + GPDMA UART 예제 – 전역 설정
 *
 *  대상 보드: NUCLEO-H563ZI (STM32H563ZI, 250 MHz)
 *
 *  DMA 구조:
 *    SDMMC1 IDMA → SD 카드 블록 R/W (내장 DMA, GPDMA와 독립)
 *    GPDMA1 CH0  → USART3 TX 비동기 (링 버퍼 기반)
 *
 *  충돌 방지:
 *    MPU Region 0 (SRAM2 0x20030000, 8KB) = Non-Cacheable
 *    → 모든 DMA 버퍼를 이 영역에 배치 → SCB 호출 불필요
 ******************************************************************************
 */
#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h5xx_hal.h"

/* ── HAL 핸들 선언 ──────────────────────────────────────────────────────── */
extern SD_HandleTypeDef   hsd1;
extern UART_HandleTypeDef huart3;
extern DMA_HandleTypeDef  hdma_usart3_tx;

/* ── NUCLEO-H563ZI LED ──────────────────────────────────────────────────── */
#define LED_GREEN_PIN    GPIO_PIN_0
#define LED_GREEN_PORT   GPIOB
#define LED_YELLOW_PIN   GPIO_PIN_4
#define LED_YELLOW_PORT  GPIOF
#define LED_RED_PIN      GPIO_PIN_4
#define LED_RED_PORT     GPIOG

/* ── SD 카드 테스트 설정 ────────────────────────────────────────────────── */

/** 읽기/쓰기 시작 LBA 섹터 (0 = MBR 영역, 데이터 전용 카드에서 사용) */
#define SD_TEST_SECTOR   200U

/** 읽기/쓰기 섹터 수 (섹터당 512 B → 4 섹터 = 2 KB) */
#define SD_TEST_COUNT    4U

/* ── 오류 핸들러 ────────────────────────────────────────────────────────── */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif
#endif /* MAIN_H */
