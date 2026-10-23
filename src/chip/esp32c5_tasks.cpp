// SPDX-License-Identifier: MIT
// See esp32c5_tasks.h.
#include "esp32c5_tasks.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

namespace Esp32C5 {

BaseType_t createPinnedTask(TaskFunction_t fn, const char* name,
                            uint32_t stackBytes, void* arg,
                            UBaseType_t priority, TaskHandle_t* handle) {
    return xTaskCreatePinnedToCore(fn, name, stackBytes, arg, priority, handle, workerCore());
}

void printChipInfo() {
    Serial.printf("[chip] %s, %d core(s), %lu MHz, heap %lu free / %lu largest\n",
                  ESP.getChipModel(), coreCount(),
                  (unsigned long)getCpuFrequencyMhz(),
                  (unsigned long)ESP.getFreeHeap(),
                  (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

}  // namespace Esp32C5
