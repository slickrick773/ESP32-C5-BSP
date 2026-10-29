// SPDX-License-Identifier: MIT
// Host-test mock of the FreeRTOS surface the bus lock uses.
#pragma once
#include <stdint.h>

typedef uint32_t TickType_t;
#define portMAX_DELAY     ((TickType_t)0xffffffff)
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define pdTRUE  1
#define pdFALSE 0
