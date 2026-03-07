/**
 * @file    stm32h5xx_it.c
 * @brief   인터럽트 서비스 루틴 (ISR)
 *
 * 인터럽트 우선순위 (낮은 숫자 = 높은 우선순위)
 * -----------------------------------------------
 * 5 : GPDMA1_Channel0 (SDMMC1_RX), GPDMA1_Channel1 (SDMMC1_TX), SDMMC1
 * 6 : GPDMA2_Channel0 (USART1_TX), USART1
 *
 * SD 전송이 UART 전송보다 우선순위가 높아 SD DMA 중 UART DMA가
 * 선점(preempt)하지 못하므로 전송 끊김/데이터 손상 방지됨.
 */

#include "main.h"
#include "stm32h5xx_it.h"

/* -----------------------------------------------------------------------
 * Cortex-M33 Fault 핸들러
 * ----------------------------------------------------------------------- */
void NMI_Handler(void)
{
    while (1) {}
}

void HardFault_Handler(void)
{
    /* 디버깅 편의를 위해 레지스터 보존 후 무한 루프 */
    __asm volatile (
        "TST lr, #4       \n"
        "ITE EQ           \n"
        "MRSEQ r0, MSP    \n"
        "MRSNE r0, PSP    \n"
        "B .              \n"
    );
}

void MemManage_Handler(void)  { while (1) {} }
void BusFault_Handler(void)   { while (1) {} }
void UsageFault_Handler(void) { while (1) {} }
void SVC_Handler(void)        {}
void DebugMon_Handler(void)   {}
void PendSV_Handler(void)     {}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* -----------------------------------------------------------------------
 * GPDMA1 채널 인터럽트 — SDMMC1 DMA (우선순위 5)
 *
 * HAL_DMA_IRQHandler가 전송 완료/에러를 감지하고
 * 등록된 XferCpltCallback / XferErrorCallback을 호출함.
 * → 최종적으로 HAL_SD_RxCpltCallback / HAL_SD_TxCpltCallback이 실행됨.
 * ----------------------------------------------------------------------- */
void GPDMA1_Channel0_IRQHandler(void)
{
    /* SDMMC1 RX */
    HAL_DMA_IRQHandler(&hdma_sdmmc1_rx);
}

void GPDMA1_Channel1_IRQHandler(void)
{
    /* SDMMC1 TX */
    HAL_DMA_IRQHandler(&hdma_sdmmc1_tx);
}

/* -----------------------------------------------------------------------
 * GPDMA2 채널 인터럽트 — USART1 TX DMA (우선순위 6)
 * ----------------------------------------------------------------------- */
void GPDMA2_Channel0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}

/* -----------------------------------------------------------------------
 * SDMMC1 전역 인터럽트 (우선순위 5)
 * 명령 완료, CRC 오류, 타임아웃, FIFO 이벤트 등 처리
 * ----------------------------------------------------------------------- */
void SDMMC1_IRQHandler(void)
{
    HAL_SD_IRQHandler(&hsd1);
}

/* -----------------------------------------------------------------------
 * USART1 전역 인터럽트 (우선순위 6)
 * RX 수신, 에러, IDLE 등 처리
 * ----------------------------------------------------------------------- */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
