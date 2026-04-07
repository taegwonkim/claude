/**
 * @file    packet_parser.h
 * @brief   STX/ETX 패킷 파서 - 데이터 내 STX/ETX 처리 포함
 *
 * ═══════════════════════════════════════════════════════════════
 *  문제: 데이터 바이트가 STX(0x02) 또는 ETX(0x03) 값일 때
 *        파서가 오동작함.
 *
 *  해결책 비교
 * ───────────────────────────────────────────────────────────────
 *  방법 1: DLE Byte Stuffing  (이 파일의 기본 구현)
 * ───────────────────────────────────────────────────────────────
 *  DLE(0x10) = Data Link Escape 문자를 이스케이프 신호로 사용.
 *
 *  [송신 측 인코딩 규칙]
 *    일반 데이터 바이트 b 에 대해:
 *      b == STX → 전송: DLE + (STX ^ 0x20) = DLE + 0x22
 *      b == ETX → 전송: DLE + (ETX ^ 0x20) = DLE + 0x23
 *      b == DLE → 전송: DLE + (DLE ^ 0x20) = DLE + 0x30
 *      그 외    → 전송: b  (변경 없음)
 *
 *  [수신 측 디코딩 규칙]
 *    DLE 수신 시 → 다음 바이트 b' 를 (b' ^ 0x20) 으로 복원
 *
 *  [프레임 형식]
 *    STX | encoded_data... | ETX
 *    (STX, ETX, DLE 는 프레임 제어 문자로만 사용, 데이터는 인코딩)
 *
 *  [장점]  오버헤드 최소 (특수 문자 등장 시에만 1바이트 추가)
 *  [단점]  송신 측도 인코딩 구현 필요, 최악 2배 크기
 *
 * ───────────────────────────────────────────────────────────────
 *  방법 2: Length-Prefixed Frame  (대안, 하단 참조)
 * ───────────────────────────────────────────────────────────────
 *  [프레임 형식]
 *    STX | LEN_H | LEN_L | DATA[LEN] | CRC8 | ETX
 *
 *    LEN      : 데이터 바이트 수 (빅엔디언 2바이트)
 *    DATA[LEN]: 인코딩 없이 원시 데이터 (STX/ETX 가 있어도 무관)
 *    CRC8     : DATA 에 대한 체크섬
 *    ETX      : 프레임 끝 (확인용)
 *
 *  동작 원리:
 *    LEN 을 먼저 읽어 정확히 LEN 바이트만 DATA 로 취급하므로
 *    DATA 안에 STX/ETX/DLE 가 있어도 완전히 무시합니다.
 *
 *  [장점]  송신 인코딩 불필요, CRC 로 무결성 검증 가능
 *  [단점]  항상 헤더(LEN 2바이트 + CRC 1바이트) 오버헤드 존재
 *
 *  방법 2 사용 시: packet_parser.h 의 PARSER_MODE 를
 *                  PARSER_MODE_LENGTH_PREFIX 로 변경하십시오.
 * ═══════════════════════════════════════════════════════════════
 *
 *  상태 머신 (방법 1: DLE Stuffing)
 *
 *   ┌─────────┐  STX    ┌───────────┐  DLE    ┌────────────┐
 *   │  IDLE   │────────▶│ IN_PACKET │────────▶│ DLE_ESCAPE │
 *   └─────────┘         └─────┬─────┘◀────────┴────────────┘
 *        ▲                    │ ETX          (b'^0x20 저장)
 *        └────────────────────┘
 *
 *  상태 머신 (방법 2: Length-Prefix)
 *
 *   ┌──────┐ STX ┌──────────┐  LEN  ┌──────────┐  DATA  ┌──────────┐  CRC  ┌──────────┐ ETX ┌──────┐
 *   │ IDLE │────▶│ GET_LENH │──────▶│ GET_LENL │───────▶│ GET_DATA │──────▶│ GET_CRC  │────▶│ IDLE │
 *   └──────┘     └──────────┘       └──────────┘        └──────────┘       └──────────┘     └──────┘
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
 *  파서 모드 선택
 *  하나만 활성화하십시오.
 * ──────────────────────────────────────────────────────────── */
#define PARSER_MODE_DLE_STUFFING     0   /**< 방법 1: DLE Byte Stuffing  */
#define PARSER_MODE_LENGTH_PREFIX    1   /**< 방법 2: Length-Prefixed    */

/** 사용할 파서 모드 */
#define PARSER_MODE    PARSER_MODE_DLE_STUFFING

/* ────────────────────────────────────────────────────────────
 *  프로토콜 제어 문자
 * ──────────────────────────────────────────────────────────── */
#define DLE     0x10U   /**< Data Link Escape (방법 1 전용) */

/* ────────────────────────────────────────────────────────────
 *  Queue 깊이
 * ──────────────────────────────────────────────────────────── */
#define PACKET_QUEUE_DEPTH  8U

/* ────────────────────────────────────────────────────────────
 *  파서 상태 (방법 1: DLE Stuffing)
 * ──────────────────────────────────────────────────────────── */
typedef enum {
    /* 공통 */
    PARSER_IDLE,            /**< STX 대기                          */

    /* 방법 1 전용 */
    PARSER_IN_PACKET,       /**< 데이터 누적 중                    */
    PARSER_DLE_ESCAPE,      /**< DLE 수신 후 다음 바이트 대기       */

    /* 방법 2 전용 */
    PARSER_GET_LEN_H,       /**< LEN 상위 바이트 대기               */
    PARSER_GET_LEN_L,       /**< LEN 하위 바이트 대기               */
    PARSER_GET_DATA,        /**< DATA 수신 중 (LEN 바이트)          */
    PARSER_GET_CRC,         /**< CRC 바이트 대기                    */
    PARSER_GET_ETX,         /**< ETX 최종 확인 대기                 */
} ParserState_t;

/**
 * @brief 파서 컨텍스트
 */
typedef struct {
    ParserState_t      state;
    uint8_t            pkt_buf[MAX_PACKET_DATA_LEN];
    uint16_t           pkt_idx;

    /* 방법 2 전용 필드 */
    uint16_t           expected_len;    /**< 수신할 DATA 바이트 수  */
    uint8_t            crc_accum;       /**< CRC 누산 값            */

    osMessageQueueId_t out_queue;
    uint32_t           error_count;
} ParserCtx_t;

/* ────────────────────────────────────────────────────────────
 *  공개 API
 * ──────────────────────────────────────────────────────────── */

void               parser_init(ParserCtx_t *ctx, osMessageQueueId_t out_queue);
void               parser_feed(ParserCtx_t *ctx, const uint8_t *buf, uint16_t len);
osMessageQueueId_t parser_get_packet_queue(void);

/* ────────────────────────────────────────────────────────────
 *  송신 측 인코딩 유틸리티 (방법 1 전용)
 *
 *  PC(호스트) 측에서 사용하는 인코딩 함수입니다.
 *  STM32 에서 응답 패킷을 만들 때도 활용할 수 있습니다.
 *
 *  @param  src      원본 데이터
 *  @param  src_len  원본 길이
 *  @param  dst      인코딩 후 저장할 버퍼 (최소 src_len*2+2 바이트)
 *  @return 인코딩된 총 바이트 수 (STX/ETX 포함)
 * ──────────────────────────────────────────────────────────── */
uint16_t dle_encode_frame(const uint8_t *src, uint16_t src_len,
                          uint8_t *dst, uint16_t dst_size);

/* ────────────────────────────────────────────────────────────
 *  CRC8 유틸리티 (방법 2 전용)
 * ──────────────────────────────────────────────────────────── */
uint8_t crc8_calc(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __PACKET_PARSER_H */
