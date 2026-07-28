/**
 * ring_buffer.h
 *
 * Small byte ring buffer used to move data out of UART RX interrupt
 * context into task context. Single-producer (ISR) / single-consumer
 * (one task) use only - no internal locking.
 */
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *buf;
    uint16_t capacity;
    volatile uint16_t head;   /* next write index (producer/ISR)  */
    volatile uint16_t tail;   /* next read index  (consumer/task) */
} RingBuffer_t;

void     RingBuffer_Init(RingBuffer_t *rb, uint8_t *storage, uint16_t capacity);
bool     RingBuffer_PutByte(RingBuffer_t *rb, uint8_t byte);   /* call from ISR */
bool     RingBuffer_GetByte(RingBuffer_t *rb, uint8_t *byte);  /* call from task */
uint16_t RingBuffer_Count(const RingBuffer_t *rb);
void     RingBuffer_Reset(RingBuffer_t *rb);

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H */
