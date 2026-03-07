#ifndef __STM32H5xx_IT_H
#define __STM32H5xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Cortex-M33 Fault 핸들러
 * ----------------------------------------------------------------------- */
void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

/* -----------------------------------------------------------------------
 * 주변장치 인터럽트 핸들러
 * ----------------------------------------------------------------------- */
void GPDMA1_Channel0_IRQHandler(void);   /* SDMMC1 RX DMA */
void GPDMA1_Channel1_IRQHandler(void);   /* SDMMC1 TX DMA */
void GPDMA2_Channel0_IRQHandler(void);   /* USART1 TX DMA */
void SDMMC1_IRQHandler(void);
void USART1_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32H5xx_IT_H */
