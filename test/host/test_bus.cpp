// SPDX-License-Identifier: MIT
// Host tests for the shared-bus core: honest recover() result + lock discipline.
// Compiles against the REAL src/chip/esp32c5_i2c_bus.cpp.
#include <cstdio>
#include <Arduino.h>
#include <freertos/semphr.h>
#include "testutil.h"
#include "../../src/chip/esp32c5_i2c_bus.h"

namespace Bus = Esp32C5::I2cBus;

int main() {
    g_failures = 0;

    // Before begin(): the bus is uninitialised, so recovery cannot run and must
    // report failure rather than claim success. (Checked before begin() below.)
    std::printf("recover() before begin():\n");
    CHECK(Bus::recover() == false, "recover() == false before begin() (uninitialised)");

    Bus::begin(27, 26, 100000);   // sets pins, creates the mutex, marks the bus up

    std::printf("recover() returns an observable success condition:\n");

    // Still stuck: SDA never releases -> recovery failed -> false.
    g_sdaMode = SDA_STUCK_LOW; g_sclWriteCount = 0;
    CHECK(Bus::recover() == false, "recover() == false when SDA stays held low");

    // Not wedged: SDA already released -> bus usable -> true.
    g_sdaMode = SDA_HIGH; g_sclWriteCount = 0;
    CHECK(Bus::recover() == true, "recover() == true when SDA is already released");

    // Freed by the pulses: SDA goes high after a few SCL edges -> true.
    g_sdaMode = SDA_RELEASE_AFTER; g_sdaReleaseAtSclWrites = 6; g_sclWriteCount = 0;
    CHECK(Bus::recover() == true, "recover() == true when the clock pulses free SDA");

    std::printf("lock discipline: no bus op on failed acquisition:\n");

    // Failed lock -> the guarded body must not run.
    g_takeResult = pdFALSE;
    int busOps = 0;
    { Bus::Guard g(0); if (g) busOps++; }
    CHECK(busOps == 0, "Guard body skipped (no bus op) when lock acquisition fails");

    // Acquired lock -> the guarded body runs once.
    g_takeResult = pdTRUE;
    busOps = 0;
    { Bus::Guard g(0); if (g) busOps++; }
    CHECK(busOps == 1, "Guard body runs when the lock is acquired");

    std::printf("lock lifetime: acquire pairs with exactly one release:\n");

    // A Guard that takes the lock releases it (one give) on scope exit.
    g_takeResult = pdTRUE;
    int gives = g_giveCount;
    { Bus::Guard g(0); }
    CHECK(g_giveCount == gives + 1, "Guard releases the lock (one give) on scope exit");

    // A Guard that fails to take the lock must NOT release one it never held.
    g_takeResult = pdFALSE;
    gives = g_giveCount;
    { Bus::Guard g(0); }
    CHECK(g_giveCount == gives, "a failed Guard does not release a lock it never held");

    std::printf("test_bus: %d failure(s)\n", g_failures);
    return g_failures;
}
