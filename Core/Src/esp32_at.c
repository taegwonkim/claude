#include "esp32_at.h"
#include "uart_line_rx.h"
#include "app_config.h"
#include "usart.h" /* huart1 */
#include "main.h"  /* ESP32_NRST_GPIO_Port / ESP32_NRST_Pin (CubeMX 핀 라벨 "ESP32_NRST", PA8) */
#include "cmsis_os2.h"
#include <string.h>
#include <stdio.h>

#define ESP32_RESET_PULSE_MS (100U)  /* NRST Low 유지 시간 */
#define ESP32_BOOT_WAIT_MS   (2000U) /* NRST 해제 후 ESP32 부팅 완료까지 대기 시간 */

static uint8_t s_dmaBuf[APP_UART_RB_SIZE];
static uint8_t s_rbStorage[APP_UART_RB_SIZE * 2U];
static UartLineRx_t s_rx;
static volatile Esp32_LinkState_t s_linkState = ESP32_LINK_DOWN;

/* AT 드라이버 함수들은 ESP32_Task 단일 스레드에서만 호출됨을 전제로 하며(스레드-세이프 아님),
 * 그 외 태스크는 Esp32_GetLinkState()만 안전하게 호출할 수 있다. */

void Esp32_Init(void)
{
    UartLineRx_Init(&s_rx, &huart1, s_dmaBuf, sizeof(s_dmaBuf), s_rbStorage, sizeof(s_rbStorage));
    UartLineRx_Start(&s_rx);
}

void Esp32_HardReset(void)
{
    HAL_GPIO_WritePin(ESP32_NRST_GPIO_Port, ESP32_NRST_Pin, GPIO_PIN_RESET);
    osDelay(ESP32_RESET_PULSE_MS);
    HAL_GPIO_WritePin(ESP32_NRST_GPIO_Port, ESP32_NRST_Pin, GPIO_PIN_SET);
    osDelay(ESP32_BOOT_WAIT_MS);
    s_linkState = ESP32_LINK_DOWN;
}

void Esp32_OnUartRxEvent(UART_HandleTypeDef *huart, uint16_t pos)
{
    if (huart->Instance == huart1.Instance) {
        UartLineRx_HandleEvent(&s_rx, pos);
    }
}

static void SendRaw(const char *cmd, uint32_t len)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, len, APP_AT_RESP_TIMEOUT_MS);
}

static void SendCmd(const char *cmd)
{
    SendRaw(cmd, (uint32_t)strlen(cmd));
}

/* window에 수신 바이트를 계속 누적하며 token 부분 문자열이 나타날 때까지 대기.
 * "ERROR"/"FAIL"이 먼저 나타나면(찾는 token이 그 자체가 아닌 한) 즉시 실패 반환. */
static bool WaitForToken(const char *token, uint32_t timeout_ms)
{
    char window[APP_AT_LINE_MAX];
    uint32_t win_len = 0;
    uint32_t start = HAL_GetTick();

    window[0] = '\0';

    while ((HAL_GetTick() - start) < timeout_ms) {
        uint8_t byte;

        while (RingBuffer_Read(&s_rx.rb, &byte, 1U) == 1U) {
            if (win_len < (sizeof(window) - 1U)) {
                window[win_len++] = (char)byte;
            } else {
                memmove(window, window + 1, sizeof(window) - 2U);
                window[sizeof(window) - 2U] = (char)byte;
            }
            window[win_len] = '\0';

            if (strstr(window, token) != NULL) {
                return true;
            }
            if (strcmp(token, "ERROR") != 0 && strstr(window, "ERROR") != NULL) {
                return false;
            }
            if (strcmp(token, "FAIL") != 0 && strstr(window, "FAIL") != NULL) {
                return false;
            }
        }
        osDelay(2);
    }
    return false;
}

bool Esp32_Probe(void)
{
    SendCmd("AT\r\n");
    if (!WaitForToken("OK", APP_AT_RESP_TIMEOUT_MS)) return false;

    SendCmd("ATE0\r\n");
    if (!WaitForToken("OK", APP_AT_RESP_TIMEOUT_MS)) return false;

    SendCmd("AT+CWMODE=1\r\n");
    if (!WaitForToken("OK", APP_AT_RESP_TIMEOUT_MS)) return false;

    SendCmd("AT+CIPMUX=0\r\n");
    return WaitForToken("OK", APP_AT_RESP_TIMEOUT_MS);
}

bool Esp32_ConnectWifi(const NetConfig_t *cfg)
{
    char cmd[160];

    if (cfg->dhcp_enable) {
        SendCmd("AT+CWDHCP=1,1\r\n");
        if (!WaitForToken("OK", APP_AT_RESP_TIMEOUT_MS)) return false;
    } else {
        SendCmd("AT+CWDHCP=1,0\r\n");
        if (!WaitForToken("OK", APP_AT_RESP_TIMEOUT_MS)) return false;

        snprintf(cmd, sizeof(cmd), "AT+CIPSTA=\"%s\",\"%s\",\"%s\"\r\n",
                 cfg->static_ip, cfg->gateway, cfg->netmask);
        SendCmd(cmd);
        if (!WaitForToken("OK", APP_AT_RESP_TIMEOUT_MS)) return false;
    }

    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", cfg->ssid, cfg->pass);
    SendCmd(cmd);
    if (!WaitForToken("OK", APP_AT_CWJAP_TIMEOUT_MS)) {
        s_linkState = ESP32_LINK_DOWN;
        return false;
    }

    s_linkState = ESP32_WIFI_UP;
    return true;
}

bool Esp32_TcpConnect(const NetConfig_t *cfg)
{
    char cmd[64];

    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u\r\n",
             cfg->server_ip, (unsigned)cfg->server_port);
    SendCmd(cmd);
    if (!WaitForToken("OK", APP_AT_CIPSTART_TIMEOUT_MS)) {
        return false;
    }

    s_linkState = ESP32_TCP_UP;
    return true;
}

bool Esp32_TcpSend(const uint8_t *payload, uint16_t len)
{
    char cmd[32];
    int n;

    if (s_linkState != ESP32_TCP_UP) {
        return false;
    }

    n = snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u\r\n", (unsigned)len);
    SendRaw(cmd, (uint32_t)n);

    if (!WaitForToken(">", 2000U)) {
        return false;
    }

    SendRaw((const char *)payload, len);

    return WaitForToken("SEND OK", APP_AT_RESP_TIMEOUT_MS);
}

void Esp32_TcpClose(void)
{
    SendCmd("AT+CIPCLOSE\r\n");
    (void)WaitForToken("OK", APP_AT_RESP_TIMEOUT_MS);
    if (s_linkState == ESP32_TCP_UP) {
        s_linkState = ESP32_WIFI_UP;
    }
}

Esp32_LinkState_t Esp32_GetLinkState(void)
{
    return s_linkState;
}

void Esp32_PollUrc(void)
{
    char line[APP_AT_LINE_MAX];

    while (UartLineRx_PopLine(&s_rx, line, sizeof(line), '\n')) {
        if (strstr(line, "WIFI DISCONNECT") != NULL) {
            s_linkState = ESP32_LINK_DOWN;
        } else if (strstr(line, "WIFI GOT IP") != NULL) {
            if (s_linkState < ESP32_WIFI_UP) {
                s_linkState = ESP32_WIFI_UP;
            }
        } else if (strstr(line, "CLOSED") != NULL) {
            if (s_linkState == ESP32_TCP_UP) {
                s_linkState = ESP32_WIFI_UP;
            }
        }
    }
}
