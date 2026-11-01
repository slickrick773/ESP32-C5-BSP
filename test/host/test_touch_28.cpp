// SPDX-License-Identifier: MIT
// Host test for the CST3530 (2.8" board) read path: a short/partial I2C packet
// must never be published as a coordinate, and a complete packet must decode to
// the native-portrait coordinate the bit layout implies. Compiles against the
// REAL 2.8" touch driver, paralleling test_touch.cpp (the FT6336 / 3.5" one).
#include <cstdio>
#include <Arduino.h>
#include <Wire.h>
#include <freertos/semphr.h>
#include "testutil.h"
#include "../../src/chip/esp32c5_i2c_bus.h"
#include "../../src/board/ws_c5_28/ws_c5_28_touch.h"

namespace Bus = Esp32C5::I2cBus;

// Build a 9-byte CST3530 first-contact packet (register 0xD0070000 layout):
//   p[3]&0x0F = count, p[8]&0xF0 = finger-down,
//   nx = ((p[7]&0x0F)<<8)|p[4], ny = ((p[7]&0xF0)<<4)|p[5].
static void packet(uint8_t out[9], unsigned count, bool down, uint16_t nx, uint16_t ny) {
    for (int i = 0; i < 9; i++) out[i] = 0;
    out[3] = (uint8_t)(count & 0x0F);
    out[4] = (uint8_t)(nx & 0xFF);
    out[5] = (uint8_t)(ny & 0xFF);
    out[7] = (uint8_t)(((nx >> 8) & 0x0F) | ((ny >> 4) & 0xF0));
    out[8] = down ? 0xF0 : 0x00;
}

int main() {
    g_failures = 0;
    Bus::begin(0, 1, 100000);     // 2.8" bus pins (SDA 0 / SCL 1)
    g_takeResult = pdTRUE;        // read() takes lock(0); let it succeed
    g_wireEndTransStatus = 0;     // register-address writes and the ack all ACK

    // The driver only reads when it believes the chip is present. present() is
    // set by begin(); on the host, begin()'s probe endTransmission ACKs (status
    // 0), so the chip reads as present and read() proceeds.
    WsC5_28::Touch::begin();

    std::printf("Touch::read() rejects a short packet:\n");
    uint16_t x = 0xFFFF, y = 0xFFFF;
    uint8_t full[9];
    packet(full, 1, true, 100, 200);
    // requestFrom reports only 4 of 9 bytes: a dropped transfer -> must reject.
    mockWireSetResponse(4, full, 9);
    bool got = WsC5_28::Touch::read(x, y);
    CHECK(got == false, "read() returns false on a short (partial) packet");
    CHECK(x == 0xFFFF && y == 0xFFFF, "read() publishes no coordinate on a short read");

    std::printf("Touch::read() accepts a complete packet:\n");
    x = 0; y = 0;
    packet(full, 1, true, 100, 200);
    mockWireSetResponse(9, full, 9);
    got = WsC5_28::Touch::read(x, y);
    CHECK(got == true, "read() returns true on a complete 1-contact packet");
    CHECK(x == 100 && y == 200, "read() decodes the correct native coordinate");

    std::printf("Touch::read() treats finger-up as no touch:\n");
    x = 0xFFFF; y = 0xFFFF;
    packet(full, 0, false, 0, 0);     // count 0, down flag clear -> a real lift
    mockWireSetResponse(9, full, 9);
    got = WsC5_28::Touch::read(x, y);
    CHECK(got == false, "read() returns false on a finger-up packet");
    CHECK(x == 0xFFFF && y == 0xFFFF, "read() publishes no coordinate on finger-up");

    std::printf("Touch::read() rejects an out-of-range coordinate:\n");
    x = 0xFFFF; y = 0xFFFF;
    packet(full, 1, true, 240, 0);    // nx == PANEL_W -> out of bounds
    mockWireSetResponse(9, full, 9);
    got = WsC5_28::Touch::read(x, y);
    CHECK(got == false, "read() returns false when nx >= 240");

    std::printf("Touch::toLandscape() maps native->landscape at rotation 1:\n");
    int sx = -1, sy = -1;
    WsC5_28::Touch::toLandscape(100, 200, sx, sy);
    // sx = y = 200; sy = (PANEL_W-1) - x = 239 - 100 = 139.
    CHECK(sx == 200 && sy == 139, "toLandscape(100,200) -> (200,139)");

    std::printf("test_touch_28: %d failure(s)\n", g_failures);
    return g_failures;
}
