/* =========================================================
 * UART 프로토콜 - Non-RTOS
 * TX: 링버퍼 → DMA (메인루프에서 Proto_TxPump() 호출)
 * RX: DMA 원형버퍼 + IDLE 인터럽트
 * =========================================================*/
#include "uart_protocol.h"
#include "current_monitor.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

/* ----- TX 링버퍼 전역 ----- */
TxQueue_t g_txq;

/* ----- RX 전역 ----- */
uint8_t  g_uart_rx_dma_buf[PROTO_RX_BUF_SIZE];
uint16_t g_uart_rx_prev_pos = 0;

/* ----- RX 라인 파싱 상태 ----- */
static char     rx_line_buf[PROTO_CMD_MAX_LEN];
static uint8_t  rx_line_idx = 0;

/* 명령 실행 콜백 (main.c에서 등록) */
static void (*cmd_handler)(ParsedCmd_t *) = NULL;

/* =========================================================
 * TX 링버퍼 내부 함수
 * =========================================================*/
static bool _txq_push(const char *str)
{
    if (g_txq.count >= TX_QUEUE_DEPTH) return false; /* 버퍼 풀 */
    strncpy(g_txq.buf[g_txq.head], str, TX_ITEM_SIZE - 1);
    g_txq.buf[g_txq.head][TX_ITEM_SIZE - 1] = '\0';
    g_txq.head = (g_txq.head + 1) % TX_QUEUE_DEPTH;
    g_txq.count++;
    return true;
}

/* =========================================================
 * TX DMA 구동 (메인 루프에서 반복 호출)
 * DMA가 비어있고 큐에 데이터가 있으면 전송 시작
 * =========================================================*/
void Proto_TxPump(void)
{
    if (g_txq.tx_busy) return;
    if (g_txq.count == 0) return;

    const char *item = g_txq.buf[g_txq.tail];
    uint16_t len = (uint16_t)strlen(item);
    if (len == 0) {
        /* 빈 항목 건너뜀 */
        g_txq.tail  = (g_txq.tail + 1) % TX_QUEUE_DEPTH;
        g_txq.count--;
        return;
    }

    g_txq.tx_busy = true;
    HAL_UART_Transmit_DMA(PROTO_UART, (uint8_t *)item, len);
}

/* DMA TX 완료 콜백 (stm32l5xx_it.c → HAL_UART_TxCpltCallback에서 호출) */
void Proto_TxDoneCallback(void)
{
    g_txq.tail    = (g_txq.tail + 1) % TX_QUEUE_DEPTH;
    g_txq.count--;
    g_txq.tx_busy = false;
    /* 다음 항목 즉시 전송 */
    Proto_TxPump();
}

/* =========================================================
 * 문자열 전송 (큐에 추가)
 * =========================================================*/
void Proto_Send(const char *str)
{
    _txq_push(str);
}

void Proto_Sendf(const char *fmt, ...)
{
    char buf[PROTO_TX_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    _txq_push(buf);
}

void Proto_SendOKf(const char *fmt, ...)
{
    char msg[PROTO_TX_BUF_SIZE - 8];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    char buf[PROTO_TX_BUF_SIZE];
    snprintf(buf, sizeof(buf), "OK %s\r\n", msg);
    _txq_push(buf);
}

void Proto_SendError(ErrCode_t err, const char *msg)
{
    static const char *es[] = {"NONE","SYNTAX","PARAM","BAD_CH","CH_FAULT"};
    const char *es_str = (err < 5) ? es[err] : "UNK";
    char buf[PROTO_TX_BUF_SIZE];
    snprintf(buf, sizeof(buf), "ERR %d %s: %s\r\n", (int)err, es_str, msg?msg:"");
    _txq_push(buf);
}

void Proto_SendEvent(const char *type, const char *data)
{
    char buf[PROTO_TX_BUF_SIZE];
    snprintf(buf, sizeof(buf), "EVT %s %s\r\n", type?type:"?", data?data:"");
    _txq_push(buf);
}

/* =========================================================
 * 명령 파싱
 * =========================================================*/
bool Proto_ParseCmd(const char *raw, ParsedCmd_t *out)
{
    if (!raw || !out) return false;

    char buf[PROTO_CMD_MAX_LEN];
    strncpy(buf, raw, PROTO_CMD_MAX_LEN - 1);
    buf[PROTO_CMD_MAX_LEN - 1] = '\0';

    /* 대문자 변환 + 개행 제거 */
    for (char *p = buf; *p; p++) {
        if (*p == '\r' || *p == '\n') { *p = '\0'; break; }
        *p = toupper((unsigned char)*p);
    }

    memset(out, 0, sizeof(ParsedCmd_t));
    out->type = CMD_UNKNOWN;

    char *tok = strtok(buf, " \t");
    if (!tok) return false;

    if (strcmp(tok, "SET") == 0) {
        char *cs = strtok(NULL, " \t"), *ms = strtok(NULL, " \t");
        if (!cs || !ms) return false;
        int ch = atoi(cs) - 1, mv = atoi(ms);
        if (ch < 0 || ch >= VOLTAGE_NUM_CH) return false;
        if (mv < 0  || mv > VOLTAGE_MAX_MV) return false;
        out->type = CMD_SET; out->channel = ch; out->value = mv;
        return true;
    }
    if (strcmp(tok, "GET") == 0) {
        char *cs = strtok(NULL, " \t");
        if (!cs) return false;
        int ch = atoi(cs) - 1;
        if (ch < 0 || ch >= VOLTAGE_NUM_CH) return false;
        out->type = CMD_GET; out->channel = ch;
        return true;
    }
    if (strcmp(tok, "STATUS")  == 0) { out->type = CMD_STATUS;  return true; }
    if (strcmp(tok, "HELP")    == 0) { out->type = CMD_HELP;    return true; }
    if (strcmp(tok, "VERSION") == 0) { out->type = CMD_VERSION; return true; }
    if (strcmp(tok, "ENABLE")  == 0) {
        char *cs = strtok(NULL, " \t"), *vs = strtok(NULL, " \t");
        if (!cs || !vs) return false;
        int ch = atoi(cs) - 1;
        if (ch < 0 || ch >= VOLTAGE_NUM_CH) return false;
        out->type = CMD_ENABLE; out->channel = ch; out->value = atoi(vs) ? 1 : 0;
        return true;
    }
    if (strcmp(tok, "RESET") == 0) {
        char *arg = strtok(NULL, " \t");
        if (!arg) return false;
        if (strcmp(arg, "ALL") == 0) {
            out->type = CMD_RESET; out->all_channels = true;
            return true;
        }
        int ch = atoi(arg) - 1;
        if (ch < 0 || ch >= VOLTAGE_NUM_CH) return false;
        out->type = CMD_RESET; out->channel = ch;
        return true;
    }
    return false;
}

/* =========================================================
 * 상태 출력
 * =========================================================*/
void Proto_SendStatus(VoltageCtrl_t *hctrl)
{
    char line[128];
    Proto_Send("OK STATUS_BEGIN\r\n");
    for (int i = 0; i < VOLTAGE_NUM_CH; i++) {
        VoltageCh_t *ch = &hctrl->ch[i];
        const char *fs = "OK";
        if      (ch->fault_flags & CH_STATUS_SHORT)    fs = "SHORT";
        else if (ch->fault_flags & CH_STATUS_OPEN)     fs = "OPEN";
        else if (ch->fault_flags & CH_STATUS_OVERVOLT) fs = "OVERVOLT";
        else if (ch->fault_flags & CH_STATUS_DISABLED) fs = "DISABLED";
        snprintf(line, sizeof(line),
                 "CH%d TGT=%ldmV MEAS=%ldmV CURR=%ldmA PWM=%u FAULT=%s EN=%d\r\n",
                 i+1, (long)ch->target_mv, (long)ch->measured_mv,
                 (long)ch->current_ma, ch->pwm_duty, fs, ch->enabled?1:0);
        Proto_Send(line);
    }
    Proto_Send("OK STATUS_END\r\n");
}

void Proto_SendChannelInfo(VoltageCtrl_t *hctrl, uint8_t ch)
{
    if (ch >= VOLTAGE_NUM_CH) { Proto_SendError(ERR_CH_INVALID, ""); return; }
    VoltageCh_t *c = &hctrl->ch[ch];
    Proto_SendOKf("CH%d TGT=%ldmV MEAS=%ldmV ERR=%.1fmV CURR=%ldmA PWM=%u",
                  ch+1, (long)c->target_mv, (long)c->measured_mv,
                  c->error_mv, (long)c->current_ma, c->pwm_duty);
}

void Proto_SendHelp(void)
{
    const char *h[] = {
        "--- STM32L5 VoltCtrl (NoRTOS) Help ---\r\n",
        "SET <ch> <mv>      : Set target voltage (ch:1-4, mv:0-3300)\r\n",
        "GET <ch>           : Get channel info\r\n",
        "STATUS             : All channels status\r\n",
        "ENABLE <ch> <1|0>  : Enable/disable channel\r\n",
        "RESET <ch|ALL>     : Reset fault\r\n",
        "VERSION            : Firmware version\r\n",
        "HELP               : This help\r\n",
        NULL
    };
    for (int i = 0; h[i]; i++) Proto_Send(h[i]);
}

void Proto_SendVersion(void)
{
    Proto_SendOKf("STM32L5 VoltCtrl %s (%s %s)", FW_VERSION, __DATE__, __TIME__);
}

/* =========================================================
 * RX DMA 버퍼 처리 (메인루프에서 g_flags.uart_rx_ready 감지 후 호출)
 * new_pos: DMA 카운터로 계산한 현재 쓰기 위치
 * =========================================================*/
void Proto_RxProcess(uint16_t new_pos)
{
    extern VoltageCtrl_t  g_vctrl;
    extern CurrentMonitor_t g_currmon;

    uint16_t start = g_uart_rx_prev_pos;
    uint16_t end   = new_pos;

    while (start != end) {
        uint8_t c = g_uart_rx_dma_buf[start];
        start = (start + 1) % PROTO_RX_BUF_SIZE;

        if (c == '\n' || c == '\r') {
            if (rx_line_idx > 0) {
                rx_line_buf[rx_line_idx] = '\0';
                rx_line_idx = 0;

                /* 명령 파싱 및 즉시 실행 */
                ParsedCmd_t cmd;
                if (Proto_ParseCmd(rx_line_buf, &cmd)) {
                    switch (cmd.type) {
                        case CMD_SET:
                            VoltageCtrl_SetTarget(&g_vctrl, cmd.channel, cmd.value);
                            Proto_SendOKf("CH%d target=%ldmV", cmd.channel+1, (long)cmd.value);
                            break;
                        case CMD_GET:
                            Proto_SendChannelInfo(&g_vctrl, cmd.channel);
                            break;
                        case CMD_STATUS:
                            Proto_SendStatus(&g_vctrl);
                            break;
                        case CMD_ENABLE:
                            VoltageCtrl_Enable(&g_vctrl, cmd.channel, (bool)cmd.value);
                            Proto_SendOKf("CH%d %s", cmd.channel+1, cmd.value?"enabled":"disabled");
                            break;
                        case CMD_RESET:
                            if (cmd.all_channels) {
                                for (int i = 0; i < VOLTAGE_NUM_CH; i++) {
                                    VoltageCtrl_Reset(&g_vctrl, i);
                                    g_currmon.ch[i].fault = FAULT_NONE;
                                    g_currmon.ch[i].fault_confirm = 0;
                                }
                                Proto_Send("OK ALL channels reset\r\n");
                            } else {
                                VoltageCtrl_Reset(&g_vctrl, cmd.channel);
                                g_currmon.ch[cmd.channel].fault = FAULT_NONE;
                                Proto_SendOKf("CH%d reset", cmd.channel+1);
                            }
                            break;
                        case CMD_HELP:    Proto_SendHelp();    break;
                        case CMD_VERSION: Proto_SendVersion(); break;
                        default: Proto_SendError(ERR_SYNTAX, rx_line_buf); break;
                    }
                } else {
                    Proto_SendError(ERR_SYNTAX, rx_line_buf);
                }
            }
        } else if (rx_line_idx < PROTO_CMD_MAX_LEN - 1) {
            rx_line_buf[rx_line_idx++] = (char)c;
        }
    }

    g_uart_rx_prev_pos = new_pos;
}

/* =========================================================
 * 초기화 (main.c에서 호출)
 * =========================================================*/
void Proto_Init(void)
{
    memset(&g_txq, 0, sizeof(g_txq));
    /* DMA 원형 수신 시작 */
    HAL_UARTEx_ReceiveToIdle_DMA(PROTO_UART, g_uart_rx_dma_buf, PROTO_RX_BUF_SIZE);
    Proto_SendVersion();
    Proto_SendHelp();
    Proto_TxPump(); /* 첫 전송 시작 */
}
