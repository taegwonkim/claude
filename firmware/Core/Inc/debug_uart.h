#ifndef __DEBUG_UART_H
#define __DEBUG_UART_H

#include "stm32l5xx_hal.h"
#include <stdint.h>

/**
 * @file    debug_uart.h
 * @brief   Debug output via UART4 (TX only)
 *
 * UART4 TX: PA0 (AF8), 115200 baud, 8N1
 * Transmit-only debug port for real-time status monitoring.
 *
 * Debug message format (all ASCII, \r\n terminated):
 *   [DBG][<task>] <message>
 *
 * Examples:
 *   [DBG][VCTRL] CH0 tgt=3.300V act=3.298V dac=10813 err=-2mV
 *   [DBG][FAULT] CH1 SHORT I=523.4mA
 *   [DBG][COMM]  RX CMD: SET 0 3.300
 *   [DBG][SYS]   Boot OK, FreeRTOS started
 */

#define DEBUG_TX_BUF_SIZE   256

/**
 * @brief Initialize debug UART4 handle (call after MX_UART4_Init)
 */
void Debug_Init(UART_HandleTypeDef *huart_debug);

/**
 * @brief Print formatted debug message (blocking TX)
 * @param tag  Module tag (e.g., "VCTRL", "FAULT", "COMM", "SYS")
 * @param fmt  printf-style format string
 */
void Debug_Print(const char *tag, const char *fmt, ...);

/**
 * @brief Print raw string without tag (for multi-line output)
 */
void Debug_Raw(const char *fmt, ...);

/**
 * @brief Print channel status summary (all 4 channels)
 */
void Debug_PrintChannelStatus(void);

/**
 * @brief Check if debug output is enabled
 */
uint8_t Debug_IsEnabled(void);

/**
 * @brief Enable/disable debug output at runtime
 */
void Debug_SetEnabled(uint8_t en);

#endif /* __DEBUG_UART_H */
