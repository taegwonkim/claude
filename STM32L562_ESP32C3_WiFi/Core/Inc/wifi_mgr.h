/**
  ******************************************************************************
  * @file    wifi_mgr.h
  * @brief   Wi-Fi 연결 관리 상태 머신
  *
  *   RESET ─▶ INIT ─▶ AP_CONNECT ─▶ SERVER_CONNECT ─▶ ONLINE
  *     ▲        │          │              │              │
  *     │        │          │  실패 n회    │  실패 n회    │ WIFI DISCONNECT → AP_CONNECT
  *     │        │          ▼              ▼              │ CLOSED          → SERVER_CONNECT
  *     └────────┴──────────┴──────────────┴──────────────┘ 응답 없음(timeout) → RESET
  *
  *  모든 상태 전이는 WIFI_Process() 안에서 일어나며, 재시도 대기는
  *  HAL_GetTick() 기반 논블로킹이라 대기 중에도 메인 루프가 계속 돈다.
  ******************************************************************************
  */
#ifndef __WIFI_MGR_H
#define __WIFI_MGR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum
{
  WIFI_STATE_RESET = 0,      /* ESP32 하드웨어 리셋                         */
  WIFI_STATE_INIT,           /* AT 동기화, 기본 설정, MAC 읽기               */
  WIFI_STATE_AP_CONNECT,     /* DHCP/고정 IP 설정 + AP 접속(CWJAP)           */
  WIFI_STATE_SERVER_CONNECT, /* TCP 서버 접속(CIPSTART)                     */
  WIFI_STATE_ONLINE,         /* AP + 서버 모두 연결됨                        */
  WIFI_STATE_WAIT,           /* 재시도 대기 (다음 상태는 내부에 저장)         */
} WIFI_State_t;

typedef struct
{
  char     ssid[33];
  char     password[65];
  char     server_ip[64];        /* IP 또는 도메인 이름 */
  uint16_t server_port;
  uint16_t tcp_keepalive_s;      /* 0 = 사용 안 함 */
  bool     use_dhcp;
  char     static_ip[16];
  char     gateway[16];
  char     netmask[16];
} WIFI_Config_t;

/* wifi_config.h 의 값으로 설정 구조체를 채운다 */
void          WIFI_GetDefaultConfig(WIFI_Config_t *cfg);

/* 설정을 적용하고 상태 머신을 RESET 부터 시작한다 (cfg == NULL 이면 기본값) */
void          WIFI_Init(const WIFI_Config_t *cfg);

/* 메인 루프에서 계속 호출. 내부에서 ESP_Poll() 도 호출한다 */
void          WIFI_Process(void);

/* 실행 중 설정 변경 → 적용을 위해 재접속한다 */
void          WIFI_Reconfigure(const WIFI_Config_t *cfg);

WIFI_State_t  WIFI_GetState(void);
const char   *WIFI_GetStateName(void);
bool          WIFI_IsOnline(void);
const char   *WIFI_GetMac(void);        /* "7c:df:a1:xx:xx:xx" (INIT 후 유효) */
const char   *WIFI_GetIp(void);         /* AP 접속 후 유효                     */
const char   *WIFI_GetFwVersion(void);  /* AT+GMR 첫 줄                        */

/* 서버로 송신 (ONLINE 일 때만). 실패하면 false 를 돌려주고 재접속에 들어간다 */
bool          WIFI_Send(const uint8_t *data, uint16_t len);
/* 서버에서 받은 데이터 읽기 (없으면 0) */
uint16_t      WIFI_Receive(uint8_t *buf, uint16_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* __WIFI_MGR_H */
