#include "uart_line_rx.h"
#include <string.h>

void UartLineRx_Init(UartLineRx_t *ctx, UART_HandleTypeDef *huart,
                      uint8_t *dma_buf, uint16_t dma_buf_size,
                      uint8_t *rb_storage, uint32_t rb_capacity)
{
    ctx->huart = huart;
    ctx->dma_buf = dma_buf;
    ctx->dma_buf_size = dma_buf_size;
    ctx->last_pos = 0;
    RingBuffer_Init(&ctx->rb, rb_storage, rb_capacity);
}

void UartLineRx_Start(UartLineRx_t *ctx)
{
    ctx->last_pos = 0;
    HAL_UARTEx_ReceiveToIdle_DMA(ctx->huart, ctx->dma_buf, ctx->dma_buf_size);
    /* half-transfer 콜백은 사용하지 않음(idle/full 이벤트만으로 충분, 콜백 과다호출 방지) */
    __HAL_DMA_DISABLE_IT(ctx->huart->hdmarx, DMA_IT_HT);
}

void UartLineRx_HandleEvent(UartLineRx_t *ctx, uint16_t pos)
{
    if (pos == ctx->last_pos) {
        return;
    }

    if (pos > ctx->last_pos) {
        RingBuffer_Write(&ctx->rb, &ctx->dma_buf[ctx->last_pos], (uint32_t)(pos - ctx->last_pos));
    } else {
        RingBuffer_Write(&ctx->rb, &ctx->dma_buf[ctx->last_pos],
                          (uint32_t)(ctx->dma_buf_size - ctx->last_pos));
        RingBuffer_Write(&ctx->rb, &ctx->dma_buf[0], pos);
    }
    ctx->last_pos = pos;
}

bool UartLineRx_PopLine(UartLineRx_t *ctx, char *out, uint32_t out_size, uint8_t delim)
{
    return RingBuffer_PopLine(&ctx->rb, out, out_size, delim);
}
