#include "ring_buffer.h"

void RingBuffer_Init(RingBuffer_t *rb, uint8_t *storage, uint16_t capacity)
{
    rb->buf = storage;
    rb->capacity = capacity;
    rb->head = 0;
    rb->tail = 0;
}

void RingBuffer_Reset(RingBuffer_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

bool RingBuffer_PutByte(RingBuffer_t *rb, uint8_t byte)
{
    uint16_t next = (uint16_t)((rb->head + 1U) % rb->capacity);
    if (next == rb->tail) {
        /* buffer full - drop byte */
        return false;
    }
    rb->buf[rb->head] = byte;
    rb->head = next;
    return true;
}

bool RingBuffer_GetByte(RingBuffer_t *rb, uint8_t *byte)
{
    if (rb->head == rb->tail) {
        return false; /* empty */
    }
    *byte = rb->buf[rb->tail];
    rb->tail = (uint16_t)((rb->tail + 1U) % rb->capacity);
    return true;
}

uint16_t RingBuffer_Count(const RingBuffer_t *rb)
{
    if (rb->head >= rb->tail) {
        return (uint16_t)(rb->head - rb->tail);
    }
    return (uint16_t)(rb->capacity - rb->tail + rb->head);
}
