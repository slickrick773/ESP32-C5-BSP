// SPDX-License-Identifier: MIT
// Host test for touch short-read rejection: a partial I2C packet must never be
// published as a coordinate. Compiles against the REAL touch driver.
#include <cstdio>
#include <Arduino.h>
#include <Wire.h>
#include <freertos/semphr.h>
#include "testutil.h"
#include "../../src/chip/esp32c5_i2c_bus.h"
#include "../../src/board/ws_c5_35/ws_c5_35_touch.h"

namespace Bus = Esp32C5::I2cBus;

int main() {
    g_failures = 0;
    Bus::begin(27, 26, 100000);
    g_takeResult = pdTRUE;        // Touch::read takes lock(0); let it succeed so we reach ftRead
    g_wireEndTransStatus = 0;     // the register-address write ACKs

    std::printf("Touch::read() rejects a short packet:\n");

    // Short read: TD_STATUS wants 7 bytes but only 3 arrive. Old code zero-filled
    // the rest and returned a point near (0,0); now it must reject the read.
    uint16_t x = 0xFFFF, y = 0xFFFF;
    const uint8_t partial[3] = { 0x01, 0x00, 0x64 };   // "1 point" then truncated
    mockWireSetResponse(3, partial, 3);
    bool got = WsC5_35::Touch::read(x, y);
    CHECK(got == false, "read() returns false on a short (partial) packet");
    CHECK(x == 0xFFFF && y == 0xFFFF, "read() publishes no coordinate on a short read");

    // Full read: 7 bytes, one valid point at (100, 200).
    std::printf("Touch::read() accepts a complete packet:\n");
    x = 0; y = 0;
    const uint8_t full[7] = { 0x01, 0x00, 0x64, 0x00, 0xC8, 0x00, 0x00 };
    mockWireSetResponse(7, full, 7);
    got = WsC5_35::Touch::read(x, y);
    CHECK(got == true, "read() returns true on a complete packet");
    CHECK(x == 100 && y == 200, "read() parses the correct coordinate on a full read");

    std::printf("test_touch: %d failure(s)\n", g_failures);
    return g_failures;
}
