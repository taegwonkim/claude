/**
 * app_fpga_if.h
 *
 * Interface to the Cyclone IV FPGA measurement board, and the top-level
 * continuous measurement loop:
 *
 *  1. Ensure the ESP32-C3-WROOM Wi-Fi + server link is up (app_esp32.c);
 *     if it isn't, request a (re)connect and wait for it before proceeding.
 *  2. Send a "start measurement" command to Cyclone IV over USART3
 *     (FPGA_CMD_START_MEASURE).
 *  3. Wait for the FPGA to pulse a dedicated GPIO ("trigger") low to
 *     signal "measurement complete" (EXTI falling edge interrupt).
 *  4. Immediately after the trigger, the FPGA streams the measurement
 *     data to STM32L562 over UART (USART3, DMA + IDLE-line detection).
 *  5. The received frame is forwarded to the ESP32-C3-WROOM task for
 *     transmission to the server.
 *  6. Repeat from step 1 - this runs continuously as long as the link
 *     stays connected; if it drops, step 1 blocks (with retry) until
 *     reconnected before the next measurement is started.
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
