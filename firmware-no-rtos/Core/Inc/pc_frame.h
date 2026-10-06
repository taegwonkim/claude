/**
 * pc_frame.h
 *
 * PC ↔ MCU(USART3/USB CDC 전용) 프레임 포맷: STX(0x02) + Data1,Data2,...,DataN + CR(0x0D) + LF(0x0A).
 * 필드는 콤마(',')로 구분한다. ESP32 AT(USART1)나 FPGA 링크(USART2), ESP32 경유 TCP 서버 전송
 * 페이로드에는 적용하지 않는다 - PC 시리얼 링크 전용 프레이밍이다 (docs/프로토콜_명세.md §1 참고).
 *
 * 프레임의 종단(CR LF)은 uart_line_rx.c의 idle-line + '\n' 기준 라인 추출로 이미 처리되므로,
 * 이 모듈은 팝업된 한 줄(트레일링 '\r'까지 제거된 상태)에서 선두 STX 확인과 콤마 분리만 담당한다.
 */
#ifndef PC_FRAME_H
#define PC_FRAME_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PC_FRAME_STX          ((char)0x02)
#define PC_FRAME_MAX_FIELDS   (16U)

/**
 * @brief raw_line(팝업된 한 줄, 이미 트레일링 '\r'/'\n' 제거됨)의 선두 STX를 확인하고
 *        나머지를 ','로 분리해 out_fields[]에 채운다. raw_line 버퍼 자체를 파괴적으로
 *        수정한다(strtok 방식 - 각 필드는 raw_line 내부 메모리를 가리킴, 별도 복사 없음).
 * @return 필드 개수(0 이상). raw_line이 STX로 시작하지 않으면 -1(잡음/깨진 프레임).
 */
int PcFrame_Parse(char *raw_line, char *out_fields[], size_t max_fields);

/**
 * @brief csv_payload(콤마로 이미 join된 필드 문자열, 예: "OK" 또는 "ERR,INVALID_IP")를
 *        STX + csv_payload + CR + LF로 감싸 out에 채운다(NUL 종단 포함).
 * @return 기록된 바이트 수(NUL 제외). out_size가 부족하면 payload를 잘라서라도 프레임을 완성한다.
 */
int PcFrame_Wrap(char *out, size_t out_size, const char *csv_payload);

#ifdef __cplusplus
}
#endif

#endif /* PC_FRAME_H */
