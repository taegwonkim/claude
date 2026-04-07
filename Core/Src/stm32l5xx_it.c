/**
 * @file    stm32l5xx_it.c
 * @brief   인터럽트 서비스 루틴 (ISR)
 *
 * CubeMX가 자동 생성하는 파일입니다.
 * USART1_IRQHandler 만 수정이 필요합니다.
 */

#include "main.h"
#include "stm32l5xx_it.h"
#include "usart_dma.h"
#include "FreeRTOS.h"
#include "task.h"

/* ─────────────────────────────────────────────────────────
 *  외부 선언
 * ───────────────────────────────────────────────────────── */
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef  hdma_usart1_rx;

/* ═════════════════════════════════════════════════════════
 *  Cortex-M33 예외 핸들러 (CubeMX 생성, 수정 불필요)
 * ═════════════════════════════════════════════════════════ */

void NMI_Handler(void)
{
    while (1) { }
}

void HardFault_Handler(void)
{
    while (1) { }
}

void MemManage_Handler(void)
{
    while (1) { }
}

void BusFault_Handler(void)
{
    while (1) { }
}

void UsageFault_Handler(void)
{
    while (1) { }
}

void SecureFault_Handler(void)
{
    while (1) { }
}

/* FreeRTOS 예약 핸들러 */
void SVC_Handler(void)          { }
void DebugMon_Handler(void)     { }
void PendSV_Handler(void)       { }
void SysTick_Handler(void)
{
    HAL_IncTick();
    /* FreeRTOS tick은 port.c 내부에서 처리 */
}

/* ═════════════════════════════════════════════════════════
 *  DMA1 Channel1 인터럽트 핸들러
 *
 *  DMA Transfer Complete(TC) 발생 시 HAL이 내부적으로
 *  HAL_UART_RxCpltCallback() 을 호출합니다.
 *  → USART_DMA_RxCpltCallback() 으로 전달됩니다.
 * ═════════════════════════════════════════════════════════ */
void DMA1_Channel1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
}

/* ═════════════════════════════════════════════════════════
 *  USART1 글로벌 인터럽트 핸들러
 *
 *  핵심 수정 사항:
 *    반드시 HAL_UART_IRQHandler() 보다 먼저
 *    USART_DMA_IdleCallback() 을 호출해야 합니다.
 *
 *  이유:
 *    HAL_UART_IRQHandler 내부에서 IDLE 플래그를 확인하지 않으므로,
 *    먼저 IDLE 플래그를 체크하고 처리한 뒤 HAL 루틴을 호출합니다.
 *    (HAL이 IDLE 플래그를 클리어하기 전에 직접 처리)
 * ═════════════════════════════════════════════════════════ */
void USART1_IRQHandler(void)
{
    /* ① IDLE Line 인터럽트 처리 (HAL보다 먼저) */
    USART_DMA_IdleCallback(&huart1);

    /* ② 나머지 UART 인터럽트 처리 (RXNE, TC, ERR 등) */
    HAL_UART_IRQHandler(&huart1);
}
