/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h5xx_it.c
  * @brief   인터럽트 핸들러 구현
  *
  *  각 핸들러는 해당 HAL 함수를 호출하여 DMA/주변장치 상태를 처리.
  *  GPDMA1 ↔ SDMMC1 / GPDMA2 ↔ USART1 매핑이 핵심.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "stm32h5xx_it.h"

/* 외부 핸들 선언 */
extern SD_HandleTypeDef   hsd1;
extern UART_HandleTypeDef huart1;

extern DMA_HandleTypeDef hdma_sdmmc1_tx;
extern DMA_HandleTypeDef hdma_sdmmc1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA_HandleTypeDef hdma_usart1_rx;

/* ============================================================================
 *  Cortex-M33 예외 핸들러
 * ============================================================================ */
void NMI_Handler(void)
{
    while (1) {}
}

void HardFault_Handler(void)
{
    while (1) {}
}

void MemManage_Handler(void)
{
    while (1) {}
}

void BusFault_Handler(void)
{
    while (1) {}
}

void UsageFault_Handler(void)
{
    while (1) {}
}

void SVC_Handler(void)      {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void)   {}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ============================================================================
 *  GPDMA1 인터럽트 핸들러 (SDMMC1 TX/RX)
 * ============================================================================ */

/**
 * @brief GPDMA1 Channel 0: SDMMC1 TX 완료/에러
 */
void GPDMA1_Channel0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_sdmmc1_tx);
}

/**
 * @brief GPDMA1 Channel 1: SDMMC1 RX 완료/에러
 */
void GPDMA1_Channel1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_sdmmc1_rx);
}

/* ============================================================================
 *  SDMMC1 인터럽트 핸들러
 * ============================================================================ */

/**
 * @brief SDMMC1 전역 인터럽트
 *        - 카드 삽입/제거
 *        - 전송 완료
 *        - 오류 처리
 */
void SDMMC1_IRQHandler(void)
{
    HAL_SD_IRQHandler(&hsd1);
}

/* ============================================================================
 *  GPDMA2 인터럽트 핸들러 (USART1 TX/RX)
 * ============================================================================ */

/**
 * @brief GPDMA2 Channel 0: USART1 TX 완료/에러
 */
void GPDMA2_Channel0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}

/**
 * @brief GPDMA2 Channel 1: USART1 RX 완료/에러
 */
void GPDMA2_Channel1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
}

/* ============================================================================
 *  USART1 인터럽트 핸들러
 * ============================================================================ */

/**
 * @brief USART1 전역 인터럽트
 *        - IDLE 라인 감지 (DMA 수신 완료 감지에 활용)
 *        - 오류 처리 (framing, overrun 등)
 */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
