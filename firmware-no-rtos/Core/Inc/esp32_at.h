/**
 * esp32_at.h
 *
 * USART1로 연결된 ESP32(ESP-AT 펌웨어) 제어 드라이버 (RTOS 미사용 버전).
 * USART1은 CubeMX에서 idle-line 검출 + Circular DMA RX로 설정하고,
 * HAL_UARTEx_ReceiveToIdle_DMA()로 수신을 시작한다 (Esp32_Init()이 내부적으로 처리).
 *
 * 모든 함수는 main()의 super-loop(Esp32_Poll()) 컨텍스트에서만 호출되는 단일 스레드
 * 전제이므로 락이 없다. AT 응답 대기 함수(WaitForToken 등)는 최대 수 초~수십 초까지
 * 블로킹될 수 있으며, 그동안 super-loop의 다른 폴링(FPGA/PC통신)은 진행되지 않는다
 * (firmware-no-rtos/README.md "RTOS 미사용의 결과" 참고).
 */
#ifndef ESP32_AT_H
#define ESP32_AT_H

#include <stdint.h>
#include <stddef.h>
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
 * @brief USART1 DMA 수신 시작 등 드라이버 초기화. main()의 App_Init() 단계에서 1회 호출.
 */
void Esp32_Init(void);

/**
 * @brief ESP32_NRST(PA8, Low active)를 통해 ESP32를 하드웨어 리셋한다. 리셋 펄스 후 모듈이
 *        부팅을 마칠 때까지 블로킹 대기(HAL_Delay)한다.
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
 *        Esp32_Poll()에서 매 반복 호출.
 */
void Esp32_PollUrc(void);

/**
 * @brief 마지막으로 캐시된 ESP32 station IP/MAC 주소를 복사해 반환한다. 단일 스레드 전제라
 *        락이 없다(FpgaLink_Poll()이 PC 프레임 조립 시 호출). WiFi 연결 전에는
 *        "0.0.0.0"/"00:00:00:00:00:00"이 채워진다.
 */
void Esp32_GetCachedNetInfo(char *ip_out, size_t ip_size, char *mac_out, size_t mac_size);

#ifdef __cplusplus
}
#endif

#endif /* ESP32_AT_H */
