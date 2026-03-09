/**
 ******************************************************************************
 * @file    uart_gpdma.h
 * @brief   USART3 + GPDMA1 Channel 0 비동기 TX (링 버퍼 방식)
 *
 *  SD 카드 IDMA 와 충돌하지 않는 이유:
 *    ┌───────────────────────────────────────────────────────────┐
 *    │  SDMMC1  → [IDMA]  → SRAM2(sd_buf)   : 내장 DMA 엔진   │
 *    │  USART3  ← [GPDMA1 CH0] ← SRAM2(tx_buf) : 외부 GPDMA   │
 *    │                                                           │
 *    │  두 DMA 는 서로 다른 엔진. AHB 버스를 공유하더라도       │
 *    │  arbiter 가 중재하여 충돌 없이 동시 동작.               │
 *    └───────────────────────────────────────────────────────────┘
 *
 *  링 버퍼 구조:
 *    - 애플리케이션 → UART_Printf() → s_ring[] 에 복사
 *    - 백그라운드 (인터럽트 콜백) → GPDMA 전송 완료 시
 *      남은 데이터가 있으면 자동으로 다음 청크 전송 시작
 *    - SD IDMA 전송 중에도 UART 큐에 메시지 누적 가능
 *
 *  D-Cache:
 *    tx 버퍼가 MPU Non-Cacheable 영역에 있으므로 SCB 호출 불필요.
 *
 *  대상: NUCLEO-H563ZI, USART3 PD8/PD9 (ST-Link VCP)
 ******************************************************************************
 */
#ifndef UART_GPDMA_H
#define UART_GPDMA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h5xx_hal.h"
#include <stdarg.h>
#include <stdint.h>

/* ── 설정 상수 ──────────────────────────────────────────────────────────── */

/** UART 링 버퍼 총 크기 (2의 거듭제곱 권장) */
#define UART_RING_SIZE      1024U

/** 한 번에 GPDMA 로 전송하는 최대 바이트 수 */
#define UART_DMA_CHUNK      256U

/** TX 완료 대기 타임아웃 (ms) */
#define UART_TX_TIMEOUT_MS  3000U

/* ── HAL 핸들 (uart_gpdma.c 에서 정의) ─────────────────────────────────── */
extern UART_HandleTypeDef huart3;
extern DMA_HandleTypeDef  hdma_usart3_tx;

/* ── 공개 API ────────────────────────────────────────────────────────────── */

/**
 * @brief  USART3 + GPDMA1 CH0 초기화
 *
 *  초기화 항목:
 *    ① GPDMA1 CH0 핸들 설정 및 HAL_DMA_Init
 *    ② USART3 핸들 설정 및 HAL_UART_Init
 *    ③ __HAL_LINKDMA 로 DMA↔UART 연결
 *    ④ NVIC 우선순위 설정
 *    ⑤ 링 버퍼 초기화
 *
 * @note  SystemClock_Config() 및 MPU_ConfigDMARegion() 이후 호출
 */
void UART_GPDMA_Init(void);

/**
 * @brief  포맷 문자열을 UART 링 버퍼에 추가 (비동기)
 *
 *  SD IDMA 전송 중에도 안전하게 호출 가능.
 *  링 버퍼가 가득 찬 경우 초과 데이터는 버림.
 *
 * @param  fmt  printf 호환 포맷 문자열
 * @param  ...  가변 인수
 */
void UART_Printf(const char *fmt, ...);

/**
 * @brief  문자열을 UART 링 버퍼에 추가 (비동기)
 * @param  str  NULL 종단 문자열
 */
void UART_SendStr(const char *str);

/**
 * @brief  링 버퍼의 모든 데이터가 전송될 때까지 블로킹 대기
 * @retval 0=완료, 1=타임아웃
 */
uint8_t UART_Flush(void);

/**
 * @brief  GPDMA1 Channel 0 IRQ 핸들러 (stm32h5xx_it.c 에서 호출)
 */
void UART_GPDMA_IRQHandler(void);

#ifdef __cplusplus
}
#endif
#endif /* UART_GPDMA_H */
