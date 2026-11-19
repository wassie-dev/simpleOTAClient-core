/* Host stand-in for freertos/semphr.h. */
#ifndef FREERTOS_SEMPHR_H
#define FREERTOS_SEMPHR_H
#include "freertos/FreeRTOS.h"
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *buf);
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem);
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t ticks);
#endif
