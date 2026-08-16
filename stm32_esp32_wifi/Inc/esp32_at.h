/*
 * esp32_at.h
 *
 * ESP32-C3-WROOM(ESP-AT firmware)를 STM32 HAL UART로 제어하기 위한
 * 최소한의 AT 명령 송수신 드라이버.
 *
 *  - TX: HAL_UART_Transmit()으로 블로킹 전송 (AT 명령 길이가 짧아 문제 없음)
 *  - RX: 1바이트 인터럽트 수신을 이어붙여 라인 단위 링버퍼에 저장
 *        "\r\n"으로 끝나는 한 줄, 또는 CIPSEND의 '>' 프롬프트를 인식
 *
 * 사용하는 UART의 HAL_UART_RxCpltCallback()에서 반드시
 * ESP32_AT_UART_RxCpltCallback()을 호출해 주어야 한다.
 */

#ifndef ESP32_AT_H
#define ESP32_AT_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32l5xx_hal.h"

#define ESP32_AT_LINE_MAX      256
#define ESP32_AT_LINE_QUEUE    16

typedef enum {
    AT_OK = 0,
    AT_ERROR,
    AT_TIMEOUT,
    AT_BUSY,
} at_status_t;

void ESP32_AT_Init(UART_HandleTypeDef *huart);

/* cmd는 개행 없이 넘기면 내부에서 "\r\n"을 붙여 전송한다.
 * expected는 성공으로 볼 부분 문자열(예: "OK"), NULL이면 "OK"/"ERROR"만 검사.
 * 응답 라인들 중 expected가 포함된 라인이 오면 AT_OK, "ERROR"/"FAIL"이면 AT_ERROR,
 * timeout_ms 안에 응답이 없으면 AT_TIMEOUT을 반환한다. */
at_status_t ESP32_AT_SendCommand(const char *cmd, const char *expected, uint32_t timeout_ms);

/* SendCommand와 동일하게 동작하되, 응답 라인 중 capture_substr을 포함하는
 * 라인이 있으면 그 라인 전체를 capture_out에 복사해 둔다(찾지 못하면
 * capture_out은 건드리지 않으므로 호출 전에 빈 문자열로 초기화해 둘 것).
 * capture_substr이 NULL이면 ESP32_AT_SendCommand와 완전히 동일하다.
 * AT+CIPSTATUS처럼 "OK"와 별개로 상태를 담은 라인을 함께 읽어야 할 때 사용한다. */
at_status_t ESP32_AT_SendCommandEx(const char *cmd, const char *expected, uint32_t timeout_ms,
                                    const char *capture_substr, char *capture_out, uint16_t capture_out_size);

/* raw 데이터 전송(CIPSEND 이후 '>' 프롬프트를 받은 뒤 페이로드 전송용) */
void ESP32_AT_SendRaw(const uint8_t *data, uint16_t len);

/* 마지막으로 수신된 한 줄을 큐에서 꺼낸다. 없으면 false.
 * WiFi 매니저가 URC("WIFI DISCONNECT" 등)를 감시하는 데 사용한다. */
bool ESP32_AT_PopLine(char *out, uint16_t out_size);

/* 인터럽트 콜백: 각 UART의 HAL_UART_RxCpltCallback()에서 huart가 일치할 때 호출 */
void ESP32_AT_UART_RxCpltCallback(UART_HandleTypeDef *huart);

#endif /* ESP32_AT_H */
