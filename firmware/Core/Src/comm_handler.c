#include "comm_handler.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

void Comm_Init(Comm_Handle_t *hcomm, UART_HandleTypeDef *huart,
               FDCAN_HandleTypeDef *hfdcan, VoltCtrl_Handle_t *hctrl)
{
    hcomm->huart = huart;
    hcomm->hfdcan = hfdcan;
    hcomm->hctrl = hctrl;
    hcomm->rx_index = 0;
    memset(hcomm->rx_buffer, 0, sizeof(hcomm->rx_buffer));
}

void Comm_StartReception(Comm_Handle_t *hcomm)
{
    HAL_UART_Receive_IT(hcomm->huart, &hcomm->rx_byte, 1);
}

void Comm_UART_RxCallback(Comm_Handle_t *hcomm)
{
    if (hcomm->rx_byte == '\n' || hcomm->rx_byte == '\r') {
        if (hcomm->rx_index > 0) {
            hcomm->rx_buffer[hcomm->rx_index] = '\0';
            Comm_ProcessCommand(hcomm, hcomm->rx_buffer);
            hcomm->rx_index = 0;
        }
    } else {
        if (hcomm->rx_index < COMM_UART_RX_BUF_SIZE - 1) {
            hcomm->rx_buffer[hcomm->rx_index++] = (char)hcomm->rx_byte;
        }
    }

    /* Re-enable reception for next byte */
    HAL_UART_Receive_IT(hcomm->huart, &hcomm->rx_byte, 1);
}

void Comm_UartSend(Comm_Handle_t *hcomm, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(hcomm->tx_buffer, COMM_UART_TX_BUF_SIZE, fmt, args);
    va_end(args);

    if (len > 0) {
        HAL_UART_Transmit(hcomm->huart, (uint8_t *)hcomm->tx_buffer, (uint16_t)len, 100);
    }
}

void Comm_ProcessCommand(Comm_Handle_t *hcomm, const char *cmd)
{
    char cmd_type[16] = {0};
    int ch = 0;
    float voltage = 0.0f;
    int enable = 0;

    /* Parse command type */
    if (sscanf(cmd, "%15s", cmd_type) < 1) {
        Comm_UartSend(hcomm, "ERR INVALID_CMD\r\n");
        return;
    }

    /* SET <ch> <voltage> */
    if (strcmp(cmd_type, "SET") == 0) {
        if (sscanf(cmd, "SET %d %f", &ch, &voltage) == 2) {
            if (ch < 0 || ch >= VCTRL_NUM_CHANNELS) {
                Comm_UartSend(hcomm, "ERR INVALID_CH %d\r\n", ch);
                return;
            }
            if (voltage < 0.0f || voltage > 5.0f) {
                Comm_UartSend(hcomm, "ERR INVALID_VOLTAGE %.3f\r\n", voltage);
                return;
            }

            HAL_StatusTypeDef st = VoltCtrl_SetTarget(hcomm->hctrl, (uint8_t)ch, voltage);
            if (st == HAL_OK) {
                Comm_UartSend(hcomm, "OK SET %d %.3f\r\n", ch, voltage);
            } else {
                Comm_UartSend(hcomm, "ERR SET_FAIL %d\r\n", ch);
            }
        } else {
            Comm_UartSend(hcomm, "ERR SET_SYNTAX\r\n");
        }
    }
    /* GET <ch> */
    else if (strcmp(cmd_type, "GET") == 0) {
        if (sscanf(cmd, "GET %d", &ch) == 1) {
            if (ch < 0 || ch >= VCTRL_NUM_CHANNELS) {
                Comm_UartSend(hcomm, "ERR INVALID_CH %d\r\n", ch);
                return;
            }
            ChannelData_t data;
            VoltCtrl_GetChannelData(hcomm->hctrl, (uint8_t)ch, &data);

            const char *state_str;
            switch (data.state) {
                case CH_STATE_OK:            state_str = "OK"; break;
                case CH_STATE_SHORT_CIRCUIT:  state_str = "SHORT"; break;
                case CH_STATE_OPEN_CIRCUIT:   state_str = "OPEN"; break;
                case CH_STATE_DISABLED:       state_str = "OFF"; break;
                case CH_STATE_CALIBRATING:    state_str = "CAL"; break;
                default:                      state_str = "UNK"; break;
            }

            Comm_UartSend(hcomm, "DATA %d %.3f %.3f %.2f %s\r\n",
                          ch, data.target_voltage, data.actual_voltage,
                          data.current_mA, state_str);
        } else {
            Comm_UartSend(hcomm, "ERR GET_SYNTAX\r\n");
        }
    }
    /* GETALL */
    else if (strcmp(cmd_type, "GETALL") == 0) {
        ChannelData_t data[VCTRL_NUM_CHANNELS];
        VoltCtrl_GetAllChannelData(hcomm->hctrl, data);

        for (int i = 0; i < VCTRL_NUM_CHANNELS; i++) {
            const char *state_str;
            switch (data[i].state) {
                case CH_STATE_OK:            state_str = "OK"; break;
                case CH_STATE_SHORT_CIRCUIT:  state_str = "SHORT"; break;
                case CH_STATE_OPEN_CIRCUIT:   state_str = "OPEN"; break;
                case CH_STATE_DISABLED:       state_str = "OFF"; break;
                case CH_STATE_CALIBRATING:    state_str = "CAL"; break;
                default:                      state_str = "UNK"; break;
            }
            Comm_UartSend(hcomm, "DATA %d %.3f %.3f %.2f %s\r\n",
                          i, data[i].target_voltage, data[i].actual_voltage,
                          data[i].current_mA, state_str);
        }
    }
    /* EN <ch> <0|1> */
    else if (strcmp(cmd_type, "EN") == 0) {
        if (sscanf(cmd, "EN %d %d", &ch, &enable) == 2) {
            if (ch < 0 || ch >= VCTRL_NUM_CHANNELS) {
                Comm_UartSend(hcomm, "ERR INVALID_CH %d\r\n", ch);
                return;
            }
            VoltCtrl_EnableChannel(hcomm->hctrl, (uint8_t)ch, (bool)enable);
            Comm_UartSend(hcomm, "OK EN %d %d\r\n", ch, enable);
        } else {
            Comm_UartSend(hcomm, "ERR EN_SYNTAX\r\n");
        }
    }
    /* STATUS */
    else if (strcmp(cmd_type, "STATUS") == 0) {
        Comm_UartSend(hcomm, "STATUS RUNNING\r\n");
        ChannelData_t data[VCTRL_NUM_CHANNELS];
        VoltCtrl_GetAllChannelData(hcomm->hctrl, data);
        Comm_SendUartReport(hcomm);
    }
    else {
        Comm_UartSend(hcomm, "ERR UNKNOWN_CMD %s\r\n", cmd_type);
    }
}

void Comm_SendUartReport(Comm_Handle_t *hcomm)
{
    ChannelData_t data[VCTRL_NUM_CHANNELS];
    VoltCtrl_GetAllChannelData(hcomm->hctrl, data);

    Comm_UartSend(hcomm, "REPORT %.3f %.2f %.3f %.2f %.3f %.2f %.3f %.2f\r\n",
                  data[0].actual_voltage, data[0].current_mA,
                  data[1].actual_voltage, data[1].current_mA,
                  data[2].actual_voltage, data[2].current_mA,
                  data[3].actual_voltage, data[3].current_mA);
}

void Comm_SendCanReport(Comm_Handle_t *hcomm)
{
    ChannelData_t data[VCTRL_NUM_CHANNELS];
    VoltCtrl_GetAllChannelData(hcomm->hctrl, data);

    FDCAN_TxHeaderTypeDef tx_header;
    uint8_t tx_data[8];

    tx_header.IdType = FDCAN_STANDARD_ID;
    tx_header.TxFrameType = FDCAN_DATA_FRAME;
    tx_header.DataLength = FDCAN_DLC_BYTES_8;
    tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    tx_header.FDFormat = FDCAN_CLASSIC_CAN;
    tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    tx_header.MessageMarker = 0;

    /* Message 1: CH0 + CH1 */
    tx_header.Identifier = CAN_ID_REPORT_CH01;
    uint16_t v0_mv = (uint16_t)(data[0].actual_voltage * 1000.0f);
    uint16_t i0_01mA = (uint16_t)(data[0].current_mA * 10.0f);
    uint16_t v1_mv = (uint16_t)(data[1].actual_voltage * 1000.0f);
    uint16_t i1_01mA = (uint16_t)(data[1].current_mA * 10.0f);

    tx_data[0] = (uint8_t)(v0_mv >> 8);
    tx_data[1] = (uint8_t)(v0_mv & 0xFF);
    tx_data[2] = (uint8_t)(i0_01mA >> 8);
    tx_data[3] = (uint8_t)(i0_01mA & 0xFF);
    tx_data[4] = (uint8_t)(v1_mv >> 8);
    tx_data[5] = (uint8_t)(v1_mv & 0xFF);
    tx_data[6] = (uint8_t)(i1_01mA >> 8);
    tx_data[7] = (uint8_t)(i1_01mA & 0xFF);

    HAL_FDCAN_AddMessageToTxFifoQ(hcomm->hfdcan, &tx_header, tx_data);

    /* Message 2: CH2 + CH3 */
    tx_header.Identifier = CAN_ID_REPORT_CH23;
    uint16_t v2_mv = (uint16_t)(data[2].actual_voltage * 1000.0f);
    uint16_t i2_01mA = (uint16_t)(data[2].current_mA * 10.0f);
    uint16_t v3_mv = (uint16_t)(data[3].actual_voltage * 1000.0f);
    uint16_t i3_01mA = (uint16_t)(data[3].current_mA * 10.0f);

    tx_data[0] = (uint8_t)(v2_mv >> 8);
    tx_data[1] = (uint8_t)(v2_mv & 0xFF);
    tx_data[2] = (uint8_t)(i2_01mA >> 8);
    tx_data[3] = (uint8_t)(i2_01mA & 0xFF);
    tx_data[4] = (uint8_t)(v3_mv >> 8);
    tx_data[5] = (uint8_t)(v3_mv & 0xFF);
    tx_data[6] = (uint8_t)(i3_01mA >> 8);
    tx_data[7] = (uint8_t)(i3_01mA & 0xFF);

    HAL_FDCAN_AddMessageToTxFifoQ(hcomm->hfdcan, &tx_header, tx_data);
}

void Comm_SendFaultAlert(Comm_Handle_t *hcomm, uint8_t ch, ChannelState_t state)
{
    /* UART fault alert */
    const char *fault_str = (state == CH_STATE_SHORT_CIRCUIT) ? "SHORT" : "OPEN";
    Comm_UartSend(hcomm, "FAULT %d %s\r\n", ch, fault_str);

    /* FDCAN fault alert */
    FDCAN_TxHeaderTypeDef tx_header;
    uint8_t tx_data[8] = {0};

    tx_header.Identifier = CAN_ID_FAULT;
    tx_header.IdType = FDCAN_STANDARD_ID;
    tx_header.TxFrameType = FDCAN_DATA_FRAME;
    tx_header.DataLength = FDCAN_DLC_BYTES_8;
    tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    tx_header.FDFormat = FDCAN_CLASSIC_CAN;
    tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    tx_header.MessageMarker = 0;

    tx_data[0] = ch;
    tx_data[1] = (uint8_t)state;

    ChannelData_t data;
    VoltCtrl_GetChannelData(hcomm->hctrl, ch, &data);
    uint16_t v_mv = (uint16_t)(data.actual_voltage * 1000.0f);
    uint16_t i_01mA = (uint16_t)(data.current_mA * 10.0f);

    tx_data[2] = (uint8_t)(v_mv >> 8);
    tx_data[3] = (uint8_t)(v_mv & 0xFF);
    tx_data[4] = (uint8_t)(i_01mA >> 8);
    tx_data[5] = (uint8_t)(i_01mA & 0xFF);

    HAL_FDCAN_AddMessageToTxFifoQ(hcomm->hfdcan, &tx_header, tx_data);
}
