/**
 ******************************************************************************
 * @file    uart_protocol.h
 * @brief   UART Command Parser and Protocol Handler
 ******************************************************************************
 *
 * UART Protocol Format (ASCII-based for easy PC interaction):
 *
 * Command Format:  $CMD,CH,VALUE\r\n
 *
 * Commands:
 *   $SV,<ch>,<mV>\r\n      Set Voltage (mV) for channel ch (0-3)
 *   $GV,<ch>\r\n            Get measured Voltage for channel ch
 *   $GA\r\n                 Get All channel data
 *   $GI,<ch>\r\n            Get current (I) for channel ch
 *   $GF\r\n                 Get Fault status for all channels
 *   $EN,<ch>\r\n            Enable channel
 *   $DI,<ch>\r\n            Disable channel
 *   $SP,<Kp>,<Ki>\r\n       Set PID gains (float)
 *   $ST\r\n                 Get System sTatus
 *
 * Response Format:
 *   #OK,<data>\r\n          Success
 *   #ERR,<code>\r\n         Error
 *   #VD,<ch>,<target>,<measured>\r\n   Voltage Data
 *   #ID,<ch>,<current_mA>\r\n          Current Data
 *   #FD,<ch>,<fault_type>\r\n          Fault Data
 *   #SD,<ch>,<target>,<meas>,<current>,<state>\r\n  Status Data
 *
 ******************************************************************************
 */
#ifndef __UART_PROTOCOL_H
#define __UART_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l5xx_hal.h"
#include "app_types.h"
#include <stdint.h>
#include <stdbool.h>

/* ── Error Codes ──────────────────────────────────────────────────────── */
#define UART_ERR_NONE               0
#define UART_ERR_INVALID_CMD        1
#define UART_ERR_INVALID_CHANNEL    2
#define UART_ERR_INVALID_VALUE      3
#define UART_ERR_PARSE_FAIL         4
#define UART_ERR_OVERFLOW           5

/* ── Handle ───────────────────────────────────────────────────────────── */
typedef struct {
    UART_HandleTypeDef  *huart;
    uint8_t             rx_buf[UART_RX_BUF_SIZE];
    uint8_t             cmd_buf[UART_CMD_MAX_LEN];
    uint8_t             cmd_idx;
    volatile bool       line_ready;
} UartProtocol_Handle_t;

/* ── API ──────────────────────────────────────────────────────────────── */

/**
 * @brief Initialize UART protocol handler (starts DMA/IT reception)
 */
HAL_StatusTypeDef UartProtocol_Init(UartProtocol_Handle_t *proto);

/**
 * @brief Process received byte (call from UART RX ISR or idle callback)
 * @param byte  Received byte
 * @return true if a complete command line is ready
 */
bool UartProtocol_ProcessByte(UartProtocol_Handle_t *proto, uint8_t byte);

/**
 * @brief Parse the command buffer into a UartCmd_t structure
 * @param proto  Protocol handle
 * @param cmd    Output: parsed command
 * @return 0 on success, error code on failure
 */
uint8_t UartProtocol_Parse(UartProtocol_Handle_t *proto, UartCmd_t *cmd);

/**
 * @brief Send voltage data response
 */
void UartProtocol_SendVoltageData(UartProtocol_Handle_t *proto,
                                  uint8_t ch, uint16_t target_mv,
                                  uint16_t measured_mv);

/**
 * @brief Send current data response
 */
void UartProtocol_SendCurrentData(UartProtocol_Handle_t *proto,
                                  uint8_t ch, uint16_t current_ma);

/**
 * @brief Send fault data response
 */
void UartProtocol_SendFaultData(UartProtocol_Handle_t *proto,
                                uint8_t ch, FaultType_t fault);

/**
 * @brief Send full status for all channels
 */
void UartProtocol_SendAllStatus(UartProtocol_Handle_t *proto,
                                ChannelCtrl_t channels[NUM_CHANNELS]);

/**
 * @brief Send OK response
 */
void UartProtocol_SendOK(UartProtocol_Handle_t *proto, const char *msg);

/**
 * @brief Send Error response
 */
void UartProtocol_SendError(UartProtocol_Handle_t *proto, uint8_t err_code);

/**
 * @brief Send formatted message over UART (blocking, mutex-protected)
 */
void UartProtocol_SendString(UartProtocol_Handle_t *proto, const char *str);

#ifdef __cplusplus
}
#endif

#endif /* __UART_PROTOCOL_H */
