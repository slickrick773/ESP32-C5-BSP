// SPDX-License-Identifier: MIT
// ESP32-C5 task helpers — chip-level, reusable across any C5 board.
//
// The C5 is single-core. FreeRTOS runs unicore, so core-pinning code copied
// from a dual-core ESP32/S3 is a quiet trap: xTaskCreatePinnedToCore(..., 1)
// asks for core 1, which does not exist here, and the request is silently
// treated as "no affinity". That is not wrong on the C5 itself, but it hides
// the assumption that bites the moment the code moves back to a dual-core part.
// workerCore() / createPinnedTask() keep the intent ("pin background work off
// the core the UI/radio uses, if there is one") honest on both.
#pragma once

#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace Esp32C5 {

// CPU cores on this chip (1 on the C5).
inline int  coreCount()    { return portNUM_PROCESSORS; }
inline bool isSingleCore() { return portNUM_PROCESSORS <= 1; }

// The core background/worker tasks should pin to: core 1 on a dual-core part,
// core 0 on a single-core part. Use this instead of a hard-coded 1 so the same
// source is correct on the C5 and on an S3/ESP32.
inline BaseType_t workerCore() { return portNUM_PROCESSORS > 1 ? 1 : 0; }

// Create a task pinned chip-aware (to workerCore()). stackBytes is in BYTES
// (the ESP-IDF xTaskCreatePinnedToCore convention, not vanilla-FreeRTOS words).
// Returns pdPASS on success. Fills handle if non-null.
BaseType_t createPinnedTask(TaskFunction_t fn, const char* name,
                            uint32_t stackBytes, void* arg,
                            UBaseType_t priority, TaskHandle_t* handle = nullptr);

// One-line chip summary over Serial: model, core count, CPU MHz, heap.
void printChipInfo();

}  // namespace Esp32C5
