/**
 ******************************************************************************
 * @file    dma_sram2_example.h
 * @brief   SRAM2 DMA 예제 헤더
 ******************************************************************************
 */

#ifndef DMA_SRAM2_EXAMPLE_H
#define DMA_SRAM2_EXAMPLE_H

#include <stdint.h>
#include "stm32h5xx_hal.h"

/* 버퍼 크기 정의 */
#define UART_DMA_BUF_SIZE   (256U)
#define SPI_DMA_BUF_SIZE    (512U)
#define MEM_DMA_BUF_WORDS   (256U)    /* 32비트 워드 단위 */

/* 외부에서 접근 가능한 SRAM2 버퍼 선언 */
extern uint8_t  uartRxDmaBuf[UART_DMA_BUF_SIZE];
extern uint8_t  uartTxDmaBuf[UART_DMA_BUF_SIZE];
extern uint8_t  spiRxDmaBuf[SPI_DMA_BUF_SIZE];
extern uint8_t  spiTxDmaBuf[SPI_DMA_BUF_SIZE];
extern uint32_t memDmaSrcBuf[MEM_DMA_BUF_WORDS];
extern uint32_t memDmaDstBuf[MEM_DMA_BUF_WORDS];

/* 함수 선언 */
void             MPU_ConfigSRAM2NonCacheable(void);
HAL_StatusTypeDef DMA_MemToMem_Init(void);
HAL_StatusTypeDef DMA_MemToMem_Transfer_SRAM2(uint32_t *src, uint32_t *dst, uint32_t numWords);
HAL_StatusTypeDef DMA_MemToMem_Transfer_SRAM1_Cached(uint32_t *src, uint32_t *dst, uint32_t numBytes);
void             DMA_SRAM2_Test(void);

/* IRQ 핸들러 및 콜백 */
void GPDMA1_Channel0_IRQHandler(void);
void DMA_TransferComplete_Callback(DMA_HandleTypeDef *hdma);
void DMA_TransferError_Callback(DMA_HandleTypeDef *hdma);

#endif /* DMA_SRAM2_EXAMPLE_H */
