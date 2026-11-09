#pragma once
#include <stdint.h>
typedef uint32_t TickType_t; typedef long BaseType_t; typedef unsigned long UBaseType_t;
typedef void* TaskHandle_t; typedef void* QueueHandle_t; typedef void* SemaphoreHandle_t;
typedef enum { eNoAction, eSetBits } eNotifyAction;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY 0xFFFFFFFFu
#define pdMS_TO_TICKS(x) ((TickType_t)(x))
#define configMAX_PRIORITIES 7
#define tskIDLE_PRIORITY 0
#define portYIELD_FROM_ISR(x) (void)(x)
#define taskENTER_CRITICAL() do{}while(0)
#define taskEXIT_CRITICAL() do{}while(0)
