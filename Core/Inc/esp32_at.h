/**
 * esp32_at.h
 *
 * USART1로 연결된 ESP32(ESP-AT 펌웨어) 제어 드라이버.
 * USART1은 CubeMX에서 idle-line 검출 + Circular DMA RX로 설정하고,
 * HAL_UARTEx_ReceiveToIdle_DMA()로 수신을 시작한다 (ESP32_Start()가 내부적으로 처리).
 */
#ifndef ESP32_AT_H
#define ESP32_AT_H

#include <stdint.h>
#include <stdbool.h>
#include "net_config_store.h"
#include "usart.h" /* UART_HandleTypeDef */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP32_LINK_DOWN = 0,
    ESP32_WIFI_UP,        /* WiFi 연결됨, TCP 미연결 */
    ESP32_TCP_UP,         /* WiFi + 서버 TCP 연결됨 */
} Esp32_LinkState_t;

/**
 * @brief USART1 DMA 수신 시작 등 드라이버 초기화. RTOS 스케줄러 시작 후(태스크 컨텍스트에서) 호출.
 */
void Esp32_Init(void);

/**
 * @brief ESP32_NRST(PA8, Low active)를 통해 ESP32를 하드웨어 리셋한다. 리셋 펄스 후 모듈이
 *        부팅을 마칠 때까지 블로킹 대기(osDelay)하므로 태스크 컨텍스트에서만 호출할 것.
 *        보통 Esp32_Init() 직후, Esp32_Probe() 이전에 1회 호출해 이전 세션 상태를 정리한다.
 */
void Esp32_HardReset(void);

/**
 * @brief HAL_UARTEx_RxEventCallback에서 huart 종류에 상관없이 호출. 내부에서 USART1 여부 확인.
 */
void Esp32_OnUartRxEvent(UART_HandleTypeDef *huart, uint16_t pos);

/**
 * @brief 모듈 응답 확인(AT) + 에코 off(ATE0) + station 모드 설정까지 수행.
 * @return true: 모듈 정상 응답
 */
bool Esp32_Probe(void);

/**
 * @brief cfg에 따라 WiFi 연결(DHCP 또는 정적 IP)을 시도한다. 블로킹 호출(최대 APP_AT_CWJAP_TIMEOUT_MS).
 */
bool Esp32_ConnectWifi(const NetConfig_t *cfg);

/**
 * @brief cfg의 server_ip:server_port로 단일 TCP 연결을 연다.
 */
bool Esp32_TcpConnect(const NetConfig_t *cfg);

/**
 * @brief 현재 TCP 연결로 payload(len바이트)를 전송한다 (AT+CIPSEND).
 */
bool Esp32_TcpSend(const uint8_t *payload, uint16_t len);

/**
 * @brief 현재 TCP 연결을 닫는다 (AT+CIPCLOSE). WiFi 재설정으로 재연결하기 전에 호출.
 */
void Esp32_TcpClose(void);

/**
 * @brief 현재 링크 상태를 반환한다 (URC 백그라운드 파싱 결과 반영).
 */
Esp32_LinkState_t Esp32_GetLinkState(void);

/**
 * @brief 수신 링버퍼에 도착한 비동기 URC(WIFI DISCONNECT/CLOSED 등)를 처리해 링크 상태를 갱신한다.
 *        ESP32_Task 루프에서 주기적으로(또는 응답 대기 사이사이) 호출.
 */
void Esp32_PollUrc(void);

#ifdef __cplusplus
}
#endif

#endif /* ESP32_AT_H */
