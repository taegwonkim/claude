#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "app_pc_uart.h"
#include "app_eeprom.h"
#include "app_esp32.h"
#include "ring_buffer.h"
#include "cmsis_os2.h"

static uint8_t      s_rxStorage[PC_UART_RX_RINGBUF_SIZE];
static RingBuffer_t  s_rxRing;
static uint8_t       s_rxByte;              /* 1-byte IT scratch buffer */

static char          s_line[PC_UART_LINE_MAX_LEN];
static uint16_t      s_lineLen;

static AppConfig_t   s_pendingCfg;

static void PcUart_Reply(const char *msg)
{
    HAL_UART_Transmit(PC_UART_HANDLE, (uint8_t *)msg, (uint16_t)strlen(msg), 100U);
}

static void PcUart_ReplyOk(void)
{
    PcUart_Reply("OK\r\n");
}

static void PcUart_ReplyErr(const char *reason)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "ERR:%s\r\n", reason);
    PcUart_Reply(buf);
}

/* Copy "KEY=VALUE" payload (value already isolated) into a fixed dest,
 * truncating to max_len characters. */
static void CopyValue(char *dst, size_t dst_size, const char *src)
{
    size_t n = strlen(src);
    if (n >= dst_size) {
        n = dst_size - 1U;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static bool IsValidIPv4(const char *s)
{
    int parts = 0, val = -1;
    const char *p = s;

    while (*p) {
        if (*p >= '0' && *p <= '9') {
            if (val < 0) val = 0;
            val = val * 10 + (*p - '0');
            if (val > 255) return false;
        } else if (*p == '.') {
            if (val < 0) return false;
            parts++;
            val = -1;
        } else {
            return false;
        }
        p++;
    }
    return (val >= 0) && (parts == 3);
}

static void HandleLine(char *line)
{
    /* strip trailing CR/LF/whitespace */
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
        line[--len] = '\0';
    }
    if (len == 0) {
        return; /* ignore blank lines */
    }

    if (strncmp(line, "CFG:", 4) != 0) {
        PcUart_ReplyErr("UNKNOWN");
        return;
    }
    char *rest = line + 4;
    char *eq = strchr(rest, '=');
    char *key = rest;
    char *val = NULL;
    if (eq != NULL) {
        *eq = '\0';
        val = eq + 1;
    }

    if (strcmp(key, "SSID") == 0 && val) {
        CopyValue(s_pendingCfg.ap_ssid, sizeof(s_pendingCfg.ap_ssid), val);
        PcUart_ReplyOk();
    } else if (strcmp(key, "PASS") == 0 && val) {
        CopyValue(s_pendingCfg.ap_pass, sizeof(s_pendingCfg.ap_pass), val);
        PcUart_ReplyOk();
    } else if (strcmp(key, "DHCP") == 0 && val) {
        s_pendingCfg.dhcp_enable = (uint8_t)(atoi(val) != 0);
        PcUart_ReplyOk();
    } else if (strcmp(key, "IP") == 0 && val) {
        if (!IsValidIPv4(val)) { PcUart_ReplyErr("BAD_IP"); return; }
        CopyValue(s_pendingCfg.static_ip, sizeof(s_pendingCfg.static_ip), val);
        PcUart_ReplyOk();
    } else if (strcmp(key, "GW") == 0 && val) {
        if (!IsValidIPv4(val)) { PcUart_ReplyErr("BAD_IP"); return; }
        CopyValue(s_pendingCfg.static_gw, sizeof(s_pendingCfg.static_gw), val);
        PcUart_ReplyOk();
    } else if (strcmp(key, "MASK") == 0 && val) {
        if (!IsValidIPv4(val)) { PcUart_ReplyErr("BAD_IP"); return; }
        CopyValue(s_pendingCfg.static_mask, sizeof(s_pendingCfg.static_mask), val);
        PcUart_ReplyOk();
    } else if (strcmp(key, "SERVERIP") == 0 && val) {
        if (!IsValidIPv4(val)) { PcUart_ReplyErr("BAD_IP"); return; }
        CopyValue(s_pendingCfg.server_ip, sizeof(s_pendingCfg.server_ip), val);
        PcUart_ReplyOk();
    } else if (strcmp(key, "SERVERPORT") == 0 && val) {
        int port = atoi(val);
        if (port <= 0 || port > 65535) { PcUart_ReplyErr("BAD_PORT"); return; }
        s_pendingCfg.server_port = (uint16_t)port;
        PcUart_ReplyOk();
    } else if (strcmp(key, "SAVE") == 0) {
        if (s_pendingCfg.ap_ssid[0] == '\0') {
            PcUart_ReplyErr("NO_SSID");
            return;
        }
        if (!App_Eeprom_SaveConfig(&s_pendingCfg)) {
            PcUart_ReplyErr("EEPROM_WRITE");
            return;
        }
        PcUart_ReplyOk();
        App_Esp32_RequestConnect(); /* async: (re)connect to AP + server */
    } else if (strcmp(key, "CONNECT") == 0) {
        PcUart_ReplyOk();
        App_Esp32_RequestConnect();
    } else {
        PcUart_ReplyErr("UNKNOWN");
    }
}

void App_PcUart_Init(void)
{
    RingBuffer_Init(&s_rxRing, s_rxStorage, PC_UART_RX_RINGBUF_SIZE);
    s_lineLen = 0;

    App_Eeprom_LoadConfig(&s_pendingCfg); /* seed with last-saved values, or defaults */

    HAL_UART_Receive_IT(PC_UART_HANDLE, &s_rxByte, 1U);
}

void App_PcUart_UART_RxCpltCallback(void)
{
    RingBuffer_PutByte(&s_rxRing, s_rxByte);
    HAL_UART_Receive_IT(PC_UART_HANDLE, &s_rxByte, 1U);
}

void App_PcUart_Task(void *argument)
{
    (void)argument;
    uint8_t byte;

    for (;;) {
        if (RingBuffer_GetByte(&s_rxRing, &byte)) {
            if (byte == '\n') {
                s_line[s_lineLen] = '\0';
                HandleLine(s_line);
                s_lineLen = 0;
            } else if (byte != '\r') {
                if (s_lineLen < (PC_UART_LINE_MAX_LEN - 1U)) {
                    s_line[s_lineLen++] = (char)byte;
                } else {
                    /* line too long: drop it and resync on next '\n' */
                    s_lineLen = 0;
                }
            }
        } else {
            osDelay(5);
        }
    }
}
