/* =========================================================
 * UART 프로토콜 구현 (ASCII 명령/응답)
 * =========================================================*/
#include "uart_protocol.h"
#include "current_monitor.h"
#include "cmsis_os2.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

/* ----- 외부 큐 핸들 (freertos.c에서 생성) ----- */
extern osMessageQueueId_t xRespQueueHandle;

/* ----- 내부 송신 버퍼 ----- */
static char tx_line_buf[PROTO_TX_BUF_SIZE];

/* =========================================================
 * 초기화
 * =========================================================*/
void Proto_Init(void)
{
    /* DMA 기반 수신 시작은 freertos.c의 UartRxTask에서 수행 */
    Proto_SendVersion();
    Proto_SendHelp();
}

/* =========================================================
 * 명령 파싱
 * 입력: "SET 1 1500\r\n" 또는 "GET 2\r\n" 등
 * =========================================================*/
bool Proto_ParseCmd(const char *raw, ParsedCmd_t *cmd)
{
    if (!raw || !cmd) return false;

    char buf[PROTO_CMD_MAX_LEN];
    strncpy(buf, raw, PROTO_CMD_MAX_LEN - 1);
    buf[PROTO_CMD_MAX_LEN - 1] = '\0';

    /* 줄바꿈 제거 */
    char *p = buf;
    while (*p) {
        if (*p == '\r' || *p == '\n') { *p = '\0'; break; }
        *p = toupper((unsigned char)*p);
        p++;
    }

    /* 초기화 */
    memset(cmd, 0, sizeof(ParsedCmd_t));
    cmd->type = CMD_UNKNOWN;

    /* 명령어 토큰 파싱 */
    char *token = strtok(buf, " \t");
    if (!token) return false;

    /* ----- SET <ch> <mv> ----- */
    if (strcmp(token, "SET") == 0) {
        char *ch_str  = strtok(NULL, " \t");
        char *mv_str  = strtok(NULL, " \t");
        if (!ch_str || !mv_str) return false;

        int ch  = atoi(ch_str) - 1; /* 1-based → 0-based */
        int mv  = atoi(mv_str);

        if (ch < 0 || ch >= VOLTAGE_NUM_CH) return false;
        if (mv < VOLTAGE_MIN_MV || mv > VOLTAGE_MAX_MV) return false;

        cmd->type    = CMD_SET;
        cmd->channel = (uint8_t)ch;
        cmd->value   = (int32_t)mv;
        return true;
    }

    /* ----- GET <ch> ----- */
    if (strcmp(token, "GET") == 0) {
        char *ch_str = strtok(NULL, " \t");
        if (!ch_str) return false;

        int ch = atoi(ch_str) - 1;
        if (ch < 0 || ch >= VOLTAGE_NUM_CH) return false;

        cmd->type    = CMD_GET;
        cmd->channel = (uint8_t)ch;
        return true;
    }

    /* ----- STATUS ----- */
    if (strcmp(token, "STATUS") == 0) {
        cmd->type = CMD_STATUS;
        return true;
    }

    /* ----- ENABLE <ch> <0|1> ----- */
    if (strcmp(token, "ENABLE") == 0) {
        char *ch_str  = strtok(NULL, " \t");
        char *val_str = strtok(NULL, " \t");
        if (!ch_str || !val_str) return false;

        int ch  = atoi(ch_str) - 1;
        int val = atoi(val_str);

        if (ch < 0 || ch >= VOLTAGE_NUM_CH) return false;

        cmd->type         = CMD_ENABLE;
        cmd->channel      = (uint8_t)ch;
        cmd->value        = (val != 0) ? 1 : 0;
        return true;
    }

    /* ----- RESET <ch|ALL> ----- */
    if (strcmp(token, "RESET") == 0) {
        char *arg = strtok(NULL, " \t");
        if (!arg) return false;

        if (strcmp(arg, "ALL") == 0) {
            cmd->type         = CMD_RESET;
            cmd->all_channels = true;
            return true;
        }

        int ch = atoi(arg) - 1;
        if (ch < 0 || ch >= VOLTAGE_NUM_CH) return false;

        cmd->type    = CMD_RESET;
        cmd->channel = (uint8_t)ch;
        return true;
    }

    /* ----- HELP ----- */
    if (strcmp(token, "HELP") == 0) {
        cmd->type = CMD_HELP;
        return true;
    }

    /* ----- VERSION ----- */
    if (strcmp(token, "VERSION") == 0) {
        cmd->type = CMD_VERSION;
        return true;
    }

    return false;
}

/* =========================================================
 * 응답 큐에 문자열 추가 (내부 공통 함수)
 * =========================================================*/
static void _enqueue_response(const char *str)
{
    RespQueueItem_t item;
    strncpy(item.data, str, sizeof(item.data) - 1);
    item.data[sizeof(item.data) - 1] = '\0';
    item.len = (uint16_t)strlen(item.data);

    /* 큐가 꽉 차도 블로킹하지 않음 (메시지 손실 허용) */
    osMessageQueuePut(xRespQueueHandle, &item, 0, 0);
}

/* =========================================================
 * OK 응답 전송
 * =========================================================*/
void Proto_SendOK(const char *msg)
{
    snprintf(tx_line_buf, sizeof(tx_line_buf), "OK %s\r\n", msg ? msg : "");
    _enqueue_response(tx_line_buf);
}

void Proto_SendOKf(const char *fmt, ...)
{
    char msg_buf[PROTO_TX_BUF_SIZE - 8];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    snprintf(tx_line_buf, sizeof(tx_line_buf), "OK %s\r\n", msg_buf);
    _enqueue_response(tx_line_buf);
}

/* =========================================================
 * 에러 응답 전송
 * =========================================================*/
void Proto_SendError(ErrCode_t err, const char *msg)
{
    static const char *err_strs[] = {
        "NONE", "SYNTAX_ERROR", "PARAM_RANGE",
        "INVALID_CHANNEL", "CHANNEL_FAULT", "TIMEOUT"
    };

    const char *err_str = (err < 6) ? err_strs[err] : "UNKNOWN";
    snprintf(tx_line_buf, sizeof(tx_line_buf),
             "ERR %d %s: %s\r\n", (int)err, err_str, msg ? msg : "");
    _enqueue_response(tx_line_buf);
}

/* =========================================================
 * 이벤트 알림 전송 (비동기, 폴트 발생 시 등)
 * =========================================================*/
void Proto_SendEvent(const char *type, const char *data)
{
    snprintf(tx_line_buf, sizeof(tx_line_buf),
             "EVT %s %s\r\n", type ? type : "UNKNOWN", data ? data : "");
    _enqueue_response(tx_line_buf);
}

/* =========================================================
 * 전체 채널 상태 출력
 * =========================================================*/
void Proto_SendStatus(VoltageCtrl_t *hctrl)
{
    char line[128];

    _enqueue_response("OK STATUS_BEGIN\r\n");

    for (int i = 0; i < VOLTAGE_NUM_CH; i++) {
        VoltageCh_t *ch = &hctrl->ch[i];

        char fault_str[32] = "OK";
        if (ch->fault_flags & CH_STATUS_SHORT)      strcpy(fault_str, "SHORT");
        else if (ch->fault_flags & CH_STATUS_OPEN)  strcpy(fault_str, "OPEN");
        else if (ch->fault_flags & CH_STATUS_OVERVOLTAGE) strcpy(fault_str, "OVERVOLT");
        else if (ch->fault_flags & CH_STATUS_DISABLED)    strcpy(fault_str, "DISABLED");

        snprintf(line, sizeof(line),
                 "CH%d TGT=%ldmV MEAS=%ldmV CURR=%ldmA PWM=%u FAULT=%s EN=%d\r\n",
                 i + 1,
                 (long)ch->target_mv,
                 (long)ch->measured_mv,
                 (long)ch->current_ma,
                 ch->pwm_duty,
                 fault_str,
                 ch->enabled ? 1 : 0);
        _enqueue_response(line);
    }

    _enqueue_response("OK STATUS_END\r\n");
}

/* =========================================================
 * 단일 채널 정보 출력
 * =========================================================*/
void Proto_SendChannelInfo(VoltageCtrl_t *hctrl, uint8_t ch)
{
    if (ch >= VOLTAGE_NUM_CH) {
        Proto_SendError(ERR_CH_INVALID, "channel out of range");
        return;
    }

    VoltageCh_t *c = &hctrl->ch[ch];

    Proto_SendOKf("CH%d TGT=%ldmV MEAS=%ldmV ERR=%.1fmV CURR=%ldmA PWM=%u",
                  ch + 1,
                  (long)c->target_mv,
                  (long)c->measured_mv,
                  c->error_mv,
                  (long)c->current_ma,
                  c->pwm_duty);
}

/* =========================================================
 * 도움말 출력
 * =========================================================*/
void Proto_SendHelp(void)
{
    const char *help[] = {
        "--- STM32L5 Voltage Controller Help ---\r\n",
        "SET <ch> <mv>   : Set target voltage (ch:1-4, mv:0-3300)\r\n",
        "GET <ch>        : Get channel measurement\r\n",
        "STATUS          : Get all channel status\r\n",
        "ENABLE <ch> <1|0>: Enable/disable channel\r\n",
        "RESET <ch|ALL>  : Reset channel fault\r\n",
        "VERSION         : Show firmware version\r\n",
        "HELP            : Show this help\r\n",
        "--- Response Format ---\r\n",
        "OK <data>       : Success response\r\n",
        "ERR <code> <msg>: Error response\r\n",
        "EVT <type> <data>: Async event notification\r\n",
        "---\r\n",
        NULL
    };

    for (int i = 0; help[i] != NULL; i++) {
        _enqueue_response(help[i]);
    }
}

/* =========================================================
 * 버전 정보 출력
 * =========================================================*/
void Proto_SendVersion(void)
{
    Proto_SendOKf("STM32L5 VoltageController v%d.%d.%d (%s %s)",
                  FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH,
                  FW_BUILD_DATE, FW_BUILD_TIME);
}
