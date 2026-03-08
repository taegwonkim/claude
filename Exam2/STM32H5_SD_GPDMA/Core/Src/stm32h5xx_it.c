/**
 ******************************************************************************
 * @file    stm32h5xx_it.c
 * @brief   STM32H5 인터럽트 핸들러
 *
 *  등록된 인터럽트:
 *    - SysTick_Handler        : HAL 틱 (1 ms)
 *    - SDMMC1_IRQHandler      : SD 카드 IDMA 완료/오류
 *    - GPDMA1_Channel0_IRQHandler : USART3 TX DMA 완료/오류
 *    - USART3_IRQHandler      : UART 오류 처리
 ******************************************************************************
 */
#include "main.h"
#include "sd_driver.h"
#include "uart_gpdma.h"

/* ── SysTick ────────────────────────────────────────────────────────────── */

/**
 * @brief  SysTick 인터럽트 핸들러 (1 ms, HAL 틱 카운터 증가)
 *
 * @note   HAL_GetTick() 기반 타임아웃이 이 핸들러에 의존.
 *         우선순위는 SDMMC/GPDMA 보다 낮게 설정 (Preempt=15).
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ── SDMMC1 (SD 카드 IDMA) ──────────────────────────────────────────────── */

/**
 * @brief  SDMMC1 글로벌 인터럽트
 *
 *  처리 내용:
 *    - IDMA 읽기/쓰기 완료 → HAL_SD_RxCpltCallback / HAL_SD_TxCpltCallback
 *    - 전송 오류            → HAL_SD_ErrorCallback
 *
 *  NVIC 우선순위: Preempt=4 (GPDMA=6 보다 높음 → SD 완료가 UART 보다 먼저 처리)
 */
void SDMMC1_IRQHandler(void)
{
    SD_IRQHandler();   /* sd_driver.c : HAL_SD_IRQHandler(&hsd1) 호출 */
}

/* ── GPDMA1 Channel 0 (USART3 TX) ──────────────────────────────────────── */

/**
 * @brief  GPDMA1 Channel 0 인터럽트 (USART3 TX DMA 완료/오류)
 *
 *  처리 내용:
 *    - TX 완료 → HAL_UART_TxCpltCallback → s_tx_done = 1
 *    - TX 오류 → HAL DMA 오류 처리
 *
 *  NVIC 우선순위: Preempt=6 (SDMMC=4 보다 낮음)
 */
void GPDMA1_Channel0_IRQHandler(void)
{
    UART_GPDMA_IRQHandler();   /* uart_gpdma.c : HAL_DMA_IRQHandler(&hdma_usart3_tx) */
}

/* ── USART3 (오류 처리) ─────────────────────────────────────────────────── */

/**
 * @brief  USART3 글로벌 인터럽트 (오류/수신 처리)
 *
 *  TX DMA 완료는 GPDMA1_Channel0_IRQHandler 에서 처리되므로
 *  여기서는 오류 및 RX 이벤트만 처리.
 */
void USART3_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart3);
}
