/**
 ******************************************************************************
 * @file    fdcan_comm.h
 * @brief   FDCAN Communication Module
 ******************************************************************************
 * Sends voltage measurements, fault events, and status via FDCAN.
 *
 * CAN Message Format:
 *   ID 0x100 + ch: Voltage data  [target_H, target_L, meas_H, meas_L, raw_H, raw_L, state, 0]
 *   ID 0x200 + ch: Fault event   [fault_type, value_H, value_L, 0, 0, 0, 0, 0]
 *   ID 0x300:      System status  [ch0_st, ch1_st, ch2_st, ch3_st, 0, 0, 0, 0]
 ******************************************************************************
 */
#ifndef __FDCAN_COMM_H
#define __FDCAN_COMM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"
#include "app_types.h"

/* ── Handle ───────────────────────────────────────────────────────────── */
typedef struct {
    FDCAN_HandleTypeDef     *hfdcan;
    FDCAN_TxHeaderTypeDef   tx_header;
} FdcanComm_Handle_t;

/* ── API ──────────────────────────────────────────────────────────────── */

/**
 * @brief Initialize FDCAN communication
 */
HAL_StatusTypeDef FdcanComm_Init(FdcanComm_Handle_t *fcan);

/**
 * @brief Send voltage data for a channel
 */
HAL_StatusTypeDef FdcanComm_SendVoltageData(FdcanComm_Handle_t *fcan,
                                            uint8_t ch,
                                            uint16_t target_mv,
                                            uint16_t measured_mv,
                                            uint16_t dac_raw,
                                            ChannelState_t state);

/**
 * @brief Send fault event
 */
HAL_StatusTypeDef FdcanComm_SendFaultEvent(FdcanComm_Handle_t *fcan,
                                           const FaultEvent_t *evt);

/**
 * @brief Send system status (all channels summary)
 */
HAL_StatusTypeDef FdcanComm_SendSystemStatus(FdcanComm_Handle_t *fcan,
                                             ChannelCtrl_t channels[NUM_CHANNELS]);

/**
 * @brief Send current measurement data for a channel
 */
HAL_StatusTypeDef FdcanComm_SendCurrentData(FdcanComm_Handle_t *fcan,
                                            uint8_t ch, uint16_t current_ma);

#ifdef __cplusplus
}
#endif

#endif /* __FDCAN_COMM_H */
