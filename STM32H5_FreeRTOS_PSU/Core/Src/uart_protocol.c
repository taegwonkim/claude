/**
 * @file    uart_protocol.c
 * @brief   UART ASCII 프로토콜 구현
 */

#include "uart_protocol.h"
#include "channel_ctrl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* =========================================================
 * 내부 버퍼
 * ========================================================= */
static char     s_rx_line[PROTO_RX_BUF_SIZE];
static uint16_t s_rx_pos = 0U;

/* UART 수신 단일 바이트 버퍼 (인터럽트 수신용) */
static volatile uint8_t s_rx_byte;

/* =========================================================
 * 내부 헬퍼: 문자열 앞뒤 공백 제거 (in-place)
 * ========================================================= */
static void trim(char *str)
{
    if (str == NULL) { return; }

    /* 앞 공백 제거 */
    char *start = str;
    while (*start && isspace((unsigned char)*start)) { start++; }
    if (start != str) {
        memmove(str, start, strlen(start) + 1U);
    }

    /* 뒤 공백 제거 */
    size_t len = strlen(str);
    while (len > 0U && isspace((unsigned char)str[len - 1U])) {
        str[--len] = '\0';
    }
}

/* =========================================================
 * 내부 헬퍼: 문자열을 대문자로 변환
 * ========================================================= */
static void to_upper(char *str)
{
    for (; *str; str++) {
        *str = (char)toupper((unsigned char)*str);
    }
}

/* =========================================================
 * UART 전송 (블로킹, 짧은 문자열용)
 * TX 큐를 경유하지 않고 직접 전송
 * ========================================================= */
static void uart_send_blocking(const char *str)
{
    HAL_UART_Transmit(&huart3, (const uint8_t *)str, (uint16_t)strlen(str),
                      100U);
}

/* =========================================================
 * TX 큐에 메시지 푸시 (UART TX Task가 전송)
 * ========================================================= */
static void push_to_tx_queue(const char *str)
{
    TxMessage_t msg;
    strncpy(msg.msg, str, TX_MSG_MAX_LEN - 1U);
    msg.msg[TX_MSG_MAX_LEN - 1U] = '\0';
    /* 큐가 가득 차면 대기하지 않고 버림 */
    xQueueSend(xTxQueue, &msg, 0U);
}

/* =========================================================
 * 공개 함수 구현
 * ========================================================= */

void UartProto_Init(void)
{
    s_rx_pos = 0U;
    memset(s_rx_line, 0, sizeof(s_rx_line));
    /* UART 인터럽트 수신 시작 */
    HAL_UART_Receive_IT(&huart3, (uint8_t *)&s_rx_byte, 1U);
}

/* =========================================================
 * UART RX 인터럽트 콜백 (HAL weak 재정의)
 * - 1바이트씩 수신, '\n'에서 명령 파싱
 * ========================================================= */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART3) {
        return;
    }

    uint8_t byte = s_rx_byte;

    if (byte == '\r') {
        /* '\r' 무시 (Windows '\r\n' 처리) */
    } else if (byte == '\n') {
        /* 라인 완성 → 파싱 */
        s_rx_line[s_rx_pos] = '\0';
        if (s_rx_pos > 0U) {
            UartProto_RxByte(byte);
        }
        s_rx_pos = 0U;
    } else {
        if (s_rx_pos < (PROTO_RX_BUF_SIZE - 1U)) {
            s_rx_line[s_rx_pos++] = (char)byte;
        }
    }

    /* 다음 바이트 수신 대기 */
    HAL_UART_Receive_IT(&huart3, (uint8_t *)&s_rx_byte, 1U);
}

void UartProto_RxByte(uint8_t byte)
{
    (void)byte; /* '\n' 수신 시 호출됨 */

    Command_t cmd;
    if (UartProto_ParseLine(s_rx_line, &cmd) == 0) {
        /* 명령 큐에 전송 (UART RX는 인터럽트 컨텍스트이므로 FromISR 사용) */
        BaseType_t woken = pdFALSE;
        xQueueSendFromISR(xCmdQueue, &cmd, &woken);
        portYIELD_FROM_ISR(woken);
    } else {
        /* 파싱 실패 응답 (인터럽트 컨텍스트이므로 블로킹 전송) */
        uart_send_blocking("ERR:UNKNOWN_CMD\r\n");
    }
    s_rx_pos = 0U;
}

/* =========================================================
 * 명령 파싱
 * 형식: <COMMAND> [<ch>] [<value>]
 * ========================================================= */
int8_t UartProto_ParseLine(const char *line, Command_t *cmd)
{
    if (line == NULL || cmd == NULL) {
        return -1;
    }

    /* 복사본 작업 (strtok는 문자열 수정) */
    char buf[PROTO_RX_BUF_SIZE];
    strncpy(buf, line, PROTO_RX_BUF_SIZE - 1U);
    buf[PROTO_RX_BUF_SIZE - 1U] = '\0';
    trim(buf);
    to_upper(buf);

    if (strlen(buf) == 0U) {
        return -1;
    }

    char *tok = strtok(buf, " \t");
    if (tok == NULL) {
        return -1;
    }

    memset(cmd, 0, sizeof(Command_t));

    /* ---- SET <ch> <mV> ---- */
    if (strcmp(tok, "SET") == 0) {
        char *ch_str  = strtok(NULL, " \t");
        char *val_str = strtok(NULL, " \t");
        if (ch_str == NULL || val_str == NULL) { return -1; }
        cmd->type    = CMD_SET_VOLTAGE;
        cmd->channel = (uint8_t)atoi(ch_str);
        cmd->value   = (float)atof(val_str);
        if (cmd->channel < 1U || cmd->channel > NUM_CHANNELS) { return -1; }
    }
    /* ---- GET <ch|ALL> ---- */
    else if (strcmp(tok, "GET") == 0) {
        char *ch_str = strtok(NULL, " \t");
        if (ch_str == NULL) { return -1; }
        if (strcmp(ch_str, "ALL") == 0) {
            cmd->type    = CMD_GET_ALL;
            cmd->channel = 0U;
        } else {
            cmd->type    = CMD_GET_STATUS;
            cmd->channel = (uint8_t)atoi(ch_str);
            if (cmd->channel < 1U || cmd->channel > NUM_CHANNELS) { return -1; }
        }
    }
    /* ---- EN <ch> ---- */
    else if (strcmp(tok, "EN") == 0) {
        char *ch_str = strtok(NULL, " \t");
        if (ch_str == NULL) { return -1; }
        cmd->type    = CMD_ENABLE;
        cmd->channel = (uint8_t)atoi(ch_str);
        if (cmd->channel < 1U || cmd->channel > NUM_CHANNELS) { return -1; }
    }
    /* ---- DIS <ch> ---- */
    else if (strcmp(tok, "DIS") == 0) {
        char *ch_str = strtok(NULL, " \t");
        if (ch_str == NULL) { return -1; }
        cmd->type    = CMD_DISABLE;
        cmd->channel = (uint8_t)atoi(ch_str);
        if (cmd->channel < 1U || cmd->channel > NUM_CHANNELS) { return -1; }
    }
    /* ---- KP <ch> <val> ---- */
    else if (strcmp(tok, "KP") == 0) {
        char *ch_str  = strtok(NULL, " \t");
        char *val_str = strtok(NULL, " \t");
        if (ch_str == NULL || val_str == NULL) { return -1; }
        cmd->type    = CMD_SET_KP;
        cmd->channel = (uint8_t)atoi(ch_str);
        cmd->value   = (float)atof(val_str);
        if (cmd->channel < 1U || cmd->channel > NUM_CHANNELS) { return -1; }
    }
    /* ---- KI <ch> <val> ---- */
    else if (strcmp(tok, "KI") == 0) {
        char *ch_str  = strtok(NULL, " \t");
        char *val_str = strtok(NULL, " \t");
        if (ch_str == NULL || val_str == NULL) { return -1; }
        cmd->type    = CMD_SET_KI;
        cmd->channel = (uint8_t)atoi(ch_str);
        cmd->value   = (float)atof(val_str);
        if (cmd->channel < 1U || cmd->channel > NUM_CHANNELS) { return -1; }
    }
    /* ---- KD <ch> <val> ---- */
    else if (strcmp(tok, "KD") == 0) {
        char *ch_str  = strtok(NULL, " \t");
        char *val_str = strtok(NULL, " \t");
        if (ch_str == NULL || val_str == NULL) { return -1; }
        cmd->type    = CMD_SET_KD;
        cmd->channel = (uint8_t)atoi(ch_str);
        cmd->value   = (float)atof(val_str);
        if (cmd->channel < 1U || cmd->channel > NUM_CHANNELS) { return -1; }
    }
    /* ---- ILIM <ch> <mA> ---- */
    else if (strcmp(tok, "ILIM") == 0) {
        char *ch_str  = strtok(NULL, " \t");
        char *val_str = strtok(NULL, " \t");
        if (ch_str == NULL || val_str == NULL) { return -1; }
        cmd->type    = CMD_SET_ILIMIT;
        cmd->channel = (uint8_t)atoi(ch_str);
        cmd->value   = (float)atof(val_str);
        if (cmd->channel < 1U || cmd->channel > NUM_CHANNELS) { return -1; }
    }
    /* ---- HELP ---- */
    else if (strcmp(tok, "HELP") == 0) {
        UartProto_SendHelp();
        return -1;  /* 큐 전송 불필요 */
    }
    else {
        return -1;
    }

    return 0;
}

/* =========================================================
 * 응답 전송 함수
 * ========================================================= */

void UartProto_SendOk(void)
{
    push_to_tx_queue("OK\r\n");
}

void UartProto_SendError(const char *reason)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "ERR:%s\r\n", reason);
    push_to_tx_queue(buf);
}

void UartProto_SendChannelData(uint8_t ch_idx)
{
    if (ch_idx >= NUM_CHANNELS) { return; }

    xSemaphoreTake(xChannelMutex, portMAX_DELAY);
    Channel_t *ch = &g_channels[ch_idx];

    char buf[TX_MSG_MAX_LEN];
    snprintf(buf, sizeof(buf),
             "DATA:%u,V=%.1f,I=%.1f,ST=%s\r\n",
             ch->id,
             (double)ch->v_meas_mv,
             (double)ch->i_meas_ma,
             Channel_GetStatusStr(ch->status));

    xSemaphoreGive(xChannelMutex);
    push_to_tx_queue(buf);
}

void UartProto_SendAllData(void)
{
    for (uint8_t i = 0U; i < NUM_CHANNELS; i++) {
        UartProto_SendChannelData(i);
    }
}

void UartProto_SendAlert(uint8_t ch_idx, const char *alert_str)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "ALERT:%u,%s\r\n",
             ch_idx + 1U, alert_str);
    /* 경보는 TX 큐 없이 직접 전송 (우선순위) */
    uart_send_blocking(buf);
}

void UartProto_SendHelp(void)
{
    const char *help =
        "=== STM32H5 4CH PSU Commands ===\r\n"
        " SET <ch> <mV>    : Set channel voltage (ch=1-4, mV=0-3300)\r\n"
        " GET <ch>         : Get channel status\r\n"
        " GET ALL          : Get all channels status\r\n"
        " EN  <ch>         : Enable channel output\r\n"
        " DIS <ch>         : Disable channel output\r\n"
        " KP  <ch> <val>   : Set PID Kp\r\n"
        " KI  <ch> <val>   : Set PID Ki\r\n"
        " KD  <ch> <val>   : Set PID Kd\r\n"
        " ILIM <ch> <mA>   : Set current limit (mA)\r\n"
        " HELP             : Show this help\r\n"
        "=================================\r\n";
    uart_send_blocking(help);
}

void UartProto_SendStr(const char *str)
{
    push_to_tx_queue(str);
}
