#include "pc_comm.h"
#include "uart_line_rx.h"
#include "app_config.h"
#include "app_freertos.h" /* g_cfgEventQueueId */
#include "esp32_at.h"      /* Esp32_GetLinkState (STATUS 커맨드용) */
#include "usbd_cdc_if.h"   /* CDC_Transmit_FS (USB_DEVICE 미들웨어 생성) */
#include "cmsis_os2.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

static uint8_t s_dmaBuf[APP_UART_RB_SIZE];
static uint8_t s_uartRbStorage[APP_UART_RB_SIZE * 2U];
static UartLineRx_t s_uartRx;

static uint8_t s_usbRbStorage[APP_UART_RB_SIZE * 2U];
static RingBuffer_t s_usbRb;

static NetConfig_t s_pending;

static const char *const HELP_TEXT =
    "SET SSID=<s> | SET PASS=<s> | SET SERVER_IP=<ip> | SET SERVER_PORT=<port>\r\n"
    "SET DHCP=ON|OFF | SET IP=<ip> | SET GATEWAY=<ip> | SET MASK=<ip>\r\n"
    "SAVE | GET CONFIG | STATUS | HELP";

void PcComm_Init(const NetConfig_t *initial_cfg)
{
    s_pending = *initial_cfg;

    UartLineRx_Init(&s_uartRx, &huart3, s_dmaBuf, sizeof(s_dmaBuf),
                     s_uartRbStorage, sizeof(s_uartRbStorage));
    UartLineRx_Start(&s_uartRx);

    RingBuffer_Init(&s_usbRb, s_usbRbStorage, sizeof(s_usbRbStorage));
}

void PcComm_OnUartRxEvent(UART_HandleTypeDef *huart, uint16_t pos)
{
    if (huart->Instance == huart3.Instance) {
        UartLineRx_HandleEvent(&s_uartRx, pos);
    }
}

void PC_Comm_FeedUSB(uint8_t *buf, uint32_t len)
{
    RingBuffer_Write(&s_usbRb, buf, len);
}

void PcComm_BroadcastLine(const char *line)
{
    char out[APP_PC_LINE_MAX + 2U];
    int n = snprintf(out, sizeof(out), "%s\r\n", line);

    if (n < 0) {
        return;
    }
    if ((uint32_t)n >= sizeof(out)) {
        n = (int)sizeof(out) - 1;
    }

    HAL_UART_Transmit(&huart3, (uint8_t *)out, (uint16_t)n, 100U);
    CDC_Transmit_FS((uint8_t *)out, (uint16_t)n);
}

/* "KEY=VALUE" 형태에서 KEY 뒤 '=' 다음 문자열 포인터 반환 (없으면 NULL) */
static const char *ValueAfterEquals(const char *s)
{
    const char *eq = strchr(s, '=');
    return (eq != NULL) ? (eq + 1) : NULL;
}

static void TrimTrailingCrLf(char *s)
{
    size_t len = strlen(s);
    while (len > 0U && (s[len - 1U] == '\r' || s[len - 1U] == '\n')) {
        s[--len] = '\0';
    }
}

static bool IsValidIPv4(const char *s)
{
    int parts = 0, val = -1;
    const char *p = s;

    if (s == NULL || s[0] == '\0') return false;

    while (1) {
        if (isdigit((unsigned char)*p)) {
            if (val < 0) val = 0;
            val = val * 10 + (*p - '0');
            if (val > 255) return false;
        } else if (*p == '.' || *p == '\0') {
            if (val < 0) return false;
            parts++;
            val = -1;
            if (*p == '\0') break;
        } else {
            return false;
        }
        p++;
    }
    return parts == 4;
}

static void ProcessLine(char *line)
{
    char reply[64];
    const char *val;

    TrimTrailingCrLf(line);
    if (line[0] == '\0') {
        return;
    }

    if (strncmp(line, "SET SSID=", 9) == 0) {
        val = line + 9;
        if (strlen(val) >= NET_CONFIG_SSID_MAX) {
            PcComm_BroadcastLine("ERR SSID_TOO_LONG");
            return;
        }
        strncpy(s_pending.ssid, val, NET_CONFIG_SSID_MAX - 1U);
        s_pending.ssid[NET_CONFIG_SSID_MAX - 1U] = '\0';
        PcComm_BroadcastLine("OK");

    } else if (strncmp(line, "SET PASS=", 9) == 0) {
        val = line + 9;
        if (strlen(val) >= NET_CONFIG_PASS_MAX) {
            PcComm_BroadcastLine("ERR PASS_TOO_LONG");
            return;
        }
        strncpy(s_pending.pass, val, NET_CONFIG_PASS_MAX - 1U);
        s_pending.pass[NET_CONFIG_PASS_MAX - 1U] = '\0';
        PcComm_BroadcastLine("OK");

    } else if (strncmp(line, "SET SERVER_IP=", 14) == 0) {
        val = line + 14;
        if (!IsValidIPv4(val)) {
            PcComm_BroadcastLine("ERR INVALID_IP");
            return;
        }
        strncpy(s_pending.server_ip, val, sizeof(s_pending.server_ip) - 1U);
        s_pending.server_ip[sizeof(s_pending.server_ip) - 1U] = '\0';
        PcComm_BroadcastLine("OK");

    } else if (strncmp(line, "SET SERVER_PORT=", 16) == 0) {
        long port = strtol(line + 16, NULL, 10);
        if (port <= 0 || port > 65535) {
            PcComm_BroadcastLine("ERR INVALID_PORT");
            return;
        }
        s_pending.server_port = (uint16_t)port;
        PcComm_BroadcastLine("OK");

    } else if (strcmp(line, "SET DHCP=ON") == 0) {
        s_pending.dhcp_enable = 1U;
        PcComm_BroadcastLine("OK");

    } else if (strcmp(line, "SET DHCP=OFF") == 0) {
        s_pending.dhcp_enable = 0U;
        PcComm_BroadcastLine("OK");

    } else if (strncmp(line, "SET IP=", 7) == 0) {
        val = line + 7;
        if (!IsValidIPv4(val)) { PcComm_BroadcastLine("ERR INVALID_IP"); return; }
        strncpy(s_pending.static_ip, val, sizeof(s_pending.static_ip) - 1U);
        s_pending.static_ip[sizeof(s_pending.static_ip) - 1U] = '\0';
        PcComm_BroadcastLine("OK");

    } else if (strncmp(line, "SET GATEWAY=", 12) == 0) {
        val = line + 12;
        if (!IsValidIPv4(val)) { PcComm_BroadcastLine("ERR INVALID_IP"); return; }
        strncpy(s_pending.gateway, val, sizeof(s_pending.gateway) - 1U);
        s_pending.gateway[sizeof(s_pending.gateway) - 1U] = '\0';
        PcComm_BroadcastLine("OK");

    } else if (strncmp(line, "SET MASK=", 9) == 0) {
        val = line + 9;
        if (!IsValidIPv4(val)) { PcComm_BroadcastLine("ERR INVALID_IP"); return; }
        strncpy(s_pending.netmask, val, sizeof(s_pending.netmask) - 1U);
        s_pending.netmask[sizeof(s_pending.netmask) - 1U] = '\0';
        PcComm_BroadcastLine("OK");

    } else if (strcmp(line, "SAVE") == 0) {
        if (s_pending.dhcp_enable == 0U &&
            (s_pending.static_ip[0] == '\0' || s_pending.gateway[0] == '\0' ||
             s_pending.netmask[0] == '\0')) {
            PcComm_BroadcastLine("ERR STATIC_IP_INCOMPLETE");
            return;
        }
        if (osMessageQueuePut(g_cfgEventQueueId, &s_pending, 0, 0) != osOK) {
            PcComm_BroadcastLine("ERR QUEUE_FULL");
            return;
        }
        PcComm_BroadcastLine("SAVED");

    } else if (strcmp(line, "GET CONFIG") == 0) {
        char buf[APP_PC_LINE_MAX];
        snprintf(buf, sizeof(buf), "SSID=%s", s_pending.ssid);
        PcComm_BroadcastLine(buf);
        PcComm_BroadcastLine("PASS=****");
        snprintf(buf, sizeof(buf), "SERVER_IP=%s", s_pending.server_ip);
        PcComm_BroadcastLine(buf);
        snprintf(buf, sizeof(buf), "SERVER_PORT=%u", (unsigned)s_pending.server_port);
        PcComm_BroadcastLine(buf);
        PcComm_BroadcastLine(s_pending.dhcp_enable ? "DHCP=ON" : "DHCP=OFF");
        snprintf(buf, sizeof(buf), "IP=%s", s_pending.static_ip);
        PcComm_BroadcastLine(buf);
        snprintf(buf, sizeof(buf), "GATEWAY=%s", s_pending.gateway);
        PcComm_BroadcastLine(buf);
        snprintf(buf, sizeof(buf), "MASK=%s", s_pending.netmask);
        PcComm_BroadcastLine(buf);

    } else if (strcmp(line, "STATUS") == 0) {
        Esp32_LinkState_t st = Esp32_GetLinkState();
        snprintf(reply, sizeof(reply), "STATUS WIFI=%s TCP=%s",
                 (st >= ESP32_WIFI_UP) ? "UP" : "DOWN",
                 (st == ESP32_TCP_UP) ? "UP" : "DOWN");
        PcComm_BroadcastLine(reply);

    } else if (strcmp(line, "HELP") == 0) {
        PcComm_BroadcastLine(HELP_TEXT);

    } else {
        PcComm_BroadcastLine("ERR UNKNOWN_COMMAND");
    }
}

void PcComm_Task(void *argument)
{
    (void)argument;
    char line[APP_PC_LINE_MAX];

    for (;;) {
        while (RingBuffer_PopLine(&s_uartRx.rb, line, sizeof(line), '\n')) {
            ProcessLine(line);
        }
        while (RingBuffer_PopLine(&s_usbRb, line, sizeof(line), '\n')) {
            ProcessLine(line);
        }
        osDelay(10);
    }
}
