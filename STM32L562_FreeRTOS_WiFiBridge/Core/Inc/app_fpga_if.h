/**
 * app_fpga_if.h
 *
 * Interface to the Cyclone IV FPGA measurement board.
 *
 *  - The FPGA pulses a dedicated GPIO ("trigger") low to signal
 *    "measurement complete" (EXTI falling edge interrupt).
 *  - Immediately after the trigger, the FPGA streams the measurement
 *    data to STM32L562 over UART (USART3).
 *  - As soon as a full frame is received, it is forwarded to the
 *    ESP32-C3-WROOM task (app_esp32.c) for transmission to the server.
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
