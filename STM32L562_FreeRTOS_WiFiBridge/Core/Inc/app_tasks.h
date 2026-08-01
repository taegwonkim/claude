/**
 * app_tasks.h
 *
 * Top level glue: module initialization, FreeRTOS task creation, and the
 * shared HAL interrupt callback dispatchers (HAL_UART_RxCpltCallback,
 * HAL_UARTEx_RxEventCallback, HAL_GPIO_EXTI_Callback) that route each
 * peripheral IRQ to the owning module.
 */
#ifndef APP_TASKS_H
#define APP_TASKS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize all application modules (EEPROM, PC UART, ESP32, FPGA
 * interface). Call once after all MX_*_Init() functions have run and
 * before osKernelStart() / task creation.
 */
void App_Init(void);

/**
 * Create the application's FreeRTOS tasks. Call once, either from
 * main.c (after App_Init(), before osKernelStart()) or from
 * MX_FREERTOS_Init() in freertos.c if using CubeMX-generated FreeRTOS
 * integration.
 */
void App_CreateTasks(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_TASKS_H */
