// SPDX-License-Identifier: MIT
// Host-test mock of the FreeRTOS semaphore surface. xSemaphoreTake returns the
// test-controlled g_takeResult, so a test can simulate a contended bus.
#pragma once
#include "FreeRTOS.h"

typedef void* SemaphoreHandle_t;

extern int g_takeResult;   // value xSemaphoreTake returns (pdTRUE / pdFALSE)

SemaphoreHandle_t xSemaphoreCreateMutex();
int               xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticks);
void              xSemaphoreGive(SemaphoreHandle_t s);
