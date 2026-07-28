/**
 * freertos.c
 *
 * CMSIS_V2 FreeRTOS init hook, called from main() right after
 * osKernelInitialize(). This is where CubeMX normally generates a
 * "defaultTask" - that has been intentionally omitted here: this
 * project's tasks (pcUartTask / esp32Task / fpgaIfTask) are created by
 * App_CreateTasks() in app_tasks.c instead, so that regenerating code
 * from CubeMX (which only touches USER CODE sections) never disturbs
 * the application's own task set.
 */
#include "freertos.h"
#include "cmsis_os2.h"
#include "app_tasks.h"

void MX_FREERTOS_Init(void)
{
    App_Init();
    App_CreateTasks();
}
