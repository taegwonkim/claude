/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32l5xx_it.h
  * @brief   This file contains the headers of the interrupt handlers.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __STM32L5xx_IT_H
#define __STM32L5xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SecureFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
void RTC_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32L5xx_IT_H */
