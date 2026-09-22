/**
  ******************************************************************************
  * @file    esp32_at.h
  * @brief   ESP32-C3 (ESP-AT 펌웨어) AT 커맨드 드라이버 ― 폴링 방식
  *
  *  - 인터럽트/DMA 를 쓰지 않는다. ESP_Poll() 이 UART 수신 레지스터를 직접
  *    읽어 한 줄씩 모으고, URC(WIFI DISCONNECT, CLOSED, +IPD ...) 를 이벤트
  *    비트로 올린다.
  *  - ESP_Cmd() 는 명령을 보낸 뒤 OK / ERROR / FAIL 이 올 때까지 ESP_Poll()
  *    을 돌리며 기다린다 (그동안 도착하는 URC 도 놓치지 않는다).
  *  - 메인 루프에서는 ESP_Poll() 을 가능한 자주 불러 줘야 한다.
  *    USART1 의 8바이트 RX FIFO 를 켜 두므로 115200bps 기준 약 0.7ms 까지는
  *    루프가 늦어도 오버런이 나지 않는다.
  ******************************************************************************
  */
#ifndef __ESP32_AT_H
#define __ESP32_AT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* 명령 실행 결과 */
typedef enum
{
  ESP_OK = 0,     /* OK 수신                                        */
  ESP_ERROR,      /* ERROR / FAIL / SEND FAIL 수신                  */
  ESP_TIMEOUT,    /* 제한 시간 안에 응답 없음 (모듈 죽음/배선 문제)  */
} ESP_Result_t;

/* URC(비동기 알림) 이벤트 비트 ― ESP_TakeEvents() 로 가져간다 */
#define ESP_EVT_READY             (1UL << 0)  /* "ready"          : 모듈 (재)부팅 */
#define ESP_EVT_WIFI_CONNECTED    (1UL << 1)  /* "WIFI CONNECTED"                 */
#define ESP_EVT_WIFI_GOT_IP       (1UL << 2)  /* "WIFI GOT IP"                    */
#define ESP_EVT_WIFI_DISCONNECT   (1UL << 3)  /* "WIFI DISCONNECT" : AP 끊김       */
#define ESP_EVT_TCP_CONNECT       (1UL << 4)  /* "CONNECT"         : 서버 연결됨   */
#define ESP_EVT_TCP_CLOSED        (1UL << 5)  /* "CLOSED"          : 서버 끊김     */
#define ESP_EVT_DATA_RECEIVED     (1UL << 6)  /* "+IPD"            : 데이터 도착   */

extern UART_HandleTypeDef huart_esp;

/* ---- 초기화 / 리셋 --------------------------------------------------------- */
void         ESP_Init(void);                 /* USART1 초기화 + 버퍼 클리어       */
void         ESP_HardReset(void);            /* EN 핀으로 하드웨어 리셋 (ready 대기) */

/* ---- 폴링 ------------------------------------------------------------------ */
void         ESP_Poll(void);                 /* UART 수신 처리. 자주 호출할 것     */
void         ESP_DelayPoll(uint32_t ms);     /* HAL_Delay 대신 (기다리면서 폴링)   */

/* ---- 명령 ------------------------------------------------------------------ */
ESP_Result_t ESP_Cmd(const char *cmd, uint32_t timeout_ms);
ESP_Result_t ESP_CmdFmt(uint32_t timeout_ms, const char *fmt, ...);
/* 특정 줄(예: "ready") 이 올 때까지 대기. 앞부분 일치로 비교한다 */
bool         ESP_WaitFor(const char *token, uint32_t timeout_ms);

/* 직전 명령의 응답 전체 (여러 줄, '\n' 구분) */
const char  *ESP_Response(void);
bool         ESP_ResponseContains(const char *key);
/* 응답에서  key"값"  형태의 따옴표 문자열 추출. 예) key = "+CIPSTAMAC:" */
bool         ESP_GetQuoted(const char *key, char *out, size_t out_len);
/* 응답에서  key<정수>  추출. 예) key = "STATUS:" */
bool         ESP_GetInt(const char *key, int32_t *out);

/* ---- 이벤트 ---------------------------------------------------------------- */
uint32_t     ESP_TakeEvents(void);           /* 쌓인 이벤트 비트 반환 + 클리어     */
uint32_t     ESP_PeekEvents(void);           /* 클리어 없이 확인                   */

/* ---- TCP 데이터 ------------------------------------------------------------ */
ESP_Result_t ESP_SendData(const uint8_t *data, uint16_t len);   /* AT+CIPSEND   */
uint16_t     ESP_DataAvailable(void);
uint16_t     ESP_DataRead(uint8_t *buf, uint16_t max_len);

/* ---- 진단 ------------------------------------------------------------------ */
uint32_t     ESP_OverrunCount(void);         /* UART 오버런 횟수 (0 이어야 정상)   */

#ifdef __cplusplus
}
#endif

#endif /* __ESP32_AT_H */
