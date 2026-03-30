/**
 ******************************************************************************
 * @file    uart_protocol.c
 * @brief   UART Command Parser and Protocol Handler Implementation
 ******************************************************************************
 */
#include "uart_protocol.h"
#include "app_config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ── Private TX buffer ────────────────────────────────────────────────── */
static char tx_buf[UART_TX_BUF_SIZE];

/* ── Initialization ───────────────────────────────────────────────────── */

HAL_StatusTypeDef UartProtocol_Init(UartProtocol_Handle_t *proto)
{
    proto->cmd_idx = 0;
    proto->line_ready = false;
    memset(proto->cmd_buf, 0, UART_CMD_MAX_LEN);
    memset(proto->rx_buf, 0, UART_RX_BUF_SIZE);

    /* Start UART reception in interrupt mode (1 byte at a time) */
    return HAL_UART_Receive_IT(proto->huart, proto->rx_buf, 1);
}

/* ── Byte Processing ──────────────────────────────────────────────────── */

bool UartProtocol_ProcessByte(UartProtocol_Handle_t *proto, uint8_t byte)
{
    if (byte == '\n' || byte == '\r') {
        if (proto->cmd_idx > 0) {
            proto->cmd_buf[proto->cmd_idx] = '\0';
            proto->line_ready = true;
            return true;
        }
        return false;
    }

    if (proto->cmd_idx < (UART_CMD_MAX_LEN - 1)) {
        proto->cmd_buf[proto->cmd_idx++] = byte;
    }

    return false;
}

/* ── Command Parser ───────────────────────────────────────────────────── */

uint8_t UartProtocol_Parse(UartProtocol_Handle_t *proto, UartCmd_t *cmd)
{
    char *buf = (char *)proto->cmd_buf;
    char *token;
    char tmp[UART_CMD_MAX_LEN];

    /* Reset command */
    memset(cmd, 0, sizeof(UartCmd_t));

    /* Check for start character */
    if (buf[0] != '$') return UART_ERR_INVALID_CMD;

    /* Copy for tokenizing (skip '$') */
    strncpy(tmp, &buf[1], UART_CMD_MAX_LEN - 1);
    tmp[UART_CMD_MAX_LEN - 1] = '\0';

    /* Get command token */
    token = strtok(tmp, ",");
    if (token == NULL) return UART_ERR_PARSE_FAIL;

    /* Parse command type */
    if (strcmp(token, "SV") == 0) {
        /* $SV,<ch>,<mV> */
        cmd->cmd = CMD_SET_VOLTAGE;

        token = strtok(NULL, ",");
        if (token == NULL) return UART_ERR_PARSE_FAIL;
        cmd->channel = (uint8_t)atoi(token);
        if (cmd->channel >= NUM_CHANNELS) return UART_ERR_INVALID_CHANNEL;

        token = strtok(NULL, ",");
        if (token == NULL) return UART_ERR_PARSE_FAIL;
        cmd->value = (uint16_t)atoi(token);
        if (cmd->value > VOLTAGE_MAX_MV) return UART_ERR_INVALID_VALUE;

    } else if (strcmp(token, "GV") == 0) {
        /* $GV,<ch> */
        cmd->cmd = CMD_GET_VOLTAGE;
        token = strtok(NULL, ",");
        if (token == NULL) return UART_ERR_PARSE_FAIL;
        cmd->channel = (uint8_t)atoi(token);
        if (cmd->channel >= NUM_CHANNELS) return UART_ERR_INVALID_CHANNEL;

    } else if (strcmp(token, "GA") == 0) {
        /* $GA */
        cmd->cmd = CMD_GET_ALL;

    } else if (strcmp(token, "GI") == 0) {
        /* $GI,<ch> */
        cmd->cmd = CMD_GET_CURRENT;
        token = strtok(NULL, ",");
        if (token == NULL) return UART_ERR_PARSE_FAIL;
        cmd->channel = (uint8_t)atoi(token);
        if (cmd->channel >= NUM_CHANNELS) return UART_ERR_INVALID_CHANNEL;

    } else if (strcmp(token, "GF") == 0) {
        /* $GF */
        cmd->cmd = CMD_GET_FAULT;

    } else if (strcmp(token, "EN") == 0) {
        /* $EN,<ch> */
        cmd->cmd = CMD_ENABLE_CH;
        token = strtok(NULL, ",");
        if (token == NULL) return UART_ERR_PARSE_FAIL;
        cmd->channel = (uint8_t)atoi(token);
        if (cmd->channel >= NUM_CHANNELS) return UART_ERR_INVALID_CHANNEL;

    } else if (strcmp(token, "DI") == 0) {
        /* $DI,<ch> */
        cmd->cmd = CMD_DISABLE_CH;
        token = strtok(NULL, ",");
        if (token == NULL) return UART_ERR_PARSE_FAIL;
        cmd->channel = (uint8_t)atoi(token);
        if (cmd->channel >= NUM_CHANNELS) return UART_ERR_INVALID_CHANNEL;

    } else if (strcmp(token, "SP") == 0) {
        /* $SP,<Kp>,<Ki> */
        cmd->cmd = CMD_SET_PID;
        token = strtok(NULL, ",");
        if (token == NULL) return UART_ERR_PARSE_FAIL;
        cmd->param1 = (float)atof(token);

        token = strtok(NULL, ",");
        if (token == NULL) return UART_ERR_PARSE_FAIL;
        cmd->param2 = (float)atof(token);

    } else if (strcmp(token, "ST") == 0) {
        /* $ST */
        cmd->cmd = CMD_SYSTEM_STATUS;

    } else {
        return UART_ERR_INVALID_CMD;
    }

    /* Reset for next command */
    proto->cmd_idx = 0;
    proto->line_ready = false;

    return UART_ERR_NONE;
}

/* ── Response Senders ─────────────────────────────────────────────────── */

void UartProtocol_SendString(UartProtocol_Handle_t *proto, const char *str)
{
    HAL_UART_Transmit(proto->huart, (uint8_t *)str, (uint16_t)strlen(str), 100);
}

void UartProtocol_SendOK(UartProtocol_Handle_t *proto, const char *msg)
{
    if (msg != NULL && msg[0] != '\0') {
        snprintf(tx_buf, sizeof(tx_buf), "#OK,%s\r\n", msg);
    } else {
        snprintf(tx_buf, sizeof(tx_buf), "#OK\r\n");
    }
    UartProtocol_SendString(proto, tx_buf);
}

void UartProtocol_SendError(UartProtocol_Handle_t *proto, uint8_t err_code)
{
    snprintf(tx_buf, sizeof(tx_buf), "#ERR,%u\r\n", err_code);
    UartProtocol_SendString(proto, tx_buf);
}

void UartProtocol_SendVoltageData(UartProtocol_Handle_t *proto,
                                  uint8_t ch, uint16_t target_mv,
                                  uint16_t measured_mv)
{
    snprintf(tx_buf, sizeof(tx_buf), "#VD,%u,%u,%u\r\n",
             ch, target_mv, measured_mv);
    UartProtocol_SendString(proto, tx_buf);
}

void UartProtocol_SendCurrentData(UartProtocol_Handle_t *proto,
                                  uint8_t ch, uint16_t current_ma)
{
    snprintf(tx_buf, sizeof(tx_buf), "#ID,%u,%u\r\n", ch, current_ma);
    UartProtocol_SendString(proto, tx_buf);
}

void UartProtocol_SendFaultData(UartProtocol_Handle_t *proto,
                                uint8_t ch, FaultType_t fault)
{
    const char *fault_str;
    switch (fault) {
    case FAULT_NONE:           fault_str = "NONE";    break;
    case FAULT_SHORT_CIRCUIT:  fault_str = "SHORT";   break;
    case FAULT_OPEN_CIRCUIT:   fault_str = "OPEN";    break;
    case FAULT_OVER_VOLTAGE:   fault_str = "OVERVOLT"; break;
    case FAULT_CLEARED:        fault_str = "CLEARED"; break;
    default:                   fault_str = "UNKNOWN"; break;
    }
    snprintf(tx_buf, sizeof(tx_buf), "#FD,%u,%s\r\n", ch, fault_str);
    UartProtocol_SendString(proto, tx_buf);
}

void UartProtocol_SendAllStatus(UartProtocol_Handle_t *proto,
                                ChannelCtrl_t channels[NUM_CHANNELS])
{
    for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
        const char *state_str;
        switch (channels[ch].state) {
        case CH_STATE_DISABLED:     state_str = "DIS";   break;
        case CH_STATE_ENABLED:      state_str = "EN";    break;
        case CH_STATE_FAULT_SHORT:  state_str = "SHORT"; break;
        case CH_STATE_FAULT_OPEN:   state_str = "OPEN";  break;
        default:                    state_str = "?";     break;
        }
        snprintf(tx_buf, sizeof(tx_buf), "#SD,%u,%u,%u,%u,%s\r\n",
                 ch,
                 channels[ch].target_mv,
                 channels[ch].measured_mv,
                 channels[ch].current_ma,
                 state_str);
        UartProtocol_SendString(proto, tx_buf);
    }
}
