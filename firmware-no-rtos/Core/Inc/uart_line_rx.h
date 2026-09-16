/**
 * uart_line_rx.h
 *
 * USART1/USART2/USART3 공용: idle-line 검출 + Circular DMA로 수신한 바이트를
 * SPSC 링버퍼에 적재하는 헬퍼. HAL_UARTEx_RxEventCallback에서 각 드라이버가
 * 자신의 huart 인스턴스인지 확인 후 UartLineRx_HandleEvent()를 호출하는 식으로 사용한다.
 */
#ifndef UART_LINE_RX_H
#define UART_LINE_RX_H

#include "usart.h" /* CubeMX 생성: UART_HandleTypeDef, huart1/huart2/huart3 */
#include "ring_buffer.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    UART_HandleTypeDef *huart;
    uint8_t             *dma_buf;
    uint16_t              dma_buf_size;
    uint16_t              last_pos;
    RingBuffer_t          rb; /* 필요시 소비자 쪽에서 직접 RingBuffer_* API로 raw 접근 가능 */
} UartLineRx_t;

void UartLineRx_Init(UartLineRx_t *ctx, UART_HandleTypeDef *huart,
                      uint8_t *dma_buf, uint16_t dma_buf_size,
                      uint8_t *rb_storage, uint32_t rb_capacity);

/* HAL_UARTEx_ReceiveToIdle_DMA 시작. main()의 App_Init() 단계(super-loop 진입 전)에서 호출할 것. */
void UartLineRx_Start(UartLineRx_t *ctx);

/* HAL_UARTEx_RxEventCallback(huart, pos)에서 huart 일치 확인 후 호출 */
void UartLineRx_HandleEvent(UartLineRx_t *ctx, uint16_t pos);

/* delim(예: '\n')까지의 한 줄을 out에 NUL 종단하여 복사(성공 시 true). out_size 초과분은 버림. */
bool UartLineRx_PopLine(UartLineRx_t *ctx, char *out, uint32_t out_size, uint8_t delim);

#ifdef __cplusplus
}
#endif

#endif /* UART_LINE_RX_H */
