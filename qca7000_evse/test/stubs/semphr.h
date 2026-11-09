#pragma once
#include "queue.h"
SemaphoreHandle_t xSemaphoreCreateBinary(void);
BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t, BaseType_t*);
