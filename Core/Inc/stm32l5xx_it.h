/**
 ******************************************************************************
 * @file    stm32l5xx_it.h
 * @brief   인터럽트 핸들러 선언
 ******************************************************************************
 */

#ifndef __STM32L5XX_IT_H
#define __STM32L5XX_IT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Cortex-M33 Core Interrupt Handlers */
void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SecureFault_Handler(void);
void DebugMon_Handler(void);
void SysTick_Handler(void);

/* Peripheral Interrupt Handlers */
void USART2_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32L5XX_IT_H */
