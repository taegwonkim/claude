/*
 * esp32_at.h
 *
 * ESP32-C3 AT 펌웨어를 UART로 제어하여
 *  1) Wi-Fi(STA) 접속
 *  2) 서버 PC와 TCP 연결
 *  3) 데이터 송수신
 *  4) 연결 끊김 감지 및 자동 재접속(backoff)
 * 을 처리하는 논블로킹 상태 머신 드라이버.
 *
 * STM32CubeMX로 생성한 프로젝트의 Core/Inc, Core/Src 에 그대로 추가해서 사용한다.
 * UART는 인터럽트(HAL_UART_Receive_IT) 방식을 사용하며, DMA가 아니어도 동작한다.
 */

#ifndef INC_ESP32_AT_H_
#define INC_ESP32_AT_H_

#include "stm32l5xx_hal.h"   /* STM32L562RCT6(STM32L5 시리즈) 기준. 다른 시리즈면 stm32f1xx_hal.h 등으로 수정 */
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* 설정 값 : 필요에 따라 조정                                          */
/* ------------------------------------------------------------------ */
#define ESP32_RX_RING_SIZE        512   /* UART RX 링버퍼 크기 */
#define ESP32_LINE_BUF_SIZE       256   /* AT 응답 한 줄 최대 길이 */
#define ESP32_SSID_MAX            32
#define ESP32_PASS_MAX            64
#define ESP32_SERVER_IP_MAX       16
#define ESP32_MAC_STR_LEN         18    /* "xx:xx:xx:xx:xx:xx" + '\0' */

#define ESP32_CMD_TIMEOUT_MS      3000  /* 일반 AT 명령 응답 대기 시간 */
#define ESP32_WIFI_TIMEOUT_MS     20000 /* AT+CWJAP 응답 대기 시간(AP 접속은 느림) */
#define ESP32_TCP_TIMEOUT_MS      10000 /* AT+CIPSTART 응답 대기 시간 */

#define ESP32_RECONNECT_BASE_MS   2000  /* 재접속 대기 시간(backoff) 초기값 */
#define ESP32_RECONNECT_MAX_MS    60000 /* 재접속 대기 시간 최대값 */

/* ------------------------------------------------------------------ */
/* 상태 정의                                                          */
/* ------------------------------------------------------------------ */
typedef enum {
    ESP32_STATE_INIT = 0,     /* 드라이버 초기화 직후 */
    ESP32_STATE_MODULE_CHECK, /* AT, ATE0, CWMODE 등 모듈 응답 확인 */
    ESP32_STATE_GET_MAC,      /* AT+CIPSTAMAC? 로 Station MAC 주소 조회 */
    ESP32_STATE_WIFI_CONNECTING,
    ESP32_STATE_WIFI_CONNECTED,
    ESP32_STATE_TCP_CONNECTING,
    ESP32_STATE_TCP_CONNECTED, /* 정상 통신 가능 상태 */
    ESP32_STATE_LINK_DOWN,     /* 끊김 감지, 재접속 대기 시작 */
    ESP32_STATE_RECONNECT_WAIT,
    ESP32_STATE_FATAL_ERROR    /* 모듈 자체 응답 없음 등, 사용자 개입 필요 */
} ESP32_State_t;

/* 수신 데이터 콜백: 서버로부터 받은 payload 를 애플리케이션에 전달 */
typedef void (*ESP32_DataCallback_t)(const uint8_t *data, uint16_t len);

/* 상태 변화 콜백(옵션): 로그 출력, LED 표시 등에 사용 */
typedef void (*ESP32_StateCallback_t)(ESP32_State_t state);

/* ------------------------------------------------------------------ */
/* 공개 API                                                            */
/* ------------------------------------------------------------------ */

/* huart: ESP32-C3와 연결된 UART 핸들 (예: &huart1) */
void ESP32_Init(UART_HandleTypeDef *huart);

/* Wi-Fi / 서버 접속 정보 설정. Init 이후, 첫 Process 호출 전에 한 번 호출 */
void ESP32_SetWiFiCredentials(const char *ssid, const char *password);
void ESP32_SetServer(const char *ip, uint16_t port);

void ESP32_SetDataCallback(ESP32_DataCallback_t cb);
void ESP32_SetStateCallback(ESP32_StateCallback_t cb);

/* main() 의 while(1) 루프에서 계속 호출해야 하는 논블로킹 상태 머신 */
void ESP32_Process(void);

/* TCP_CONNECTED 상태에서만 실제로 전송됨. 그 외 상태면 HAL_ERROR 반환 */
HAL_StatusTypeDef ESP32_Send(const uint8_t *data, uint16_t len);

ESP32_State_t ESP32_GetState(void);

/* Station MAC 주소 문자열("aa:bb:cc:dd:ee:ff"). GET_MAC 단계를 거치기 전에는
 * 빈 문자열("")이 반환된다. 상태 콜백에서 ESP32_STATE_WIFI_CONNECTING 이후
 * 확인하면 값이 채워져 있음이 보장된다. */
const char *ESP32_GetMacAddress(void);

/* HAL_UART_RxCpltCallback() 안에서, huart가 ESP32용 핸들일 때 호출해줘야 함 */
void ESP32_UART_RxCpltCallback(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* INC_ESP32_AT_H_ */
