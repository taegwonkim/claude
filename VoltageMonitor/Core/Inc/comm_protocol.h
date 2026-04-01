/**
  ******************************************************************************
  * @file           : comm_protocol.h
  * @brief          : UART/FDCAN communication protocol header
  ******************************************************************************
  *
  * UART Protocol (ASCII-based):
  *   Command format:  "SET <ch> <voltage>\r\n"
  *     Example: "SET 1 3.300\r\n"   → Set channel 1 to 3.3V
  *              "SET 0 0.000\r\n"   → Set channel 0 to 0V
  *
  *   Query format:    "GET <ch>\r\n" or "GET ALL\r\n"
  *
  *   Response format: "CH<n>: V=<voltage>V I=<current>mA S=<status>\r\n"
  *     Example: "CH0: V=3.298V I=12.5mA S=OK\r\n"
  *
  *   Report format (periodic):
  *     "RPT <timestamp_ms>\r\n"
  *     "CH0: V=3.298V I=12.5mA S=OK\r\n"
  *     "CH1: V=1.200V I= 5.3mA S=OK\r\n"
  *     "CH2: V=0.000V I= 0.0mA S=OK\r\n"
  *     "CH3: V=5.000V I=25.1mA S=OK\r\n"
  *     "END\r\n"
  *
  * FDCAN Protocol (Binary):
  *   CAN ID 0x100 + channel (0x100~0x103)
  *   Data[0..3]: target voltage (float, little-endian)
  *   Data[4..7]: actual voltage (float, little-endian)
  *
  *   CAN ID 0x110 + channel (0x110~0x113)
  *   Data[0..3]: current in mA (float, little-endian)
  *   Data[4]:    status byte
  *
  ******************************************************************************
  */
#ifndef __COMM_PROTOCOL_H
#define __COMM_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* UART RX buffer size */
#define UART_RX_BUF_SIZE        128
#define UART_TX_BUF_SIZE        512

/* FDCAN IDs */
#define FDCAN_ID_VOLTAGE_BASE   0x100   /* 0x100~0x103 per channel */
#define FDCAN_ID_CURRENT_BASE   0x110   /* 0x110~0x113 per channel */

/* Initialize communication module */
void Comm_Init(void);

/* Parse received UART command - returns 1 if valid command parsed */
uint8_t Comm_ParseCommand(const char *cmd_str, VoltageCmd_t *cmd);

/* Send report data via UART1 */
void Comm_SendReportUART(const ReportData_t *report);

/* Send report data via FDCAN1 */
void Comm_SendReportFDCAN(const ReportData_t *report);

/* Send single channel response via UART1 */
void Comm_SendChannelResponse(uint8_t channel, const ChannelData_t *data);

/* Send debug message via UART4 */
void Comm_SendDebug(const char *fmt, ...);

/* UART1 RX idle line callback (called from ISR) */
void Comm_UART1_RxCallback(uint16_t size);

/* Get status string from status enum */
const char* Comm_StatusToString(ChannelStatus_t status);

#ifdef __cplusplus
}
#endif

#endif /* __COMM_PROTOCOL_H */
