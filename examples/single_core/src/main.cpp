// SPDX-License-Identifier: MIT
// single_core — prove the C5 is unicore and that the BSP's task helper pins a
// task to the only core. A worker task created via createPinnedTask() and the
// main loop both report their core id; on the C5 both are core 0, coreCount is
// 1, and workerCore() is 0 (not the 1 a dual-core habit would hard-code).
//
// Console: USB-Serial-JTAG at 115200.
#include <Arduino.h>
#include <chip/esp32c5_tasks.h>

using namespace Esp32C5;

static void worker(void* /*arg*/) {
    for (;;) {
        Serial.printf("[worker] running on core %d\n", xPortGetCoreID());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    printChipInfo();
    Serial.printf("[core] coreCount=%d  isSingleCore=%d  workerCore=%d\n",
                  coreCount(), (int)isSingleCore(), (int)workerCore());

    const BaseType_t ok = createPinnedTask(worker, "worker", 4096, nullptr, 1);
    Serial.printf("[core] createPinnedTask -> %s (pinned to core %d)\n",
                  ok == pdPASS ? "pdPASS" : "FAILED", (int)workerCore());
}

void loop() {
    static uint32_t last = 0;
    const uint32_t now = millis();
    if (now - last >= 1000) {
        last = now;
        Serial.printf("[loop] core %d of %d (workerCore=%d)\n",
                      xPortGetCoreID(), coreCount(), (int)workerCore());
    }
}
