#ifndef __COMM_HANDLER_H
#define __COMM_HANDLER_H

#include "stm32l5xx_hal.h"
#include "voltage_control.h"
#include <stdint.h>
#include <stdbool.h>

/* UART Protocol Definition
 *
 * Frame Format: STX(0x02) + Payload + ETX(0x03)
 *   - STX: Start of Text (0x02)
 *   - Payload: ASCII command/response string
 *   - ETX: End of Text (0x03)
 *   - No \r\n inside frame; STX/ETX are the delimiters
 *
 * PC -> MCU Commands:
 *   <STX>SET <ch> <voltage><ETX>    e.g., \x02SET 0 3.300\x03
 *   <STX>GET <ch><ETX>              e.g., \x02GET 0\x03
 *   <STX>GETALL<ETX>
 *   <STX>EN <ch> <0|1><ETX>         e.g., \x02EN 0 1\x03
 *   <STX>STATUS<ETX>
 *
 * MCU -> PC Responses:
 *   <STX>OK SET <ch> <voltage><ETX>
 *   <STX>DATA <ch> <target_V> <actual_V> <current_mA> <state><ETX>
 *   <STX>ERR <message><ETX>
 *   <STX>REPORT <ch0_V> <ch0_I> <ch1_V> <ch1_I> <ch2_V> <ch2_I> <ch3_V> <ch3_I><ETX>
 *   <STX>FAULT <ch> <SHORT|OPEN><ETX>
 *   <STX>CLEAR <ch><ETX>
 *
 * FDCAN Protocol:
 *   TX Message ID: 0x100 (periodic report)
 *   TX Message ID: 0x101 (fault alert)
 *   Standard CAN frame, 8 bytes per message
 */

#define COMM_STX    0x02   /* Start of Text */
#define COMM_ETX    0x03   /* End of Text */

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

    /* UART RX state (STX/ETX framing) */
    uint8_t  rx_byte;
    char     rx_buffer[COMM_UART_RX_BUF_SIZE];
    uint16_t rx_index;
    uint8_t  rx_in_frame;   /* 1 = STX received, collecting payload until ETX */

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
