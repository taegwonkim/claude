/**
 ******************************************************************************
 * @file    stm32h5xx_it.c
 * @brief   STM32H5 인터럽트 핸들러
 *
 *  NVIC 우선순위 배정:
 *    Preempt  4 : SDMMC1        – SD IDMA 완료/오류 (최우선)
 *    Preempt  6 : GPDMA1_CH0   – USART3 TX DMA 완료
 *    Preempt  6 : USART3       – UART 오류 처리
 *    Preempt 15 : SysTick      – HAL 틱 (HAL_Init 에서 자동 설정)
 *
 *  SD(4)가 UART DMA(6)보다 높으므로:
 *    GPDMA1 TX 완료 처리 중에 SDMMC 인터럽트가 발생해도
 *    선점(preempt)하여 SD 콜백을 먼저 처리.
 *    → SD_WriteBlocks / SD_ReadBlocks 의 타임아웃 방지.
 ******************************************************************************
 */
#include "main.h"
#include "sd_driver.h"
#include "uart_gpdma.h"

/* ────────────────────────────────────────────────────────────────────────── */
/*  시스템                                                                    */
/* ────────────────────────────────────────────────────────────────────────── */

void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ────────────────────────────────────────────────────────────────────────── */
/*  SDMMC1 – SD 카드 IDMA (Preempt = 4)                                     */
/* ────────────────────────────────────────────────────────────────────────── */

/**
 * @brief  SDMMC1 글로벌 인터럽트
 *
 *  내부에서 HAL_SD_IRQHandler 가 상태를 확인하고
 *  HAL_SD_RxCpltCallback / HAL_SD_TxCpltCallback / HAL_SD_ErrorCallback
 *  중 하나를 호출함.
 */
void SDMMC1_IRQHandler(void)
{
    SD_IRQHandler();
}

/* ────────────────────────────────────────────────────────────────────────── */
/*  GPDMA1 Channel 0 – USART3 TX (Preempt = 6)                              */
/* ────────────────────────────────────────────────────────────────────────── */

/**
 * @brief  GPDMA1 CH0 인터럽트 (USART3 TX DMA 완료/오류)
 *
 *  내부에서 HAL_DMA_IRQHandler 가 HAL_UART_TxCpltCallback 을 호출.
 *  uart_gpdma.c 의 TxCpltCallback 에서 링 버퍼 다음 청크 전송을 시작.
 */
void GPDMA1_Channel0_IRQHandler(void)
{
    UART_GPDMA_IRQHandler();
}

/* ────────────────────────────────────────────────────────────────────────── */
/*  USART3 – UART 오류 처리 (Preempt = 6)                                   */
/* ────────────────────────────────────────────────────────────────────────── */

/**
 * @brief  USART3 글로벌 인터럽트
 *
 *  TX DMA 완료는 GPDMA1_Channel0_IRQHandler 에서 처리.
 *  여기서는 오버런/프레이밍 오류 등 UART 자체 오류만 처리.
 */
void USART3_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart3);
}
