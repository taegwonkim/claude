#include "ring_buffer.h"

void RingBuffer_Init(RingBuffer_t *rb, uint8_t *storage, uint32_t capacity)
{
    rb->buf = storage;
    rb->capacity = capacity;
    rb->head = 0;
    rb->tail = 0;
}

uint32_t RingBuffer_Count(const RingBuffer_t *rb)
{
    uint32_t head = rb->head;
    uint32_t tail = rb->tail;
    if (head >= tail) {
        return head - tail;
    }
    return rb->capacity - tail + head;
}

uint32_t RingBuffer_Free(const RingBuffer_t *rb)
{
    /* 항상 1바이트를 비워두어 head==tail이 "빈 상태"만 뜻하도록 한다(full/empty 구분). */
    return rb->capacity - 1U - RingBuffer_Count(rb);
}

uint32_t RingBuffer_Write(RingBuffer_t *rb, const uint8_t *data, uint32_t len)
{
    uint32_t free_space = RingBuffer_Free(rb);
    uint32_t to_write = (len > free_space) ? free_space : len;
    uint32_t head = rb->head;

    for (uint32_t i = 0; i < to_write; i++) {
        rb->buf[head] = data[i];
        head = (head + 1U) % rb->capacity;
    }
    rb->head = head;
    return to_write;
}

uint32_t RingBuffer_Read(RingBuffer_t *rb, uint8_t *out, uint32_t len)
{
    uint32_t count = RingBuffer_Count(rb);
    uint32_t to_read = (len > count) ? count : len;
    uint32_t tail = rb->tail;

    for (uint32_t i = 0; i < to_read; i++) {
        out[i] = rb->buf[tail];
        tail = (tail + 1U) % rb->capacity;
    }
    rb->tail = tail;
    return to_read;
}

uint32_t RingBuffer_FindByte(const RingBuffer_t *rb, uint8_t delim)
{
    uint32_t count = RingBuffer_Count(rb);
    uint32_t tail = rb->tail;

    for (uint32_t i = 0; i < count; i++) {
        if (rb->buf[(tail + i) % rb->capacity] == delim) {
            return i + 1U;
        }
    }
    return 0U;
}

bool RingBuffer_PopLine(RingBuffer_t *rb, char *out, uint32_t out_size, uint8_t delim)
{
    uint32_t line_len = RingBuffer_FindByte(rb, delim); /* delim 포함 길이 */

    if (line_len == 0U) {
        return false;
    }

    if (line_len <= (out_size - 1U)) {
        RingBuffer_Read(rb, (uint8_t *)out, line_len);
        out[line_len] = '\0';
    } else {
        uint8_t discard[32];
        uint32_t remaining;

        RingBuffer_Read(rb, (uint8_t *)out, out_size - 1U);
        out[out_size - 1U] = '\0';

        remaining = line_len - (out_size - 1U);
        while (remaining > 0U) {
            uint32_t chunk = (remaining > sizeof(discard)) ? sizeof(discard) : remaining;
            RingBuffer_Read(rb, discard, chunk);
            remaining -= chunk;
        }
    }
    return true;
}
