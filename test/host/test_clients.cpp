// SPDX-License-Identifier: MIT
// Host tests for the locking policy on runtime bus clients: a failed lock must
// perform NO bus operation. Compiles against the REAL battery + expander drivers.
#include <cstdio>
#include <Arduino.h>
#include <Wire.h>
#include <XPowersLib.h>
#include <freertos/semphr.h>
#include "testutil.h"
#include "../../src/chip/esp32c5_i2c_bus.h"
#include "../../src/board/ws_c5_35/ws_c5_35_battery.h"
#include "../../src/board/ws_c5_35/ws_c5_35_expander.h"

namespace Bus = Esp32C5::I2cBus;

int main() {
    g_failures = 0;
    Bus::begin(27, 26, 100000);

    // ---- Battery::refresh() ----
    std::printf("Battery::refresh() honours the bus lock:\n");
    g_pmuBeginResult = 1;
    g_takeResult = pdTRUE;
    CHECK(WsC5_35::Battery::begin() == true, "Battery::begin() succeeds when the PMU answers");

    // Lock FAILS: refresh must read nothing and must not reopen the bus.
    g_takeResult = pdFALSE;
    g_pmuReadCalls = 0; g_wireBeginCount = 0; g_wireEndCount = 0;
    WsC5_35::Battery::refresh();
    CHECK(g_pmuReadCalls == 0, "refresh() reads no PMU registers when the lock fails");
    CHECK(g_wireBeginCount == 0 && g_wireEndCount == 0, "refresh() does not reopen the bus when the lock fails");

    // Lock OK: refresh reads the six snapshot values and reopens once.
    g_takeResult = pdTRUE;
    g_pmuReadCalls = 0; g_wireBeginCount = 0;
    WsC5_35::Battery::refresh();
    CHECK(g_pmuReadCalls == 6, "refresh() reads the PMU when the lock is acquired");
    CHECK(g_wireBeginCount >= 1, "refresh() reopens the bus when the lock is acquired");

    // ---- Expander::backlight() ----
    std::printf("Expander::backlight() honours the bus lock:\n");
    g_wireEndTransStatus = 0;                 // every init write ACKs
    WsC5_35::Expander::begin();
    CHECK(WsC5_35::Expander::present() == true, "Expander::begin() marks the expander ready");

    // Lock FAILS: no write to the PWM register.
    g_takeResult = pdFALSE;
    g_wireWriteCount = 0;
    WsC5_35::Expander::backlight(0x80);
    CHECK(g_wireWriteCount == 0, "backlight() writes nothing when the lock fails");

    // Lock OK: the register write goes out.
    g_takeResult = pdTRUE;
    g_wireWriteCount = 0;
    WsC5_35::Expander::backlight(0x80);
    CHECK(g_wireWriteCount >= 1, "backlight() writes the PWM register when the lock is acquired");

    std::printf("test_clients: %d failure(s)\n", g_failures);
    return g_failures;
}
