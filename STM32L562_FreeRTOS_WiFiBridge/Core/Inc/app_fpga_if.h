/**
 * app_fpga_if.h
 *
 * Interface to the Cyclone IV FPGA measurement board, and the top-level
 * measurement loop:
 *
 *  1. Ensure the ESP32-C3-WROOM Wi-Fi + server link is up (app_esp32.c);
 *     if it isn't, request a (re)connect and wait for it before proceeding.
 *  2. The FIRST time the link comes up (boot, or right after a reconnect),
 *     send a single "start" command to Cyclone IV over USART3
 *     (FPGA_CMD_START_MEASURE). Cyclone IV then free-runs its own
 *     measurement period - this command is NOT resent every cycle.
 *  3. Wait for the FPGA to pulse a dedicated GPIO ("trigger") low to
 *     signal "measurement complete" (EXTI falling edge interrupt),
 *     fired periodically by Cyclone IV on its own schedule.
 *  4. Immediately after the trigger, the FPGA streams the measurement
 *     data to STM32L562 over UART (USART3, DMA + IDLE-line detection).
 *  5. The received frame is forwarded to the ESP32-C3-WROOM task for
 *     transmission to the server.
 *  6. Repeat from step 3 (no re-arming needed) for as long as the link
 *     stays connected. If it drops, step 1 blocks (with retry) until
 *     reconnected, and step 2 fires once more to (re)kick off Cyclone IV.
 */
#ifndef APP_FPGA_IF_H
#define APP_FPGA_IF_H

#include <stdint.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize sync objects. Call from App_Init(). */
void App_FpgaIf_Init(void);

/** FreeRTOS task entry point (created by app_tasks.c). Owns USART3. */
void App_FpgaIf_Task(void *argument);

/** Called from the shared HAL_GPIO_EXTI_Callback dispatcher (ISR context). */
void App_FpgaIf_TriggerISR(void);

/** Called from the shared HAL_UARTEx_RxEventCallback dispatcher (ISR context). */
void App_FpgaIf_RxEventISR(uint16_t size);

#ifdef __cplusplus
}
#endif

#endif /* APP_FPGA_IF_H */
