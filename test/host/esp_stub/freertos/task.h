/* Host stand-in for freertos/task.h. */
#ifndef FREERTOS_TASK_H
#define FREERTOS_TASK_H
#include "freertos/FreeRTOS.h"
BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stack, void *arg,
                       UBaseType_t prio, TaskHandle_t *out);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
UBaseType_t uxTaskPriorityGet(TaskHandle_t task);
void vTaskDelete(TaskHandle_t task);
void vTaskDelay(TickType_t ticks);
#endif
