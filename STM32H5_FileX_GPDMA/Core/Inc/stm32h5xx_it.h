/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h5xx_it.h
  * @brief   인터럽트 핸들러 헤더
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __STM32H5xx_IT_H
#define __STM32H5xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Exported functions prototypes ---------------------------------------------*/
void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

/* GPDMA1 인터럽트 (SDMMC1 TX/RX) */
void GPDMA1_Channel0_IRQHandler(void);
void GPDMA1_Channel1_IRQHandler(void);

/* GPDMA2 인터럽트 (USART1 TX/RX) */
void GPDMA2_Channel0_IRQHandler(void);
void GPDMA2_Channel1_IRQHandler(void);

/* SDMMC1 인터럽트 */
void SDMMC1_IRQHandler(void);

/* USART1 인터럽트 */
void USART1_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32H5xx_IT_H */
