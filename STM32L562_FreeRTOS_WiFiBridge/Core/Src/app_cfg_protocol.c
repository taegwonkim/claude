#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "app_cfg_protocol.h"
#include "app_eeprom.h"
#include "app_esp32.h"
#include "cmsis_os2.h"

static AppConfig_t   s_pendingCfg;
static osMutexId_t   s_cfgMutex;

static void ReplyOk(CfgReplyFn reply)
{
    reply("OK\r\n");
}

static void ReplyErr(CfgReplyFn reply, const char *reason)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "ERR:%s\r\n", reason);
    reply(buf);
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

void CfgProtocol_Init(void)
{
    s_cfgMutex = osMutexNew(NULL);
    App_Eeprom_LoadConfig(&s_pendingCfg); /* seed with last-saved values, or defaults */
}

void CfgProtocol_HandleLine(char *line, CfgReplyFn reply)
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
        ReplyErr(reply, "UNKNOWN");
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

    osMutexAcquire(s_cfgMutex, osWaitForever);

    if (strcmp(key, "SSID") == 0 && val) {
        CopyValue(s_pendingCfg.ap_ssid, sizeof(s_pendingCfg.ap_ssid), val);
        ReplyOk(reply);
    } else if (strcmp(key, "PASS") == 0 && val) {
        CopyValue(s_pendingCfg.ap_pass, sizeof(s_pendingCfg.ap_pass), val);
        ReplyOk(reply);
    } else if (strcmp(key, "DHCP") == 0 && val) {
        s_pendingCfg.dhcp_enable = (uint8_t)(atoi(val) != 0);
        ReplyOk(reply);
    } else if (strcmp(key, "IP") == 0 && val) {
        if (!IsValidIPv4(val)) { ReplyErr(reply, "BAD_IP"); }
        else { CopyValue(s_pendingCfg.static_ip, sizeof(s_pendingCfg.static_ip), val); ReplyOk(reply); }
    } else if (strcmp(key, "GW") == 0 && val) {
        if (!IsValidIPv4(val)) { ReplyErr(reply, "BAD_IP"); }
        else { CopyValue(s_pendingCfg.static_gw, sizeof(s_pendingCfg.static_gw), val); ReplyOk(reply); }
    } else if (strcmp(key, "MASK") == 0 && val) {
        if (!IsValidIPv4(val)) { ReplyErr(reply, "BAD_IP"); }
        else { CopyValue(s_pendingCfg.static_mask, sizeof(s_pendingCfg.static_mask), val); ReplyOk(reply); }
    } else if (strcmp(key, "SERVERIP") == 0 && val) {
        if (!IsValidIPv4(val)) { ReplyErr(reply, "BAD_IP"); }
        else { CopyValue(s_pendingCfg.server_ip, sizeof(s_pendingCfg.server_ip), val); ReplyOk(reply); }
    } else if (strcmp(key, "SERVERPORT") == 0 && val) {
        int port = atoi(val);
        if (port <= 0 || port > 65535) { ReplyErr(reply, "BAD_PORT"); }
        else { s_pendingCfg.server_port = (uint16_t)port; ReplyOk(reply); }
    } else if (strcmp(key, "SAVE") == 0) {
        if (s_pendingCfg.ap_ssid[0] == '\0') {
            ReplyErr(reply, "NO_SSID");
        } else if (!App_Eeprom_SaveConfig(&s_pendingCfg)) {
            ReplyErr(reply, "EEPROM_WRITE");
        } else {
            ReplyOk(reply);
            App_Esp32_RequestConnect(); /* async: (re)connect to AP + server */
        }
    } else if (strcmp(key, "CONNECT") == 0) {
        ReplyOk(reply);
        App_Esp32_RequestConnect();
    } else {
        ReplyErr(reply, "UNKNOWN");
    }

    osMutexRelease(s_cfgMutex);
}
