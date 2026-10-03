#include "pc_comm.h"
#include "uart_line_rx.h"
#include "pc_frame.h"
#include "app_config.h"
#include "app_freertos.h" /* g_cfgEventQueueId */
#include "esp32_at.h"      /* Esp32_GetLinkState (STATUS 커맨드용) */
#include "rtc_wakeup.h"    /* RtcWakeup_GetPeriodSec/SetPeriodSec (RESET_R_ALL/RESET_W_ALL) */
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

void PcComm_BroadcastFrame(const char *csv_payload)
{
    char out[APP_PC_LINE_MAX + 4U]; /* STX + payload + CR LF + NUL 여유 */
    int n = PcFrame_Wrap(out, sizeof(out), csv_payload);

    if (n <= 0) {
        return;
    }

    HAL_UART_Transmit(&huart3, (uint8_t *)out, (uint16_t)n, 100U);
    CDC_Transmit_FS((uint8_t *)out, (uint16_t)n);
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

/* fields: PcFrame_Parse()로 분리된 필드 배열, field_count: 필드 개수(fields[0]가 커맨드명) */
static void ProcessSet(char *fields[], int field_count)
{
    const char *key;
    const char *val;

    if (field_count < 3) {
        PcComm_BroadcastFrame("ERR,MISSING_ARGS");
        return;
    }
    key = fields[1];
    val = fields[2];

    if (strcmp(key, "SSID") == 0) {
        if (strlen(val) >= NET_CONFIG_SSID_MAX) {
            PcComm_BroadcastFrame("ERR,SSID_TOO_LONG");
            return;
        }
        strncpy(s_pending.ssid, val, NET_CONFIG_SSID_MAX - 1U);
        s_pending.ssid[NET_CONFIG_SSID_MAX - 1U] = '\0';
        PcComm_BroadcastFrame("OK");

    } else if (strcmp(key, "PASS") == 0) {
        if (strlen(val) >= NET_CONFIG_PASS_MAX) {
            PcComm_BroadcastFrame("ERR,PASS_TOO_LONG");
            return;
        }
        strncpy(s_pending.pass, val, NET_CONFIG_PASS_MAX - 1U);
        s_pending.pass[NET_CONFIG_PASS_MAX - 1U] = '\0';
        PcComm_BroadcastFrame("OK");

    } else if (strcmp(key, "SERVER_IP") == 0) {
        if (!IsValidIPv4(val)) {
            PcComm_BroadcastFrame("ERR,INVALID_IP");
            return;
        }
        strncpy(s_pending.server_ip, val, sizeof(s_pending.server_ip) - 1U);
        s_pending.server_ip[sizeof(s_pending.server_ip) - 1U] = '\0';
        PcComm_BroadcastFrame("OK");

    } else if (strcmp(key, "SERVER_PORT") == 0) {
        long port = strtol(val, NULL, 10);
        if (port <= 0 || port > 65535) {
            PcComm_BroadcastFrame("ERR,INVALID_PORT");
            return;
        }
        s_pending.server_port = (uint16_t)port;
        PcComm_BroadcastFrame("OK");

    } else if (strcmp(key, "DHCP") == 0) {
        if (strcmp(val, "ON") == 0) {
            s_pending.dhcp_enable = 1U;
        } else if (strcmp(val, "OFF") == 0) {
            s_pending.dhcp_enable = 0U;
        } else {
            PcComm_BroadcastFrame("ERR,INVALID_DHCP");
            return;
        }
        PcComm_BroadcastFrame("OK");

    } else if (strcmp(key, "IP") == 0) {
        if (!IsValidIPv4(val)) { PcComm_BroadcastFrame("ERR,INVALID_IP"); return; }
        strncpy(s_pending.static_ip, val, sizeof(s_pending.static_ip) - 1U);
        s_pending.static_ip[sizeof(s_pending.static_ip) - 1U] = '\0';
        PcComm_BroadcastFrame("OK");

    } else if (strcmp(key, "GATEWAY") == 0) {
        if (!IsValidIPv4(val)) { PcComm_BroadcastFrame("ERR,INVALID_IP"); return; }
        strncpy(s_pending.gateway, val, sizeof(s_pending.gateway) - 1U);
        s_pending.gateway[sizeof(s_pending.gateway) - 1U] = '\0';
        PcComm_BroadcastFrame("OK");

    } else if (strcmp(key, "MASK") == 0) {
        if (!IsValidIPv4(val)) { PcComm_BroadcastFrame("ERR,INVALID_IP"); return; }
        strncpy(s_pending.netmask, val, sizeof(s_pending.netmask) - 1U);
        s_pending.netmask[sizeof(s_pending.netmask) - 1U] = '\0';
        PcComm_BroadcastFrame("OK");

    } else {
        PcComm_BroadcastFrame("ERR,UNKNOWN_KEY");
    }
}

static void ProcessSave(void)
{
    if (s_pending.dhcp_enable == 0U &&
        (s_pending.static_ip[0] == '\0' || s_pending.gateway[0] == '\0' ||
         s_pending.netmask[0] == '\0')) {
        PcComm_BroadcastFrame("ERR,STATIC_IP_INCOMPLETE");
        return;
    }
    if (osMessageQueuePut(g_cfgEventQueueId, &s_pending, 0, 0) != osOK) {
        PcComm_BroadcastFrame("ERR,QUEUE_FULL");
        return;
    }
    PcComm_BroadcastFrame("SAVED");
}

static void ProcessGetConfig(void)
{
    char buf[APP_PC_LINE_MAX];

    snprintf(buf, sizeof(buf), "CONFIG,%s,****,%s,%u,%s,%s,%s,%s",
             s_pending.ssid,
             s_pending.server_ip,
             (unsigned)s_pending.server_port,
             s_pending.dhcp_enable ? "ON" : "OFF",
             s_pending.static_ip,
             s_pending.gateway,
             s_pending.netmask);
    PcComm_BroadcastFrame(buf);
}

static void ProcessStatus(void)
{
    char buf[32];
    Esp32_LinkState_t st = Esp32_GetLinkState();

    /* Status Number = Esp32_LinkState_t 값 그대로: 0=DOWN, 1=WIFI_UP, 2=TCP_UP
     * (docs/프로토콜_명세.md §1). ESP32_Task가 이 값을 주기적으로도 브로드캐스트한다. */
    snprintf(buf, sizeof(buf), "STATUS,%d", (int)st);
    PcComm_BroadcastFrame(buf);
}

/* <STX>RESET_W_ALL,<seconds><CR><LF>: RTC Wakeup Timer 주기적 리셋 간격(초)을 설정한다.
 * 성공 시 즉시 플래시에 저장하고 Wakeup Timer를 새 값으로 재무장한다(docs/프로토콜_명세.md §6). */
static void ProcessResetWriteAll(char *fields[], int field_count)
{
    long seconds;

    if (field_count < 2) {
        PcComm_BroadcastFrame("RESET_W_ALL,ERR,MISSING_ARGS");
        return;
    }

    seconds = strtol(fields[1], NULL, 10);
    if (seconds <= 0 || !RtcWakeup_SetPeriodSec((uint32_t)seconds)) {
        PcComm_BroadcastFrame("RESET_W_ALL,ERR,INVALID_SECONDS");
        return;
    }

    PcComm_BroadcastFrame("RESET_W_ALL,OK");
}

/* <STX>RESET_R_ALL<CR><LF>: 현재 설정된 리셋 주기(초)를 조회한다. */
static void ProcessResetReadAll(void)
{
    char buf[32];

    snprintf(buf, sizeof(buf), "RESET_R_ALL,%lu", (unsigned long)RtcWakeup_GetPeriodSec());
    PcComm_BroadcastFrame(buf);
}

static void ProcessFrame(char *line)
{
    char *fields[PC_FRAME_MAX_FIELDS];
    int field_count;
    const char *cmd;

    TrimTrailingCrLf(line);
    if (line[0] == '\0') {
        return;
    }

    field_count = PcFrame_Parse(line, fields, PC_FRAME_MAX_FIELDS);
    if (field_count <= 0) {
        PcComm_BroadcastFrame("ERR,BAD_FRAME");
        return;
    }
    cmd = fields[0];

    if (strcmp(cmd, "SET") == 0) {
        ProcessSet(fields, field_count);
    } else if (strcmp(cmd, "SAVE") == 0) {
        ProcessSave();
    } else if (strcmp(cmd, "GET") == 0 && field_count >= 2 && strcmp(fields[1], "CONFIG") == 0) {
        ProcessGetConfig();
    } else if (strcmp(cmd, "STATUS") == 0) {
        ProcessStatus();
    } else if (strcmp(cmd, "RESET_W_ALL") == 0) {
        ProcessResetWriteAll(fields, field_count);
    } else if (strcmp(cmd, "RESET_R_ALL") == 0) {
        ProcessResetReadAll();
    } else if (strcmp(cmd, "HELP") == 0) {
        PcComm_BroadcastFrame(
            "HELP,SET SSID=<s> / SET PASS=<s> / SET SERVER_IP=<ip> / SET SERVER_PORT=<port> / "
            "SET DHCP=ON|OFF / SET IP=<ip> / SET GATEWAY=<ip> / SET MASK=<ip> / SAVE / GET CONFIG / STATUS / "
            "RESET_W_ALL,<seconds> / RESET_R_ALL / HELP");
    } else {
        PcComm_BroadcastFrame("ERR,UNKNOWN_COMMAND");
    }
}

void PcComm_Task(void *argument)
{
    (void)argument;
    char line[APP_PC_LINE_MAX];

    for (;;) {
        while (RingBuffer_PopLine(&s_uartRx.rb, line, sizeof(line), '\n')) {
            ProcessFrame(line);
        }
        while (RingBuffer_PopLine(&s_usbRb, line, sizeof(line), '\n')) {
            ProcessFrame(line);
        }
        osDelay(10);
    }
}
