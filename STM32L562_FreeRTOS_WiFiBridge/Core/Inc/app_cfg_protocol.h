/**
 * app_cfg_protocol.h
 *
 * Transport-agnostic CFG: command protocol shared by every PC
 * configuration channel (USART1 - app_pc_uart.c, and USB CDC -
 * app_usb_cdc.c). Both channels feed complete lines into
 * CfgProtocol_HandleLine(); the parsed configuration is kept in one
 * shared in-RAM buffer and persisted to EEPROM (app_eeprom.c), so it
 * doesn't matter which physical link a given command arrived on - the
 * two channels configure the same device state.
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
 * Each command is acknowledged with "OK\r\n" or "ERR:<reason>\r\n",
 * delivered back on whichever transport the command came in on.
 */
#ifndef APP_CFG_PROTOCOL_H
#define APP_CFG_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

/** Sends a reply string (already CR/LF terminated) back on the transport
 * the command was received on (implemented per-transport: PcUart_Reply()
 * in app_pc_uart.c, UsbCdc_Reply() in app_usb_cdc.c). */
typedef void (*CfgReplyFn)(const char *msg);

/**
 * Load the last-saved configuration (or defaults) into the shared
 * pending-config buffer and create its guard mutex. Call exactly once
 * from App_Init(), before any transport task starts.
 */
void CfgProtocol_Init(void);

/**
 * Parse and execute one CFG: command line. `line` is mutated in place
 * (trailing CR/LF stripped) and must be NUL-terminated. Thread-safe:
 * app_pc_uart.c and app_usb_cdc.c may call this concurrently from their
 * own tasks - access to the shared pending-config is mutex-guarded.
 */
void CfgProtocol_HandleLine(char *line, CfgReplyFn reply);

#ifdef __cplusplus
}
#endif

#endif /* APP_CFG_PROTOCOL_H */
