/**
 ******************************************************************************
 * @file    usart1_dma_idle.h
 * @brief   STM32L552R USART1 DMA(Normal) + IDLE Line Interrupt RX driver
 *
 * 수신 프레임 포맷 : STX(0x02) + DATA(...) + ETX(0x03)
 ******************************************************************************
 */
#ifndef __USART1_DMA_IDLE_H
#define __USART1_DMA_IDLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ===== 사용자 설정 ===== */
#define USART1_RX_BUF_SIZE      256U    /* DMA 수신 버퍼 크기 */
#define USART1_FRAME_MAX_SIZE   256U    /* 추출 프레임 최대 크기 */

#define FRAME_STX               0x02U
#define FRAME_ETX               0x03U

/* 수신 완료 콜백에 전달되는 프레임 구조체 */
typedef struct
{
    uint8_t  data[USART1_FRAME_MAX_SIZE];   /* STX/ETX 를 제외한 payload */
    uint16_t length;                        /* payload 길이 */
} Usart1_Frame_t;

/* ===== API ===== */
HAL_StatusTypeDef USART1_DMA_IDLE_Init(void);
void              USART1_DMA_IDLE_IRQHandler(void);   /* USART1_IRQHandler 에서 호출 */
void              USART1_DMA_RxCpltCallback(void);    /* DMA TC ISR 에서 호출 (옵션) */

/* 메인 루프에서 polling 으로 호출 → 새 프레임이 있으면 true */
bool              USART1_GetFrame(Usart1_Frame_t *out);

/* 사용자 콜백 (인터럽트 컨텍스트에서 호출됨, 짧게 처리할 것) */
__weak void       USART1_FrameReceivedCallback(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif
#endif /* __USART1_DMA_IDLE_H */
