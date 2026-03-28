/**
 * @file    uart_protocol.h
 * @brief   PC <-> MCU ASCII 통신 프로토콜 파서 및 포매터
 *
 * === 명령 형식 ===
 *   모든 명령은 '\n' 또는 '\r\n' 종료
 *
 *   SET <ch> <mV>     : 전압 설정
 *   GET <ch>          : 단일 채널 조회
 *   GET ALL           : 전체 채널 조회
 *   EN <ch>           : 채널 활성화
 *   DIS <ch>          : 채널 비활성화
 *   KP <ch> <val>     : Kp 설정
 *   KI <ch> <val>     : Ki 설정
 *   KD <ch> <val>     : Kd 설정
 *   ILIM <ch> <mA>    : 전류 제한 설정
 *   HELP              : 도움말
 *
 * === 응답 형식 ===
 *   OK
 *   ERR:<reason>
 *   DATA:<ch>,V=<mv>,I=<ma>,ST=<status>
 *   ALERT:<ch>,<OC|OV|OPEN>
 *   HELP:<...>
 */

#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "channel_ctrl.h"

/* =========================================================
 * 버퍼 크기
 * ========================================================= */
#define PROTO_RX_BUF_SIZE   128U    /* 수신 라인 버퍼 */
#define PROTO_TX_BUF_SIZE   256U    /* 전송 포맷 버퍼 */

/* =========================================================
 * 함수 프로토타입
 * ========================================================= */

/**
 * @brief 프로토콜 모듈 초기화 (UART IT 수신 시작)
 */
void UartProto_Init(void);

/**
 * @brief 수신 바이트 처리 (UART RX 인터럽트에서 호출)
 *   - 줄 버퍼에 축적
 *   - '\n' 수신 시 명령 큐에 파싱 결과 전송
 * @param byte  수신 바이트
 */
void UartProto_RxByte(uint8_t byte);

/**
 * @brief 수신 라인 파싱 (내부용, 테스트 시 직접 호출 가능)
 * @param line  파싱할 문자열
 * @param cmd   파싱된 명령 구조체 출력
 * @return  0=성공, -1=파싱 실패
 */
int8_t UartProto_ParseLine(const char *line, Command_t *cmd);

/**
 * @brief "OK\r\n" 전송
 */
void UartProto_SendOk(void);

/**
 * @brief "ERR:<reason>\r\n" 전송
 */
void UartProto_SendError(const char *reason);

/**
 * @brief "DATA:<ch>,V=<mv>,I=<ma>,ST=<status>\r\n" 전송
 * @param ch_idx  채널 인덱스 (0~3)
 */
void UartProto_SendChannelData(uint8_t ch_idx);

/**
 * @brief 전체 채널 데이터 전송
 */
void UartProto_SendAllData(void);

/**
 * @brief "ALERT:<ch>,<type>\r\n" 전송
 * @param ch_idx     채널 인덱스 (0~3)
 * @param alert_str  경보 문자열 ("OC", "OV", "OPEN")
 */
void UartProto_SendAlert(uint8_t ch_idx, const char *alert_str);

/**
 * @brief 도움말 전송
 */
void UartProto_SendHelp(void);

/**
 * @brief 임의 문자열 전송 (TX 큐 경유)
 * @param str  전송할 문자열 (NULL 종료)
 */
void UartProto_SendStr(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* UART_PROTOCOL_H */
