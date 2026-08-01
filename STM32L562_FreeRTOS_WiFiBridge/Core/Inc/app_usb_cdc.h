/**
 * app_usb_cdc.h
 *
 * PC configuration link over USB CDC-ACM (virtual COM port on the
 * STM32L562's native USB_OTG_FS peripheral, PA11/PA12). This is the
 * second of two parallel transports for the shared CFG: protocol (see
 * app_cfg_protocol.h) - the other is USART1 (app_pc_uart.h). Both feed
 * complete lines into CfgProtocol_HandleLine(); this file only owns USB
 * CDC byte I/O and line framing.
 *
 * NOTE: there is no "baud rate" to configure here - unlike a UART, USB
 * Full-Speed always runs its physical link at 12 Mbit/s. The "baud"
 * value a PC terminal app lets you pick for a CDC virtual COM port is
 * just a field in the USB CDC Line Coding structure (set via the
 * CDC_SET_LINE_CODING control request, handled in usbd_cdc_if.c's
 * CDC_Control_FS()); this project doesn't need to act on it.
 */
#ifndef APP_USB_CDC_H
#define APP_USB_CDC_H

#include <stdint.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the ring buffer. Call from App_Init(). */
void App_UsbCdc_Init(void);

/** FreeRTOS task entry point (created by app_tasks.c). */
void App_UsbCdc_Task(void *argument);

/**
 * Called from usbd_cdc_if.c's CDC_Receive_FS() (USB IRQ context) with a
 * just-received packet. Copies bytes into the ring buffer; does NOT
 * re-arm reception - the caller (ST's USB middleware) is responsible for
 * calling USBD_CDC_ReceivePacket() itself after this returns.
 */
void App_UsbCdc_RxFromISR(const uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* APP_USB_CDC_H */
