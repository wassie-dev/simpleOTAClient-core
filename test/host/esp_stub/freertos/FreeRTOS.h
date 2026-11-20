/* Host stand-in for the FreeRTOS types and calls the port uses. */
#ifndef FREERTOS_H
#define FREERTOS_H
#include <stdint.h>
typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef uint32_t TickType_t;
typedef void *TaskHandle_t;
typedef void *SemaphoreHandle_t;
typedef struct { int unused; } StaticSemaphore_t;
typedef void (*TaskFunction_t)(void *);
#define pdPASS 1
#define pdTRUE 1
#define portMAX_DELAY 0xffffffffu
#endif
