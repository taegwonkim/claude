/**
 * app_pc_uart.h
 *
 * PC configuration link over USART1. This is one of two parallel
 * transports for the shared CFG: protocol (see app_cfg_protocol.h) - the
 * other is USB CDC (app_usb_cdc.h). Both feed complete lines into
 * CfgProtocol_HandleLine(); this file only owns USART1 byte I/O and line
 * framing.
 */
#ifndef APP_PC_UART_H
#define APP_PC_UART_H

#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize ring buffer + arm USART1 RX interrupt. Call from App_Init(). */
void App_PcUart_Init(void);

/** FreeRTOS task entry point (created by app_tasks.c). */
void App_PcUart_Task(void *argument);

/**
 * Called from the shared HAL_UART_RxCpltCallback dispatcher (ISR context)
 * when USART1 finishes receiving its 1-byte scratch buffer. Stores the
 * byte into the ring buffer and re-arms the next single-byte reception.
 */
void App_PcUart_UART_RxCpltCallback(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_PC_UART_H */
