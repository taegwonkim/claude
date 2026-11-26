/**
 * @file    stm32h7xx_it.c
 * @brief   최소 예외 핸들러(Makefile 빌드용).
 *          CubeIDE 프로젝트에서는 CubeMX가 생성한 파일을 그대로 쓰면 됩니다.
 *          DMA1_Stream0_IRQHandler와 ADC_IRQHandler는 adc_dual.c에 있습니다.
 */
#include "main.h"

void NMI_Handler(void)        { while (1) {} }
void HardFault_Handler(void)  { while (1) {} }
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
