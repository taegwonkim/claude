#pragma once
#include "FreeRTOS.h"
TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t);
BaseType_t xTaskCreate(void(*)(void*), const char*, uint32_t, void*, UBaseType_t, TaskHandle_t*);
BaseType_t xTaskNotifyWait(uint32_t, uint32_t, uint32_t*, TickType_t);
BaseType_t xTaskNotify(TaskHandle_t, uint32_t, eNotifyAction);
BaseType_t xTaskNotifyFromISR(TaskHandle_t, uint32_t, eNotifyAction, BaseType_t*);
