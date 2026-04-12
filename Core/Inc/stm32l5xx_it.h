/**
 * @file    stm32l5xx_it.h
 * @brief   인터럽트 핸들러 프로토타입
 */
#ifndef __STM32L5XX_IT_H
#define __STM32L5XX_IT_H

#include "stm32l5xx_hal.h"

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SecureFault_Handler(void);
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
void EXTI0_IRQHandler(void);

#endif /* __STM32L5XX_IT_H */
