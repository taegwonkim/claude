/* stm32l5xx_it.c - 인터럽트 서비스 루틴 (ISR) */
#include "stm32l5xx_it.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

/* HAL 핸들 외부 참조 */
extern DMA_HandleTypeDef   hdma_adc1;
extern FDCAN_HandleTypeDef hfdcan1;
extern TIM_HandleTypeDef   htim1;

/* ==========================================================
 * Cortex-M33 시스템 예외 핸들러
 * ========================================================== */

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

void SecureFault_Handler(void)
{
    while (1) {}
}

/* SVC_Handler, PendSV_Handler, SysTick_Handler:
   FreeRTOSConfig.h 에서 FreeRTOS 포트 함수로 매핑됨
   → xPortPendSVHandler / vPortSVCHandler / xPortSysTickHandler */

void DebugMon_Handler(void) {}

/* ==========================================================
 * DMA1 Channel1 (ADC1 DMA) IRQ
 * ========================================================== */
void DMA1_Channel1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_adc1);
}

/* ==========================================================
 * ADC1 IRQ (오버런 등 오류 처리)
 * ========================================================== */
void ADC1_IRQHandler(void)
{
    HAL_ADC_IRQHandler(&hadc1);
}

/* ==========================================================
 * TIM1 Update IRQ (필요 시 사용)
 * ========================================================== */
void TIM1_UP_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim1);
}

/* ==========================================================
 * FDCAN1 IT0 IRQ (RX FIFO0, TX 완료 등)
 * ========================================================== */
void FDCAN1_IT0_IRQHandler(void)
{
    HAL_FDCAN_IRQHandler(&hfdcan1);
}

/* ==========================================================
 * FDCAN1 IT1 IRQ (오류 경고, 버스 오프 등)
 * ========================================================== */
void FDCAN1_IT1_IRQHandler(void)
{
    HAL_FDCAN_IRQHandler(&hfdcan1);
}
