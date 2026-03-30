#ifndef __COMM_HANDLER_H
#define __COMM_HANDLER_H

#include "stm32l5xx_hal.h"
#include "voltage_control.h"
#include <stdint.h>
#include <stdbool.h>

/* UART Protocol Definition
 *
 * PC -> MCU Commands (ASCII, newline terminated):
 *   SET <ch> <voltage>    - Set channel voltage (e.g., "SET 0 3.300\n")
 *   GET <ch>              - Query single channel (e.g., "GET 0\n")
 *   GETALL                - Query all channels
 *   EN <ch> <0|1>         - Enable/disable channel (e.g., "EN 0 1\n")
 *   STATUS                - Get system status
 *
 * MCU -> PC Responses (ASCII, newline terminated):
 *   OK SET <ch> <voltage>
 *   DATA <ch> <target_V> <actual_V> <current_mA> <state>
 *   ERR <message>
 *   REPORT <ch0_V> <ch0_I> <ch1_V> <ch1_I> <ch2_V> <ch2_I> <ch3_V> <ch3_I>
 *   FAULT <ch> <SHORT|OPEN>
 *
 * FDCAN Protocol:
 *   TX Message ID: 0x100 (periodic report)
 *   TX Message ID: 0x101 (fault alert)
 *   Standard CAN frame, 8 bytes per message
 */

#define COMM_UART_RX_BUF_SIZE   128
#define COMM_UART_TX_BUF_SIZE   256
#define COMM_CMD_MAX_LEN        64

/* CAN Message IDs */
#define CAN_ID_REPORT_CH01     0x100   /* CH0 + CH1 data */
#define CAN_ID_REPORT_CH23     0x101   /* CH2 + CH3 data */
#define CAN_ID_FAULT           0x102   /* Fault alert */

/* CAN data format for report:
 * Byte 0-1: CH_x voltage (uint16, mV)
 * Byte 2-3: CH_x current (uint16, 0.1mA units)
 * Byte 4-5: CH_y voltage (uint16, mV)
 * Byte 6-7: CH_y current (uint16, 0.1mA units)
 */

typedef struct {
    UART_HandleTypeDef *huart;
    FDCAN_HandleTypeDef *hfdcan;
    VoltCtrl_Handle_t  *hctrl;

    /* UART RX state */
    uint8_t  rx_byte;
    char     rx_buffer[COMM_UART_RX_BUF_SIZE];
    uint16_t rx_index;

    /* UART TX buffer */
    char     tx_buffer[COMM_UART_TX_BUF_SIZE];
} Comm_Handle_t;

/**
 * @brief Initialize communication handler
 */
void Comm_Init(Comm_Handle_t *hcomm, UART_HandleTypeDef *huart,
               FDCAN_HandleTypeDef *hfdcan, VoltCtrl_Handle_t *hctrl);

/**
 * @brief Start UART reception (call once after init)
 */
void Comm_StartReception(Comm_Handle_t *hcomm);

/**
 * @brief UART RX complete callback (call from HAL_UART_RxCpltCallback)
 */
void Comm_UART_RxCallback(Comm_Handle_t *hcomm);

/**
 * @brief Parse and execute a received command
 */
void Comm_ProcessCommand(Comm_Handle_t *hcomm, const char *cmd);

/**
 * @brief Send periodic report via UART
 */
void Comm_SendUartReport(Comm_Handle_t *hcomm);

/**
 * @brief Send periodic report via FDCAN
 */
void Comm_SendCanReport(Comm_Handle_t *hcomm);

/**
 * @brief Send fault alert via UART and FDCAN
 */
void Comm_SendFaultAlert(Comm_Handle_t *hcomm, uint8_t ch, ChannelState_t state);

/**
 * @brief Send formatted string via UART (blocking)
 */
void Comm_UartSend(Comm_Handle_t *hcomm, const char *fmt, ...);

#endif /* __COMM_HANDLER_H */
