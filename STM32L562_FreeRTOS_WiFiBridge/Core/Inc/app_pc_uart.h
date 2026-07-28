/**
 * app_pc_uart.h
 *
 * PC configuration link (USART1). Accepts a simple line-based text
 * protocol used to provision Wi-Fi AP credentials, DHCP/static-IP
 * settings and the target server IP/port, then stores them in EEPROM.
 *
 * Protocol (one command per line, terminated by '\n' or "\r\n"):
 *
 *   CFG:SSID=<ssid>               up to 32 chars
 *   CFG:PASS=<password>           up to 64 chars
 *   CFG:DHCP=<0|1>                1 = use DHCP, 0 = use static IP fields
 *   CFG:IP=<a.b.c.d>              static IP   (used when DHCP=0)
 *   CFG:GW=<a.b.c.d>              static gateway
 *   CFG:MASK=<a.b.c.d>            static netmask
 *   CFG:SERVERIP=<a.b.c.d>        remote server IP
 *   CFG:SERVERPORT=<0-65535>      remote server TCP port
 *   CFG:SAVE                      commit pending fields to EEPROM and
 *                                 (re)connect ESP32-C3 to Wi-Fi + server
 *   CFG:CONNECT                   force a (re)connect using the last
 *                                 saved EEPROM configuration
 *
 * Each command is acknowledged with "OK\r\n" or "ERR:<reason>\r\n".
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
