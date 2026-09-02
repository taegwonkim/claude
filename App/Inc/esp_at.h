/**
  ******************************************************************************
  * @file    esp_at.h
  * @brief   ESP32-C WROOM (ESP-AT) 저수준 AT 명령 계층 — USART1
  ******************************************************************************
  */
#ifndef ESP_AT_H
#define ESP_AT_H

#include "app_types.h"

/* URC 로 갱신되는 모듈 상태 */
typedef struct
{
  volatile bool wifi_up;      /* WIFI CONNECTED / GOT IP */
  volatile bool tcp_up;       /* CONNECT ~ CLOSED        */
  volatile bool ready;        /* 모듈 리부팅 알림        */
} esp_state_t;

extern esp_state_t g_esp;

/* 하드웨어 리셋 (ESP_EN 토글) 후 "ready" 대기 */
sd_res_t esp_hw_reset(uint32_t timeout_ms);

/* AT 세션이 유휴일 때만 한 줄을 읽어 URC 를 반영한다 (비침습 폴링) */
void     esp_poll_urc(uint32_t timeout_ms);

/* 한 줄 수신 + URC 자동 처리. 반환: 길이(>=0) 또는 SD_ERR_TMO
 * 주의: 이 함수는 AT 뮤텍스를 잡지 않는다. 외부에서는 esp_poll_urc() 를 쓸 것. */
int32_t  esp_read_line(char *dst, uint16_t dst_sz, uint32_t timeout_ms);

/* cmd 를 보내고 expect 문자열을 포함한 줄이 올 때까지 대기.
 * expect 가 NULL 이면 "OK" 를 기다린다. "ERROR"/"FAIL" 수신 시 즉시 SD_ERR. */
sd_res_t esp_cmd(const char *cmd, const char *expect, uint32_t timeout_ms);

/* esp_cmd 와 동일하되 마지막 응답 줄을 resp 에 담는다 (진단용) */
sd_res_t esp_cmd_resp(const char *cmd, char *resp, uint16_t resp_sz, uint32_t timeout_ms);

/* AT+CIPSEND=<len> → '>' 프롬프트 → payload → "SEND OK" */
sd_res_t esp_tcp_send(const uint8_t *data, uint16_t len, uint32_t timeout_ms);

#endif /* ESP_AT_H */
