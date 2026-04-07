/**
 ******************************************************************************
 * @file    uart_dma_parser.h
 * @brief   STM32L552 + FreeRTOS USART1 DMA(Normal) + IDLE Line 수신/파서
 *
 *   프레임 포맷 : [STX(0x02)] [DATA ...] [ETX(0x03)]
 *   수신 방식   : DMA Normal Mode + USART IDLE Line Interrupt
 *   전달 방식   : FreeRTOS Queue -> Parser Task
 ******************************************************************************
 */
#ifndef __UART_DMA_PARSER_H
#define __UART_DMA_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <stdbool.h>

/* ---------- 사용자 설정 ---------- */
#define UART_RX_DMA_BUF_SIZE    256U   /* DMA 수신 임시 버퍼 크기 */
#define UART_RX_FRAME_MAX_SIZE  256U   /* 한 프레임 최대 데이터 크기 */
#define UART_RX_QUEUE_LEN       8U     /* 수신 프레임 큐 길이 */

/* ---------- 프로토콜 상수 ---------- */
#define STX_BYTE                0x02U
#define ETX_BYTE                0x03U

/* ---------- 한 프레임 표현 ---------- */
typedef struct {
    uint16_t length;                         /* STX/ETX 제외 DATA 길이 */
    uint8_t  data[UART_RX_FRAME_MAX_SIZE];
} UartFrame_t;

/* ---------- 전역 핸들 ---------- */
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef  hdma_usart1_rx;
extern osMessageQueueId_t uartRxQueueHandle;

/* ---------- API ---------- */
/**
 * @brief  UART1 DMA + IDLE 수신 시작. FreeRTOS 초기화 이후 1회 호출.
 */
void UART_DmaIdle_Start(void);

/**
 * @brief  USART1 IRQHandler 안에서 반드시 호출 (IDLE flag 처리).
 *         stm32l5xx_it.c 의 USART1_IRQHandler() 내부에서 호출한다.
 */
void UART_DmaIdle_IRQHandler(UART_HandleTypeDef *huart);

/**
 * @brief  파서 태스크 엔트리. osThreadNew 로 생성하여 사용한다.
 */
void UART_ParserTask(void *argument);

/**
 * @brief  파싱이 완료된 한 프레임이 전달되는 콜백.
 *         사용자가 원하는 동작을 구현한다 (weak).
 */
void UART_OnFrameReceived(const UartFrame_t *frame);

#ifdef __cplusplus
}
#endif
#endif /* __UART_DMA_PARSER_H */
