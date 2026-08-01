/**
 * app_esp32.h
 *
 * Driver/task for the ESP32-C3-WROOM Wi-Fi module, controlled over
 * USART2 using the standard Espressif AT command set. Owns the UART so
 * that AT command/response exchanges are always serialized: other
 * modules never talk to the UART directly, they post a request to this
 * task instead.
 */
#ifndef APP_ESP32_H
#define APP_ESP32_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP32_WIFI_CONNECTED_BIT   (0x00000001U)

/** Initialize ring buffer, request queue, sync objects and arm RX IT. */
void App_Esp32_Init(void);

/** FreeRTOS task entry point (created by app_tasks.c). Owns USART2. */
void App_Esp32_Task(void *argument);

/**
 * Called from the shared HAL_UART_RxCpltCallback dispatcher (ISR context)
 * when USART2 finishes receiving its 1-byte scratch buffer. Stores the
 * byte into the ring buffer and re-arms the next single-byte reception.
 */
void App_Esp32_UART_RxCpltCallback(void);

/**
 * Ask the ESP32 task to (re)join the AP + TCP-connect to the server using
 * the configuration currently stored in EEPROM. Non-blocking / async -
 * the actual join happens in App_Esp32_Task's context. Returns false only
 * if the internal request queue is full.
 */
bool App_Esp32_RequestConnect(void);

/**
 * Same as App_Esp32_RequestConnect(), but blocks the calling task
 * (polling App_Esp32_IsConnected()) until the link comes up or
 * timeout_ms elapses. Intended for the measurement loop in
 * app_fpga_if.c to wait out a reconnect before starting the next cycle.
 */
bool App_Esp32_ConnectAndWait(uint32_t timeout_ms);

/**
 * Hand a measurement data buffer (e.g. forwarded from Cyclone IV, see
 * app_fpga_if.c) to the ESP32 task for transmission to the server via
 * AT+CIPSEND. Blocks the calling task until the transfer completes or
 * timeout_ms elapses. If the link is currently down, a reconnect is
 * attempted automatically before sending.
 *
 * NOTE: caller must keep `data` valid/unmodified until this call returns.
 */
bool App_Esp32_SendMeasurementData(const uint8_t *data, uint16_t len, uint32_t timeout_ms);

/** True if the module currently has an active Wi-Fi + TCP server session. */
bool App_Esp32_IsConnected(void);

/**
 * Copy the ESP32-C3's station MAC address (as read from AT+CIPSTAMAC?
 * during the last successful connect) into out as a NUL-terminated
 * "xx:xx:xx:xx:xx:xx" string. out_size should be >= ESP32_MAC_STR_LEN.
 * Returns the default "00:00:00:00:00:00" placeholder if no successful
 * connect has happened yet.
 */
void App_Esp32_GetMacAddress(char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* APP_ESP32_H */
