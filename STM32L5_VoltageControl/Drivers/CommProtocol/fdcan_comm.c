/**
 ******************************************************************************
 * @file    fdcan_comm.c
 * @brief   FDCAN Communication Module Implementation
 ******************************************************************************
 */
#include "fdcan_comm.h"
#include "app_config.h"

/* ── Initialization ───────────────────────────────────────────────────── */

HAL_StatusTypeDef FdcanComm_Init(FdcanComm_Handle_t *fcan)
{
    /* Configure default TX header */
    fcan->tx_header.Identifier = FDCAN_TX_ID_VOLTAGE;
    fcan->tx_header.IdType = FDCAN_STANDARD_ID;
    fcan->tx_header.TxFrameType = FDCAN_DATA_FRAME;
    fcan->tx_header.DataLength = FDCAN_DLC_BYTES_8;
    fcan->tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    fcan->tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    fcan->tx_header.FDFormat = FDCAN_CLASSIC_CAN;
    fcan->tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    fcan->tx_header.MessageMarker = 0;

    /* Configure RX filter to accept all (for potential bidirectional comm) */
    FDCAN_FilterTypeDef filter;
    filter.IdType = FDCAN_STANDARD_ID;
    filter.FilterIndex = 0;
    filter.FilterType = FDCAN_FILTER_MASK;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1 = 0x000;
    filter.FilterID2 = 0x000;  /* Accept all */

    HAL_StatusTypeDef status = HAL_FDCAN_ConfigFilter(fcan->hfdcan, &filter);
    if (status != HAL_OK) return status;

    /* Start FDCAN */
    status = HAL_FDCAN_Start(fcan->hfdcan);
    if (status != HAL_OK) return status;

    /* Enable RX FIFO0 new message notification */
    status = HAL_FDCAN_ActivateNotification(fcan->hfdcan,
                                             FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
    return status;
}

/* ── Send Voltage Data ────────────────────────────────────────────────── */

HAL_StatusTypeDef FdcanComm_SendVoltageData(FdcanComm_Handle_t *fcan,
                                            uint8_t ch,
                                            uint16_t target_mv,
                                            uint16_t measured_mv,
                                            uint16_t dac_raw,
                                            ChannelState_t state)
{
    uint8_t data[8];

    fcan->tx_header.Identifier = FDCAN_TX_ID_VOLTAGE + ch;

    data[0] = (uint8_t)(target_mv >> 8);    /* Target high byte */
    data[1] = (uint8_t)(target_mv & 0xFF);  /* Target low byte */
    data[2] = (uint8_t)(measured_mv >> 8);   /* Measured high byte */
    data[3] = (uint8_t)(measured_mv & 0xFF); /* Measured low byte */
    data[4] = (uint8_t)(dac_raw >> 8);       /* DAC raw high byte */
    data[5] = (uint8_t)(dac_raw & 0xFF);     /* DAC raw low byte */
    data[6] = (uint8_t)state;                /* Channel state */
    data[7] = 0x00;                          /* Reserved */

    return HAL_FDCAN_AddMessageToTxFifoQ(fcan->hfdcan, &fcan->tx_header, data);
}

/* ── Send Fault Event ─────────────────────────────────────────────────── */

HAL_StatusTypeDef FdcanComm_SendFaultEvent(FdcanComm_Handle_t *fcan,
                                           const FaultEvent_t *evt)
{
    uint8_t data[8];

    fcan->tx_header.Identifier = FDCAN_TX_ID_FAULT + evt->channel;

    data[0] = (uint8_t)evt->fault;
    data[1] = (uint8_t)(evt->measured_value >> 8);
    data[2] = (uint8_t)(evt->measured_value & 0xFF);
    data[3] = evt->channel;
    data[4] = (uint8_t)(evt->timestamp >> 24);
    data[5] = (uint8_t)(evt->timestamp >> 16);
    data[6] = (uint8_t)(evt->timestamp >> 8);
    data[7] = (uint8_t)(evt->timestamp & 0xFF);

    return HAL_FDCAN_AddMessageToTxFifoQ(fcan->hfdcan, &fcan->tx_header, data);
}

/* ── Send System Status ───────────────────────────────────────────────── */

HAL_StatusTypeDef FdcanComm_SendSystemStatus(FdcanComm_Handle_t *fcan,
                                             ChannelCtrl_t channels[NUM_CHANNELS])
{
    uint8_t data[8];

    fcan->tx_header.Identifier = FDCAN_TX_ID_STATUS;

    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        data[i * 2]     = (uint8_t)channels[i].state;
        data[i * 2 + 1] = channels[i].enabled ? 1 : 0;
    }

    return HAL_FDCAN_AddMessageToTxFifoQ(fcan->hfdcan, &fcan->tx_header, data);
}

/* ── Send Current Data ────────────────────────────────────────────────── */

HAL_StatusTypeDef FdcanComm_SendCurrentData(FdcanComm_Handle_t *fcan,
                                            uint8_t ch, uint16_t current_ma)
{
    uint8_t data[8] = {0};

    fcan->tx_header.Identifier = FDCAN_TX_ID_VOLTAGE + 0x10 + ch;

    data[0] = ch;
    data[1] = (uint8_t)(current_ma >> 8);
    data[2] = (uint8_t)(current_ma & 0xFF);

    return HAL_FDCAN_AddMessageToTxFifoQ(fcan->hfdcan, &fcan->tx_header, data);
}
