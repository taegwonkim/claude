/**
 * @file    packet_parser.c
 * @brief   STX/ETX 패킷 파서 구현
 *          방법 1: DLE Byte Stuffing
 *          방법 2: Length-Prefixed Frame
 *
 * PARSER_MODE (packet_parser.h) 로 컴파일 타임에 선택합니다.
 */

#include "packet_parser.h"
#include <string.h>

/* ────────────────────────────────────────────────────────────
 *  모듈 내부 변수
 * ──────────────────────────────────────────────────────────── */
static osMessageQueueId_t s_packet_queue = NULL;

/* ────────────────────────────────────────────────────────────
 *  내부 헬퍼: 완성된 패킷을 Queue 로 전송
 * ──────────────────────────────────────────────────────────── */
static void emit_packet(ParserCtx_t *ctx)
{
    Packet_t pkt;
    pkt.len = ctx->pkt_idx;
    (void)memcpy(pkt.data, ctx->pkt_buf, ctx->pkt_idx);
    (void)osMessageQueuePut(ctx->out_queue, &pkt, 0U, 0U);

    ctx->pkt_idx = 0U;
    ctx->state   = PARSER_IDLE;
}

/* ════════════════════════════════════════════════════════════
 *  방법 1: DLE Byte Stuffing 파서
 *
 *  프레임 형식:
 *    STX | [DLE + (b^0x20)] or [b] ... | ETX
 *
 *  상태 전이:
 *    IDLE        ──STX──────────────────▶ IN_PACKET
 *    IN_PACKET   ──ETX──────────────────▶ IDLE (패킷 완성)
 *    IN_PACKET   ──DLE──────────────────▶ DLE_ESCAPE
 *    DLE_ESCAPE  ──any b──(b^0x20 저장)──▶ IN_PACKET
 *    IN_PACKET   ──STX──────────────────▶ IN_PACKET (오류 복구, 리셋)
 * ════════════════════════════════════════════════════════════ */
#if (PARSER_MODE == PARSER_MODE_DLE_STUFFING)

static void feed_dle(ParserCtx_t *ctx, uint8_t byte)
{
    switch (ctx->state) {

    case PARSER_IDLE:
        if (byte == STX) {
            ctx->pkt_idx = 0U;
            ctx->state   = PARSER_IN_PACKET;
        }
        break;

    case PARSER_IN_PACKET:
        if (byte == ETX) {
            /* ── 패킷 완성 ── */
            emit_packet(ctx);

        } else if (byte == DLE) {
            /* ── 이스케이프 시퀀스 시작 ── */
            ctx->state = PARSER_DLE_ESCAPE;

        } else if (byte == STX) {
            /* ── 비정상 STX: 이전 패킷 버리고 재시작 ── */
            ctx->pkt_idx = 0U;
            ctx->error_count++;
            /* state 는 PARSER_IN_PACKET 유지 (새 패킷 시작) */

        } else {
            /* ── 일반 데이터 ── */
            if (ctx->pkt_idx < MAX_PACKET_DATA_LEN) {
                ctx->pkt_buf[ctx->pkt_idx++] = byte;
            } else {
                ctx->pkt_idx = 0U;
                ctx->state   = PARSER_IDLE;
                ctx->error_count++;
            }
        }
        break;

    case PARSER_DLE_ESCAPE:
        /*
         * DLE 다음 바이트를 XOR 0x20 으로 복원하여 저장.
         *   DLE + 0x22 → 0x22 ^ 0x20 = 0x02 (STX)
         *   DLE + 0x23 → 0x23 ^ 0x20 = 0x03 (ETX)
         *   DLE + 0x30 → 0x30 ^ 0x20 = 0x10 (DLE)
         */
        if (ctx->pkt_idx < MAX_PACKET_DATA_LEN) {
            ctx->pkt_buf[ctx->pkt_idx++] = byte ^ 0x20U;
            ctx->state = PARSER_IN_PACKET;
        } else {
            ctx->pkt_idx = 0U;
            ctx->state   = PARSER_IDLE;
            ctx->error_count++;
        }
        break;

    default:
        ctx->state = PARSER_IDLE;
        break;
    }
}

void parser_feed(ParserCtx_t *ctx, const uint8_t *buf, uint16_t len)
{
    if (ctx == NULL || buf == NULL || len == 0U) { return; }
    for (uint16_t i = 0U; i < len; i++) {
        feed_dle(ctx, buf[i]);
    }
}

/* ════════════════════════════════════════════════════════════
 *  방법 2: Length-Prefixed Frame 파서
 *
 *  프레임 형식:
 *    STX | LEN_H | LEN_L | DATA[LEN] | CRC8 | ETX
 *
 *  CRC8 다항식: 0x07 (CRC-8/SMBUS, 가장 단순)
 *
 *  LEN 을 먼저 파악하므로 DATA 안에 STX/ETX 가 있어도 무관합니다.
 *  수신 바이트 수를 카운트하여 정확히 LEN 만큼만 DATA 로 처리합니다.
 * ════════════════════════════════════════════════════════════ */
#elif (PARSER_MODE == PARSER_MODE_LENGTH_PREFIX)

static void feed_len_prefix(ParserCtx_t *ctx, uint8_t byte)
{
    switch (ctx->state) {

    case PARSER_IDLE:
        if (byte == STX) {
            ctx->pkt_idx      = 0U;
            ctx->expected_len = 0U;
            ctx->crc_accum    = 0U;
            ctx->state        = PARSER_GET_LEN_H;
        }
        break;

    case PARSER_GET_LEN_H:
        ctx->expected_len = (uint16_t)byte << 8;
        ctx->state        = PARSER_GET_LEN_L;
        break;

    case PARSER_GET_LEN_L:
        ctx->expected_len |= byte;
        if (ctx->expected_len == 0U ||
            ctx->expected_len > MAX_PACKET_DATA_LEN) {
            /* 길이 오류: IDLE 복귀 */
            ctx->state = PARSER_IDLE;
            ctx->error_count++;
        } else {
            ctx->state = PARSER_GET_DATA;
        }
        break;

    case PARSER_GET_DATA:
        /* DATA 바이트는 STX/ETX 값이어도 그냥 저장 */
        ctx->pkt_buf[ctx->pkt_idx++] = byte;
        ctx->crc_accum ^= byte;   /* CRC-8 누산 (XOR 방식) */

        if (ctx->pkt_idx >= ctx->expected_len) {
            ctx->state = PARSER_GET_CRC;
        }
        break;

    case PARSER_GET_CRC:
        if (byte != ctx->crc_accum) {
            /* CRC 불일치 → 패킷 버림 */
            ctx->state = PARSER_IDLE;
            ctx->error_count++;
        } else {
            ctx->state = PARSER_GET_ETX;
        }
        break;

    case PARSER_GET_ETX:
        if (byte == ETX) {
            emit_packet(ctx);  /* 정상 완성 */
        } else {
            ctx->state = PARSER_IDLE;
            ctx->error_count++;
        }
        break;

    default:
        ctx->state = PARSER_IDLE;
        break;
    }
}

void parser_feed(ParserCtx_t *ctx, const uint8_t *buf, uint16_t len)
{
    if (ctx == NULL || buf == NULL || len == 0U) { return; }
    for (uint16_t i = 0U; i < len; i++) {
        feed_len_prefix(ctx, buf[i]);
    }
}

#endif /* PARSER_MODE */

/* ════════════════════════════════════════════════════════════
 *  공통 API
 * ════════════════════════════════════════════════════════════ */

void parser_init(ParserCtx_t *ctx, osMessageQueueId_t out_queue)
{
    if (ctx == NULL) { return; }

    if (s_packet_queue == NULL) {
        s_packet_queue = osMessageQueueNew(PACKET_QUEUE_DEPTH,
                                           sizeof(Packet_t), NULL);
        if (s_packet_queue == NULL) { Error_Handler(); }
    }

    (void)memset(ctx, 0, sizeof(ParserCtx_t));
    ctx->state     = PARSER_IDLE;
    ctx->out_queue = (out_queue != NULL) ? out_queue : s_packet_queue;
}

osMessageQueueId_t parser_get_packet_queue(void)
{
    return s_packet_queue;
}

/* ════════════════════════════════════════════════════════════
 *  송신 측 DLE 인코딩 유틸리티 (방법 1)
 *
 *  STM32 또는 PC(호스트)에서 패킷을 만들 때 사용합니다.
 *
 *  사용 예:
 *    uint8_t raw[]  = {0x01, 0x02, 0xAB};   // 0x02 = STX 값!
 *    uint8_t frame[32];
 *    uint16_t flen = dle_encode_frame(raw, 3, frame, sizeof(frame));
 *    HAL_UART_Transmit(&huart1, frame, flen, 100);
 *
 *  결과 (hex):
 *    02 | 01 | 10 22 | AB | 03
 *    STX  data  DLE+encoded  data  ETX
 * ════════════════════════════════════════════════════════════ */
uint16_t dle_encode_frame(const uint8_t *src, uint16_t src_len,
                          uint8_t *dst, uint16_t dst_size)
{
    if (src == NULL || dst == NULL) { return 0U; }

    uint16_t di = 0U;

    /* STX */
    if (di < dst_size) { dst[di++] = STX; }

    for (uint16_t i = 0U; i < src_len; i++) {
        uint8_t b = src[i];
        if (b == STX || b == ETX || b == DLE) {
            /* 이스케이프 처리 */
            if (di + 1U < dst_size) {
                dst[di++] = DLE;
                dst[di++] = b ^ 0x20U;
            }
        } else {
            if (di < dst_size) { dst[di++] = b; }
        }
    }

    /* ETX */
    if (di < dst_size) { dst[di++] = ETX; }

    return di;
}

/* ════════════════════════════════════════════════════════════
 *  CRC-8 계산 (방법 2, 다항식 0x07: CRC-8/SMBUS)
 *
 *  사용 예 (송신 측):
 *    uint8_t data[]  = {0x01, 0x02, 0x03};
 *    uint8_t crc     = crc8_calc(data, 3);
 *    // 프레임: STX | 0x00 | 0x03 | 0x01 | 0x02 | 0x03 | crc | ETX
 * ════════════════════════════════════════════════════════════ */
uint8_t crc8_calc(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00U;
    for (uint16_t i = 0U; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0U; bit < 8U; bit++) {
            if (crc & 0x80U) {
                crc = (uint8_t)((crc << 1) ^ 0x07U);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
