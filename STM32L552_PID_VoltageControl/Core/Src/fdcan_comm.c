/* fdcan_comm.c - FDCAN 통신 구현 (500 kbps, Classic CAN)
 *
 * FDCAN1 핀:
 *   PD0 → FDCAN1_RX  (AF9)
 *   PD1 → FDCAN1_TX  (AF9)
 *
 * 비트 타이밍 (FDCAN 클럭 = PLLQ = 40 MHz 가정):
 *   Prescaler = 4 → TQ = 100 ns
 *   총 TQ / bit = 20 → bit rate = 1 / (20 * 100ns) = 500 kbps
 *   Seg1 = 14 TQ, Seg2 = 5 TQ, SJW = 1 TQ
 *   (CubeMX에서 실제 클럭에 맞게 재계산 필요)
 */
#include "fdcan_comm.h"
#include "channel_ctrl.h"
#include "app_tasks.h"
#include <string.h>

/* ==========================================================
 * 수신 명령 큐 (ISR → RX 태스크 전달)
 * ========================================================== */
/* g_fdcan_cmd_queue 는 app_tasks.c 에서 생성/정의 */

/* ==========================================================
 * FDCANComm_Init
 *   - 수신 필터: ID 0x200 (CMD), 0x210 (PID_CFG) 허용
 *   - FIFO0 인터럽트 활성화
 * ========================================================== */
HAL_StatusTypeDef FDCANComm_Init(void)
{
    FDCAN_FilterTypeDef sFilterConfig;
    HAL_StatusTypeDef   status;

    /* 필터 0: 명령 ID (0x200) */
    sFilterConfig.IdType       = FDCAN_STANDARD_ID;
    sFilterConfig.FilterIndex  = 0U;
    sFilterConfig.FilterType   = FDCAN_FILTER_RANGE;
    sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    sFilterConfig.FilterID1    = FDCAN_ID_CMD;      /* 0x200 */
    sFilterConfig.FilterID2    = FDCAN_ID_PID_CFG;  /* 0x210 */

    status = HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig);
    if (status != HAL_OK) return status;

    /* 필터 비일치 메시지 거부 */
    status = HAL_FDCAN_ConfigGlobalFilter(
        &hfdcan1,
        FDCAN_REJECT,          /* non-matching standard */
        FDCAN_REJECT,          /* non-matching extended */
        FDCAN_FILTER_REMOTE,   /* remote frames standard */
        FDCAN_FILTER_REMOTE);  /* remote frames extended */
    if (status != HAL_OK) return status;

    /* FIFO0 수신 인터럽트 활성화 */
    status = HAL_FDCAN_ActivateNotification(
        &hfdcan1,
        FDCAN_IT_RX_FIFO0_NEW_MESSAGE,
        0U);
    if (status != HAL_OK) return status;

    /* FDCAN 시작 */
    return HAL_FDCAN_Start(&hfdcan1);
}

/* ==========================================================
 * FDCANComm_SendTelemetry
 * ========================================================== */
HAL_StatusTypeDef FDCANComm_SendTelemetry(uint8_t ch)
{
    if (ch >= NUM_CHANNELS) return HAL_ERROR;

    ChannelCtrl_t *c = &g_channels[ch];

    /* 페이로드 구성 */
    FDCAN_TelemMsg_t payload;
    payload.ch_index    = ch;
    payload.status      = c->status;
    payload.setpoint_mV = (uint16_t)(c->voltage_setpoint_V * 1000.0f);
    payload.voltage_mV  = (uint16_t)(c->voltage_meas_V     * 1000.0f);
    payload.current_mA  = (uint16_t)(c->current_meas_A     * 1000.0f);

    /* TX 헤더 */
    FDCAN_TxHeaderTypeDef txHeader;
    txHeader.Identifier          = FDCAN_ID_TELEM_BASE + ch;
    txHeader.IdType              = FDCAN_STANDARD_ID;
    txHeader.TxFrameType         = FDCAN_DATA_FRAME;
    txHeader.DataLength          = FDCAN_DLC_BYTES_8;
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch       = FDCAN_BRS_OFF;    /* Classic CAN */
    txHeader.FDFormat            = FDCAN_CLASSIC_CAN;
    txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker       = 0U;

    return HAL_FDCAN_AddMessageToTxFifoQ(
        &hfdcan1,
        &txHeader,
        (uint8_t *)&payload);
}

/* ==========================================================
 * FDCANComm_SendHeartbeat
 *   시스템 전체 상태를 8바이트로 전송
 *   Byte 0~3: 각 채널 status (1바이트씩)
 *   Byte 4:   활성 채널 수
 *   Byte 5~7: 예약
 * ========================================================== */
HAL_StatusTypeDef FDCANComm_SendHeartbeat(void)
{
    uint8_t data[8] = {0};

    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        data[i] = g_channels[i].status;
    }

    uint8_t active = 0;
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        if (g_channels[i].status & CH_STATUS_ENABLED) active++;
    }
    data[4] = active;

    FDCAN_TxHeaderTypeDef txHeader;
    txHeader.Identifier          = FDCAN_ID_HEARTBEAT;
    txHeader.IdType              = FDCAN_STANDARD_ID;
    txHeader.TxFrameType         = FDCAN_DATA_FRAME;
    txHeader.DataLength          = FDCAN_DLC_BYTES_8;
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch       = FDCAN_BRS_OFF;
    txHeader.FDFormat            = FDCAN_CLASSIC_CAN;
    txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker       = 0U;

    return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, data);
}

/* ==========================================================
 * FDCANComm_RxCallback
 *   HAL_FDCAN_RxFifo0Callback 에서 호출
 *   수신 메시지를 큐에 넣음 (ISR 컨텍스트)
 * ========================================================== */
void FDCANComm_RxCallback(FDCAN_HandleTypeDef *hfdcan)
{
    FDCAN_RxHeaderTypeDef rxHeader;
    uint8_t               rxData[8];

    if (HAL_FDCAN_GetRxMessage(hfdcan,
                                FDCAN_RX_FIFO0,
                                &rxHeader,
                                rxData) != HAL_OK) {
        return;
    }

    /* 유효한 ID만 처리 */
    if (rxHeader.Identifier != FDCAN_ID_CMD &&
        rxHeader.Identifier != FDCAN_ID_PID_CFG) {
        return;
    }

    if (rxHeader.DataLength < FDCAN_DLC_BYTES_8) {
        return;
    }

    FDCAN_RxCmd_t cmd;
    memcpy(&cmd.msg, rxData, sizeof(FDCAN_CmdMsg_t));

    /* ISR 컨텍스트에서 FromISR 버전 사용 */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(g_fdcan_cmd_queue, &cmd, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ==========================================================
 * FDCANComm_ProcessCommands
 *   RX 태스크 루프에서 호출
 *   큐에서 명령 꺼내어 채널 제어 함수 호출
 * ========================================================== */
void FDCANComm_ProcessCommands(void)
{
    FDCAN_RxCmd_t cmd;

    /* 큐가 빌 때까지 처리 (논블로킹: timeout=0) */
    while (xQueueReceive(g_fdcan_cmd_queue, &cmd, 0) == pdTRUE) {
        uint8_t ch  = cmd.msg.ch_index;
        uint8_t opc = cmd.msg.cmd_code;

        /* 브로드캐스트 처리 (0xFF = 전체 채널) */
        uint8_t ch_start = (ch == 0xFFU) ? 0U           : ch;
        uint8_t ch_end   = (ch == 0xFFU) ? NUM_CHANNELS : ch + 1U;

        for (uint8_t i = ch_start; i < ch_end; i++) {
            if (i >= NUM_CHANNELS) break;

            switch (opc) {
            case CMD_DISABLE:
                ChannelCtrl_Enable(i, 0U);
                break;

            case CMD_ENABLE:
                ChannelCtrl_Enable(i, 1U);
                break;

            case CMD_SET_VOLTAGE:
                ChannelCtrl_SetVoltage(i, (uint32_t)cmd.msg.param_u16);
                break;

            case CMD_SET_KP:
                g_channels[i].pid.Kp = cmd.msg.param_f32;
                break;

            case CMD_SET_KI:
                g_channels[i].pid.Ki = cmd.msg.param_f32;
                break;

            case CMD_SET_KD:
                g_channels[i].pid.Kd = cmd.msg.param_f32;
                break;

            case CMD_PID_RESET:
                PID_Reset(&g_channels[i].pid);
                break;

            default:
                break;
            }
        }
    }
}

/* ==========================================================
 * HAL_FDCAN_RxFifo0Callback (weak 함수 오버라이드)
 * ========================================================== */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan,
                                uint32_t RxFifo0ITs)
{
    if (RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) {
        FDCANComm_RxCallback(hfdcan);
    }
}
