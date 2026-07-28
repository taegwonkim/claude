#include "app_tasks.h"
#include "app_config.h"
#include "app_eeprom.h"
#include "app_pc_uart.h"
#include "app_esp32.h"
#include "app_fpga_if.h"
#include "cmsis_os2.h"

void App_Init(void)
{
    App_Eeprom_Init();
    App_PcUart_Init();
    App_Esp32_Init();
    App_FpgaIf_Init();
}

void App_CreateTasks(void)
{
    const osThreadAttr_t pcUartAttr = {
        .name = "pcUartTask",
        .stack_size = TASK_STACK_PC_UART * 4U,
        .priority = TASK_PRIO_PC_UART,
    };
    const osThreadAttr_t esp32Attr = {
        .name = "esp32Task",
        .stack_size = TASK_STACK_ESP32 * 4U,
        .priority = TASK_PRIO_ESP32,
    };
    const osThreadAttr_t fpgaIfAttr = {
        .name = "fpgaIfTask",
        .stack_size = TASK_STACK_FPGA_IF * 4U,
        .priority = TASK_PRIO_FPGA_IF,
    };

    osThreadNew(App_PcUart_Task, NULL, &pcUartAttr);
    osThreadNew(App_Esp32_Task,  NULL, &esp32Attr);
    osThreadNew(App_FpgaIf_Task, NULL, &fpgaIfAttr);
}

/* ------------------------------------------------------------------------ *
 * Shared HAL interrupt callback dispatchers.
 *
 * IMPORTANT: these override the __weak defaults declared in
 * stm32l5xx_hal_uart.c / stm32l5xx_hal_gpio.c. Do NOT re-define them
 * anywhere else in the project (e.g. remove any stub left behind by
 * CubeMX code generation in main.c's USER CODE sections).
 * ------------------------------------------------------------------------ */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        App_PcUart_UART_RxCpltCallback();
    } else if (huart->Instance == USART2) {
        App_Esp32_UART_RxCpltCallback();
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART3) {
        App_FpgaIf_RxEventISR(Size);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == FPGA_TRIGGER_GPIO_PIN) {
        App_FpgaIf_TriggerISR();
    }
}
