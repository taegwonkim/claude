/**
  ******************************************************************************
  * @file           : comm_protocol.c
  * @brief          : UART/FDCAN communication protocol implementation
  ******************************************************************************
  */

#include "comm_protocol.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

/* External handles ----------------------------------------------------------*/
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart4;
extern FDCAN_HandleTypeDef hfdcan1;
extern osMutexId_t uart1MutexHandle;

/* Private variables ---------------------------------------------------------*/
static char tx_buffer[UART_TX_BUF_SIZE];
static char debug_buffer[128];

/* FDCAN TX header */
static FDCAN_TxHeaderTypeDef fdcan_tx_header;

/* Public functions ----------------------------------------------------------*/

void Comm_Init(void)
{
    /* Initialize FDCAN TX header defaults */
    fdcan_tx_header.IdType        = FDCAN_STANDARD_ID;
    fdcan_tx_header.TxFrameType   = FDCAN_DATA_FRAME;
    fdcan_tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    fdcan_tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    fdcan_tx_header.FDFormat      = FDCAN_CLASSIC_CAN;
    fdcan_tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    fdcan_tx_header.MessageMarker = 0;
}

uint8_t Comm_ParseCommand(const char *cmd_str, VoltageCmd_t *cmd)
{
    if (cmd_str == NULL || cmd == NULL) return 0;

    /* Skip leading whitespace */
    while (*cmd_str == ' ' || *cmd_str == '\t') cmd_str++;

    /* Parse "SET <ch> <voltage>" command */
    if (strncmp(cmd_str, "SET ", 4) == 0) {
        int ch;
        float voltage;
        if (sscanf(cmd_str + 4, "%d %f", &ch, &voltage) == 2) {
            if (ch >= 0 && ch < NUM_CHANNELS &&
                voltage >= VOLTAGE_MIN && voltage <= VOLTAGE_MAX) {
                cmd->channel = (uint8_t)ch;
                cmd->voltage = voltage;
                return 1;
            }
        }
    }
    /* Parse "GET <ch>" or "GET ALL" - return special marker */
    else if (strncmp(cmd_str, "GET ", 4) == 0) {
        if (strncmp(cmd_str + 4, "ALL", 3) == 0) {
            cmd->channel = 0xFF;  /* Special: all channels */
            cmd->voltage = -1.0f; /* Marker for GET command */
            return 1;
        }
        int ch;
        if (sscanf(cmd_str + 4, "%d", &ch) == 1) {
            if (ch >= 0 && ch < NUM_CHANNELS) {
                cmd->channel = (uint8_t)ch;
                cmd->voltage = -1.0f; /* Marker for GET command */
                return 1;
            }
        }
    }

    return 0;  /* Parse failed */
}

const char* Comm_StatusToString(ChannelStatus_t status)
{
    switch (status) {
        case CH_STATUS_OK:        return "OK";
        case CH_STATUS_SHORT:     return "SHORT";
        case CH_STATUS_OPEN:      return "OPEN";
        case CH_STATUS_OVERRANGE: return "OVERRANGE";
        case CH_STATUS_ERROR:     return "ERROR";
        default:                  return "UNKNOWN";
    }
}

void Comm_SendChannelResponse(uint8_t channel, const ChannelData_t *data)
{
    if (data == NULL) return;

    int len = snprintf(tx_buffer, sizeof(tx_buffer),
        "CH%d: T=%.3fV V=%.3fV I=%.1fmA S=%s DAC=%u\r\n",
        channel,
        data->target_voltage,
        data->actual_voltage,
        data->current_ma,
        Comm_StatusToString(data->status),
        data->dac_code);

    if (len > 0) {
        osMutexAcquire(uart1MutexHandle, osWaitForever);
        HAL_UART_Transmit(&huart1, (uint8_t *)tx_buffer, (uint16_t)len, 100);
        osMutexRelease(uart1MutexHandle);
    }
}

void Comm_SendReportUART(const ReportData_t *report)
{
    if (report == NULL) return;

    osMutexAcquire(uart1MutexHandle, osWaitForever);

    int len;

    /* Header */
    len = snprintf(tx_buffer, sizeof(tx_buffer), "RPT %lu\r\n",
                   (unsigned long)report->timestamp_ms);
    HAL_UART_Transmit(&huart1, (uint8_t *)tx_buffer, (uint16_t)len, 100);

    /* Each channel */
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        len = snprintf(tx_buffer, sizeof(tx_buffer),
            "CH%d: T=%.3fV V=%.3fV I=%.1fmA S=%s\r\n",
            i,
            report->channels[i].target_voltage,
            report->channels[i].actual_voltage,
            report->channels[i].current_ma,
            Comm_StatusToString(report->channels[i].status));

        HAL_UART_Transmit(&huart1, (uint8_t *)tx_buffer, (uint16_t)len, 100);
    }

    /* Footer */
    len = snprintf(tx_buffer, sizeof(tx_buffer), "END\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t *)tx_buffer, (uint16_t)len, 100);

    osMutexRelease(uart1MutexHandle);
}

void Comm_SendReportFDCAN(const ReportData_t *report)
{
    if (report == NULL) return;

    uint8_t can_data[8];

    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {

        /* --- Voltage message: CAN ID 0x100 + ch --- */
        fdcan_tx_header.Identifier = FDCAN_ID_VOLTAGE_BASE + i;
        fdcan_tx_header.DataLength = FDCAN_DLC_BYTES_8;

        /* Data[0..3]: target voltage (float, little-endian) */
        memcpy(&can_data[0], &report->channels[i].target_voltage, sizeof(float));
        /* Data[4..7]: actual voltage (float, little-endian) */
        memcpy(&can_data[4], &report->channels[i].actual_voltage, sizeof(float));

        /* Wait for free TX mailbox, with timeout */
        uint32_t start = HAL_GetTick();
        while (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) == 0) {
            if ((HAL_GetTick() - start) > 10) break;
            osDelay(1);
        }
        HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &fdcan_tx_header, can_data);

        /* --- Current/Status message: CAN ID 0x110 + ch --- */
        fdcan_tx_header.Identifier = FDCAN_ID_CURRENT_BASE + i;
        fdcan_tx_header.DataLength = FDCAN_DLC_BYTES_8;

        /* Data[0..3]: current in mA (float, little-endian) */
        memcpy(&can_data[0], &report->channels[i].current_ma, sizeof(float));
        /* Data[4]: status byte */
        can_data[4] = (uint8_t)report->channels[i].status;
        /* Data[5..6]: DAC code */
        can_data[5] = (uint8_t)(report->channels[i].dac_code >> 8);
        can_data[6] = (uint8_t)(report->channels[i].dac_code & 0xFF);
        can_data[7] = 0x00;

        start = HAL_GetTick();
        while (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) == 0) {
            if ((HAL_GetTick() - start) > 10) break;
            osDelay(1);
        }
        HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &fdcan_tx_header, can_data);
    }
}

void Comm_SendDebug(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(debug_buffer, sizeof(debug_buffer), fmt, args);
    va_end(args);

    if (len > 0) {
        HAL_UART_Transmit(&huart4, (uint8_t *)debug_buffer, (uint16_t)len, 50);
    }
}
