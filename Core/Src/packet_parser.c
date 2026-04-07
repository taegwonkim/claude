/**
 * @file    packet_parser.c
 * @brief   STX + data + ETX 패킷 파서 구현 (상태 머신)
 *
 * ┌──────────────────────────────────────────────────────────┐
 * │  상태 머신 다이어그램                                      │
 * │                                                          │
 * │   ┌─────────┐  byte==STX   ┌───────────┐                │
 * │   │  IDLE   │─────────────▶│ IN_PACKET │                │
 * │   └─────────┘              └─────┬─────┘                │
 * │        ▲                         │                      │
 * │        │   byte==ETX             │  byte==data          │
 * │        └─────────────────────────┘  (pkt_buf 에 누적)    │
 * │        │                                                 │
 * │        │   버퍼 초과 (error)                              │
 * │        └─────────────────── IDLE 로 강제 리셋             │
 * │                                                          │
 * │  ※ 단일 프레임 안에 여러 패킷 허용                         │
 * │  ※ 패킷이 두 프레임에 걸쳐 나뉘는 경우(split) 자동 처리    │
 * └──────────────────────────────────────────────────────────┘
 */

#include "packet_parser.h"
#include <string.h>

/* ────────────────────────────────────────────────────────────
 *  모듈 내부 변수
 * ──────────────────────────────────────────────────────────── */

/** 파서 → 애플리케이션 태스크 Queue (모듈 전역) */
static osMessageQueueId_t s_packet_queue = NULL;

/* ────────────────────────────────────────────────────────────
 *  공개 API 구현
 * ──────────────────────────────────────────────────────────── */

void parser_init(ParserCtx_t *ctx, osMessageQueueId_t out_queue)
{
    if (ctx == NULL) {
        return;
    }

    /* Queue가 아직 생성되지 않은 경우에만 생성 */
    if (s_packet_queue == NULL) {
        s_packet_queue = osMessageQueueNew(PACKET_QUEUE_DEPTH,
                                           sizeof(Packet_t),
                                           NULL);
        if (s_packet_queue == NULL) {
            Error_Handler();
        }
    }

    ctx->state       = PARSER_IDLE;
    ctx->pkt_idx     = 0U;
    ctx->error_count = 0U;
    ctx->out_queue   = (out_queue != NULL) ? out_queue : s_packet_queue;

    (void)memset(ctx->pkt_buf, 0, sizeof(ctx->pkt_buf));
}

void parser_feed(ParserCtx_t *ctx, const uint8_t *buf, uint16_t len)
{
    if (ctx == NULL || buf == NULL || len == 0U) {
        return;
    }

    for (uint16_t i = 0U; i < len; i++) {
        uint8_t byte = buf[i];

        switch (ctx->state) {

        /* ── IDLE: STX를 기다리는 상태 ── */
        case PARSER_IDLE:
            if (byte == STX) {
                ctx->pkt_idx = 0U;
                ctx->state   = PARSER_IN_PACKET;
            }
            /* STX가 아닌 바이트는 무시 */
            break;

        /* ── IN_PACKET: data 바이트를 누적하는 상태 ── */
        case PARSER_IN_PACKET:
            if (byte == ETX) {
                /* 패킷 완성 → Queue에 전송 */
                Packet_t pkt;
                pkt.len = ctx->pkt_idx;
                (void)memcpy(pkt.data, ctx->pkt_buf, ctx->pkt_idx);

                /*
                 * 태스크 컨텍스트이므로 일반 osMessageQueuePut 사용.
                 * timeout = 0: Queue 가득 찼으면 패킷 드롭 (실시간 우선)
                 * 필요 시 osWaitForever 로 변경 가능.
                 */
                (void)osMessageQueuePut(ctx->out_queue, &pkt, 0U, 0U);

                /* 다음 패킷을 위해 상태 초기화 */
                ctx->pkt_idx = 0U;
                ctx->state   = PARSER_IDLE;

            } else if (byte == STX) {
                /*
                 * 패킷 중간에 STX 재발견 → 이전 데이터 버리고 새 패킷 시작
                 * (프로토콜 오류 복구)
                 */
                ctx->pkt_idx = 0U;
                ctx->error_count++;

            } else {
                /* 일반 데이터 바이트 누적 */
                if (ctx->pkt_idx < MAX_PACKET_DATA_LEN) {
                    ctx->pkt_buf[ctx->pkt_idx++] = byte;
                } else {
                    /* 버퍼 초과 → 패킷 버리고 IDLE 복귀 */
                    ctx->pkt_idx = 0U;
                    ctx->state   = PARSER_IDLE;
                    ctx->error_count++;
                }
            }
            break;

        default:
            ctx->state = PARSER_IDLE;
            break;
        }
    }
}

osMessageQueueId_t parser_get_packet_queue(void)
{
    return s_packet_queue;
}
