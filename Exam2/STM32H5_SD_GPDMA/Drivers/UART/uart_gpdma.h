/**
 ******************************************************************************
 * @file    uart_gpdma.h
 * @brief   USART3 + GPDMA1 Channel 0 비동기 전송 드라이버
 *
 *  목적:
 *    기존의 HAL_UART_Transmit (블로킹) 대신 GPDMA 기반 비동기 TX 를 제공.
 *    SD 카드 IDMA 전송이 진행되는 동안에도 UART 로그를 전송 가능.
 *
 *  구조:
 *    - GPDMA1 Channel 0 : USART3 TX 전용
 *    - 내부 전송 완료 플래그 (g_uart_tx_done) 으로 동기화
 *    - printf 스타일 래퍼 (UART_Printf) 제공
 *
 *  D-Cache 정합:
 *    GPDMA 가 메모리를 읽기 전 SCB_CleanDCache_by_Addr 수행.
 *    송신 버퍼는 32바이트 정렬 및 .DMABufferSection 권장.
 *
 *  대상 보드: NUCLEO-H563ZI (STM32H563ZI)
 *  VCP 핀   : PD8(TX) / PD9(RX) → ST-Link
 ******************************************************************************
 */
#ifndef UART_GPDMA_H
#define UART_GPDMA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h5xx_hal.h"
#include <stdint.h>
#include <stdarg.h>

/* ── 설정 상수 ──────────────────────────────────────────────────────────── */
#define UART_TX_BUF_SIZE    512U    /**< 내부 DMA 전송 버퍼 크기 (바이트)   */
#define UART_TX_TIMEOUT_MS  2000U   /**< TX 완료 대기 최대 시간             */
#define UART_DCACHE_LINE_SZ 32U     /**< D-Cache 라인 크기                  */

/* ── HAL 핸들 (main.c 에서 정의) ────────────────────────────────────────── */
extern UART_HandleTypeDef  huart3;
extern DMA_HandleTypeDef   hdma_usart3_tx;

/* ── 공개 API ────────────────────────────────────────────────────────────── */

/**
 * @brief  USART3 + GPDMA1 Channel 0 초기화
 *
 *  초기화 순서:
 *    1. GPDMA1 CH0 클럭/핸들 설정
 *    2. USART3 핸들 설정 및 HAL_UART_Init
 *    3. DMA 핸들을 UART TX 에 연결 (__HAL_LINKDMA)
 *    4. NVIC 우선순위 설정
 *
 * @note  이 함수를 호출하기 전 SystemClock_Config() 완료 필요.
 */
void UART_GPDMA_Init(void);

/**
 * @brief  문자열을 GPDMA 로 비동기 전송 (이전 전송 완료 후 진행)
 *
 * @param  str   전송할 NULL 종단 문자열
 *
 * @note   이전 GPDMA TX 가 진행 중이면 완료될 때까지 블로킹 대기 후 전송.
 *         SD IDMA 와 시간 중복 허용 (서로 다른 DMA 엔진).
 */
void UART_SendStr(const char *str);

/**
 * @brief  printf 스타일 UART 전송
 *
 * @param  fmt   포맷 문자열 (printf 호환)
 * @param  ...   가변 인수
 *
 * @note   내부 버퍼 크기(UART_TX_BUF_SIZE)를 초과하는 문자열은 잘림.
 */
void UART_Printf(const char *fmt, ...);

/**
 * @brief  현재 UART TX GPDMA 전송이 완료될 때까지 대기 (폴링)
 *
 * @retval 0=완료, 1=타임아웃
 */
uint8_t UART_WaitTxDone(void);

/**
 * @brief  GPDMA1 Channel 0 IRQ 핸들러 – stm32h5xx_it.c 에서 호출
 */
void UART_GPDMA_IRQHandler(void);

#ifdef __cplusplus
}
#endif
#endif /* UART_GPDMA_H */
