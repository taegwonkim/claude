/**
 ******************************************************************************
 * @file    fdcan_comm.c
 * @brief   FDCAN 통신 모듈 구현
 ******************************************************************************
 */

#include "fdcan_comm.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * 내부 상태
 * ------------------------------------------------------------------------- */
static FDCAN_TxHeaderTypeDef s_tx_hdr;
static FDCAN_RxHeaderTypeDef s_rx_hdr;

/* -------------------------------------------------------------------------
 * CRC8 (Dallas/Maxim polynomial: 0x31)
 * ------------------------------------------------------------------------- */
uint8_t FDCAN_CRC8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31U;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/* -------------------------------------------------------------------------
 * 공개 API 구현
 * ------------------------------------------------------------------------- */

HAL_StatusTypeDef FDCAN_CommInit(void)
{
    HAL_StatusTypeDef ret;

    /* 필터: ID 0x110 수신 → FIFO0 */
    FDCAN_FilterTypeDef filter = {
        .IdType         = FDCAN_STANDARD_ID,
        .FilterIndex    = 0,
        .FilterType     = FDCAN_FILTER_MASK,
        .FilterConfig   = FDCAN_FILTER_TO_RXFIFO0,
        .FilterID1      = FDCAN_ID_CMD_RX,
        .FilterID2      = 0x7FF,    /* mask: 정확히 0x110만 */
    };
    ret = HAL_FDCAN_ConfigFilter(&hfdcan1, &filter);
    if (ret != HAL_OK) return ret;

    /* 비매칭 프레임 거부 */
    ret = HAL_FDCAN_ConfigGlobalFilter(&hfdcan1,
                                        FDCAN_REJECT, FDCAN_REJECT,
                                        FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE);
    if (ret != HAL_OK) return ret;

    /* FIFO0 새 메시지 인터럽트 활성화 */
    ret = HAL_FDCAN_ActivateNotification(&hfdcan1,
                                          FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
    if (ret != HAL_OK) return ret;

    /* FDCAN 시작 */
    ret = HAL_FDCAN_Start(&hfdcan1);
    return ret;
}

HAL_StatusTypeDef FDCAN_SendVoltage(void)
{
    uint8_t payload[8];

    if (osMutexAcquire(xMutexChannelData, 10) != osOK) return HAL_BUSY;

    for (uint8_t i = 0; i < 4; i++) {
        uint16_t mv = g_channels[i].measured_mv;
        payload[i * 2]     = (uint8_t)(mv >> 8);
        payload[i * 2 + 1] = (uint8_t)(mv & 0xFF);
    }

    osMutexRelease(xMutexChannelData);

    s_tx_hdr.Identifier          = FDCAN_ID_VOLTAGE;
    s_tx_hdr.IdType              = FDCAN_STANDARD_ID;
    s_tx_hdr.TxFrameType         = FDCAN_DATA_FRAME;
    s_tx_hdr.DataLength          = FDCAN_DLC_BYTES_8;
    s_tx_hdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    s_tx_hdr.BitRateSwitch       = FDCAN_BRS_OFF;
    s_tx_hdr.FDFormat            = FDCAN_CLASSIC_CAN;
    s_tx_hdr.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    s_tx_hdr.MessageMarker       = 0;

    return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &s_tx_hdr, payload);
}

HAL_StatusTypeDef FDCAN_SendCurrent(void)
{
    uint8_t payload[8];

    if (osMutexAcquire(xMutexChannelData, 10) != osOK) return HAL_BUSY;

    for (uint8_t i = 0; i < 4; i++) {
        int16_t ma = g_channels[i].current_ma;
        payload[i * 2]     = (uint8_t)((uint16_t)ma >> 8);
        payload[i * 2 + 1] = (uint8_t)((uint16_t)ma & 0xFF);
    }

    osMutexRelease(xMutexChannelData);

    s_tx_hdr.Identifier  = FDCAN_ID_CURRENT;
    s_tx_hdr.DataLength  = FDCAN_DLC_BYTES_8;
    s_tx_hdr.FDFormat    = FDCAN_CLASSIC_CAN;
    s_tx_hdr.BitRateSwitch = FDCAN_BRS_OFF;

    return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &s_tx_hdr, payload);
}

HAL_StatusTypeDef FDCAN_SendStatus(void)
{
    uint8_t payload[12] = { 0 };

    if (osMutexAcquire(xMutexChannelData, 10) != osOK) return HAL_BUSY;

    uint8_t fault_bits = 0;
    uint8_t enable_bits = 0;

    for (uint8_t i = 0; i < 4; i++) {
        if (g_channels[i].fault != FAULT_NONE) {
            fault_bits |= ((uint8_t)g_channels[i].fault & 0x03U) << (i * 2);
        }
        if (g_channels[i].enabled) {
            enable_bits |= (1U << i);
        }
        uint16_t tgt = g_channels[i].target_mv;
        payload[2 + i * 2]     = (uint8_t)(tgt >> 8);
        payload[2 + i * 2 + 1] = (uint8_t)(tgt & 0xFF);
    }

    osMutexRelease(xMutexChannelData);

    payload[0] = fault_bits;
    payload[1] = enable_bits;
    /* payload[2..9] = 목표 전압 4개 (위에서 작성) */
    payload[10] = FDCAN_CRC8(payload, 10);
    payload[11] = 0x00;

    s_tx_hdr.Identifier  = FDCAN_ID_STATUS;
    s_tx_hdr.DataLength  = FDCAN_DLC_BYTES_12;
    s_tx_hdr.FDFormat    = FDCAN_FD_CAN;         /* 12바이트는 FDCAN FD 필요 */
    s_tx_hdr.BitRateSwitch = FDCAN_BRS_ON;

    return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &s_tx_hdr, payload);
}

void FDCAN_RxProcess(void)
{
    uint8_t rx_data[4] = { 0 };

    if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0,
                                &s_rx_hdr, rx_data) != HAL_OK) {
        return;
    }

    if (s_rx_hdr.Identifier != FDCAN_ID_CMD_RX) return;

    UartCmd_t cmd = { 0 };
    cmd.type    = (CmdType_t)rx_data[0];
    cmd.channel = rx_data[1];
    cmd.value   = (uint16_t)((rx_data[2] << 8) | rx_data[3]);

    if (cmd.channel >= 1 && cmd.channel <= NUM_CHANNELS) {
        cmd.channel -= 1;   /* 1-based → 0-based */
        osMessageQueuePut(xQueueUARTCmd, &cmd, 0, 0);
    }
}

/* -------------------------------------------------------------------------
 * HAL 콜백
 * ------------------------------------------------------------------------- */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    if (hfdcan->Instance != FDCAN1) return;
    if (RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) {
        FDCAN_RxProcess();
    }
}
