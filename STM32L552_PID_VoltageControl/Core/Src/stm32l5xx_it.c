/* stm32l5xx_it.c - 인터럽트 서비스 루틴
 *
 * 변경 사항 (내부 ADC/DMA → 외부 SPI ADC/DAC):
 *   제거: DMA1_Channel1_IRQHandler, ADC1_IRQHandler, TIM1_UP_IRQHandler
 *   추가: EXTI5_IRQHandler (/IRQ_ADC, MCP3465R 변환 완료)
 *   유지: SPI1/SPI2 오류 핸들러, FDCAN1 핸들러
 */
#include "stm32l5xx_it.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

extern SPI_HandleTypeDef   hspi1;
extern SPI_HandleTypeDef   hspi2;
extern FDCAN_HandleTypeDef hfdcan1;

/* ==========================================================
 * Cortex-M33 시스템 예외
 * ========================================================== */
void NMI_Handler(void)         { while (1) {} }
void HardFault_Handler(void)   { while (1) {} }
void MemManage_Handler(void)   { while (1) {} }
void BusFault_Handler(void)    { while (1) {} }
void UsageFault_Handler(void)  { while (1) {} }
void SecureFault_Handler(void) { while (1) {} }
void DebugMon_Handler(void)    {}

/* SVC / PendSV / SysTick → FreeRTOSConfig.h 에서 FreeRTOS 함수로 리다이렉트 */

/* ==========================================================
 * EXTI5 - MCP3465R /IRQ_ADC (PB5, 하강 엣지)
 *   변환 완료 알림 → ADCMeas_IRQHandler 호출
 * ========================================================== */
void EXTI5_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(ADC_IRQ_Pin);
    /* HAL_GPIO_EXTI_IRQHandler → HAL_GPIO_EXTI_Callback
       → adc_meas.c 의 ADCMeas_IRQHandler 호출 */
}

/* ==========================================================
 * SPI1 오류 핸들러 (AD5641)
 * ========================================================== */
void SPI1_IRQHandler(void)
{
    HAL_SPI_IRQHandler(&hspi1);
}

/* ==========================================================
 * SPI2 오류 핸들러 (MCP3465R)
 * ========================================================== */
void SPI2_IRQHandler(void)
{
    HAL_SPI_IRQHandler(&hspi2);
}

/* ==========================================================
 * FDCAN1 IT0 (RX FIFO0, TX 완료)
 * ========================================================== */
void FDCAN1_IT0_IRQHandler(void)
{
    HAL_FDCAN_IRQHandler(&hfdcan1);
}

/* ==========================================================
 * FDCAN1 IT1 (오류 경고, 버스 오프)
 * ========================================================== */
void FDCAN1_IT1_IRQHandler(void)
{
    HAL_FDCAN_IRQHandler(&hfdcan1);
}
