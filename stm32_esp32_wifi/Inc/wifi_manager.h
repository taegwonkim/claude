/*
 * wifi_manager.h
 *
 * STM32L562CET6 + ESP32-C3-WROOM(ESP-AT) 조합에서
 *   1) AP 접속 (SSID/PASSWORD)
 *   2) DHCP on/off (off인 경우 정적 IP/Gateway/Netmask)
 *   3) TCP 서버 접속 (IP/PORT)
 *   4) AP 또는 서버 접속이 끊겼을 때 자동 재접속
 * 을 처리하는 논블로킹 상태 머신.
 *
 * main 루프에서 WiFi_Manager_Process()를 주기적으로(예: 10ms 이상 간격)
 * 호출해 주기만 하면 되고, 각 단계는 내부적으로 HAL_GetTick() 기반
 * 타임아웃/재시도 백오프로 동작한다.
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32l5xx_hal.h"

typedef struct {
    char ssid[33];
    char password[65];
} wifi_ap_config_t;

typedef struct {
    bool dhcp_enable;      /* true: DHCP 사용, false: 아래 정적 주소 사용 */
    char ip[16];           /* "192.168.0.10" 형식, dhcp_enable=false일 때만 사용 */
    char gateway[16];
    char netmask[16];
} wifi_ip_config_t;

typedef struct {
    char     ip[16];
    uint16_t port;
} wifi_server_config_t;

typedef enum {
    WIFI_STATE_INIT = 0,
    WIFI_STATE_MODULE_CHECK,
    WIFI_STATE_SET_STATION_MODE,
    WIFI_STATE_SET_DHCP,
    WIFI_STATE_SET_STATIC_IP,
    WIFI_STATE_CONNECT_AP,
    WIFI_STATE_SET_SINGLE_CONN,
    WIFI_STATE_CONNECT_SERVER,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_RETRY_WAIT,
} wifi_state_t;

/* server_cfg는 NULL을 줄 수 있고, 그 경우 AP 접속까지만 수행한다. */
void WiFi_Manager_Init(UART_HandleTypeDef *huart,
                        const wifi_ap_config_t *ap_cfg,
                        const wifi_ip_config_t *ip_cfg,
                        const wifi_server_config_t *server_cfg);

/* main 루프에서 주기적으로 호출 */
void WiFi_Manager_Process(void);

wifi_state_t WiFi_Manager_GetState(void);
bool WiFi_Manager_IsConnected(void); /* AP + 서버(설정된 경우) 모두 연결된 상태 */

/* CONNECTED 상태에서 서버로 데이터 전송. 성공 시 true. */
bool WiFi_Manager_Send(const uint8_t *data, uint16_t len);

#endif /* WIFI_MANAGER_H */
