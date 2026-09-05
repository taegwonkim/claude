/**
 * ring_buffer.h
 *
 * 단일 생산자(ISR/DMA)-단일 소비자(태스크) 바이트 링버퍼.
 * UART idle-line DMA 수신 데이터를 태스크가 안전하게 꺼내가기 위해 사용한다.
 * Push는 인터럽트 컨텍스트에서, Pop/Peek은 태스크 컨텍스트에서만 호출한다는 전제이므로
 * 별도 락 없이 head/tail 인덱스만으로 동작한다(SPSC).
 */
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t  *buf;
    uint32_t  capacity;
    volatile uint32_t head; /* 다음 write 위치 (producer만 갱신) */
    volatile uint32_t tail; /* 다음 read 위치 (consumer만 갱신) */
} RingBuffer_t;

void     RingBuffer_Init(RingBuffer_t *rb, uint8_t *storage, uint32_t capacity);
uint32_t RingBuffer_Free(const RingBuffer_t *rb);
uint32_t RingBuffer_Count(const RingBuffer_t *rb);

/* producer(ISR)용: len바이트를 밀어넣는다. 공간 부족 시 오래된 데이터를 덮어쓰지 않고
 * 넘치는 만큼 버린다(반환값 < len이면 일부 유실). */
uint32_t RingBuffer_Write(RingBuffer_t *rb, const uint8_t *data, uint32_t len);

/* consumer(task)용: 최대 len바이트를 꺼낸다. 실제로 꺼낸 바이트 수를 반환. */
uint32_t RingBuffer_Read(RingBuffer_t *rb, uint8_t *out, uint32_t len);

/* consumer용: 소비하지 않고 delim이 나오는 첫 위치까지의 길이(delim 포함)를 반환,
 * 없으면 0을 반환. 라인 단위(예: '\n') 프로토콜 파싱에 사용. */
uint32_t RingBuffer_FindByte(const RingBuffer_t *rb, uint8_t delim);

/* consumer용: delim까지의 한 줄을 out에 NUL 종단하여 복사(성공 시 true, 없으면 false).
 * out_size 초과분은 버림(길이만 소비하고 뒷부분은 잘라냄). */
bool RingBuffer_PopLine(RingBuffer_t *rb, char *out, uint32_t out_size, uint8_t delim);

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H */
