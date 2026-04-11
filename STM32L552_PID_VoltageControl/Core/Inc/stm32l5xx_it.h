/* stm32l5xx_it.h - ISR 선언 */
#ifndef STM32L5XX_IT_H
#define STM32L5XX_IT_H

#ifdef __cplusplus
extern "C" {
#endif

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SecureFault_Handler(void);
void DebugMon_Handler(void);
void DMA1_Channel1_IRQHandler(void);
void ADC1_IRQHandler(void);
void TIM1_UP_IRQHandler(void);
void FDCAN1_IT0_IRQHandler(void);
void FDCAN1_IT1_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* STM32L5XX_IT_H */
