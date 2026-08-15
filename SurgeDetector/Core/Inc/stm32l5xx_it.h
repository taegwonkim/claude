#ifndef __STM32L5xx_IT_H
#define __STM32L5xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

void EXTI4_IRQHandler(void);
void USART1_IRQHandler(void);
void USART2_IRQHandler(void);
void USART3_IRQHandler(void);
void DMA1_Channel1_IRQHandler(void);
void DMA1_Channel2_IRQHandler(void);
void DMA1_Channel3_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32L5xx_IT_H */
