/**
 * @file    packet_parser.h
 * @brief   STX + data + ETX 패킷 파서 (상태 머신)
 *
 * 단일 프레임에 여러 패킷이 있거나, 패킷이 두 프레임에 걸쳐
 * 나뉘는 경우(split packet)를 모두 처리합니다.
 *
 * 상태 머신:
 *   IDLE ──STX──→ IN_PACKET ──ETX──→ IDLE
 *                    │
 *              (data byte 누적)
 *              (버퍼 초과 시 → IDLE 로 리셋)
 */

#ifndef __PACKET_PARSER_H
#define __PACKET_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usart_dma.h"
#include <stdint.h>
#include <stdbool.h>

/* ────────────────────────────────────────────────────────────
 *  파서 Queue
 * ──────────────────────────────────────────────────────────── */

/** 파서 → 애플리케이션 태스크 간 Queue 깊이 */
#define PACKET_QUEUE_DEPTH  8U

/* ────────────────────────────────────────────────────────────
 *  파서 상태
 * ──────────────────────────────────────────────────────────── */
typedef enum {
    PARSER_IDLE,        /**< STX 대기 중            */
    PARSER_IN_PACKET,   /**< STX 수신 후 data 수집 중 */
} ParserState_t;

/**
 * @brief 파서 컨텍스트 (태스크마다 독립적으로 유지)
 *
 * parser_init() 으로 초기화한 뒤 parser_feed() 에 전달합니다.
 */
typedef struct {
    ParserState_t state;
    uint8_t       pkt_buf[MAX_PACKET_DATA_LEN];
    uint16_t      pkt_idx;
    osMessageQueueId_t out_queue; /**< 완성된 패킷을 보낼 Queue */
    uint32_t      error_count;    /**< 버퍼 초과 등 오류 횟수   */
} ParserCtx_t;

/* ────────────────────────────────────────────────────────────
 *  공개 API
 * ──────────────────────────────────────────────────────────── */

/**
 * @brief 파서 컨텍스트 및 출력 Queue 초기화
 * @param ctx       초기화할 파서 컨텍스트
 * @param out_queue 완성 패킷을 전달할 Queue (Packet_t 타입)
 */
void parser_init(ParserCtx_t *ctx, osMessageQueueId_t out_queue);

/**
 * @brief 원시 바이트 배열을 파서에 공급
 *
 * 내부적으로 상태 머신을 구동하며, ETX 를 만날 때마다
 * out_queue 에 Packet_t 를 전송합니다.
 *
 * @param ctx  파서 컨텍스트
 * @param buf  바이트 배열
 * @param len  바이트 수
 */
void parser_feed(ParserCtx_t *ctx, const uint8_t *buf, uint16_t len);

/**
 * @brief 파서 → 애플리케이션 Queue 핸들 반환
 */
osMessageQueueId_t parser_get_packet_queue(void);

#ifdef __cplusplus
}
#endif

#endif /* __PACKET_PARSER_H */
