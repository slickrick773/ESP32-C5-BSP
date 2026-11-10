// SPDX-License-Identifier: MIT
// Host test for the 3.5-inch IMU driver against a mocked register map.
#include <cmath>
#include <Wire.h>
#include <freertos/semphr.h>
#include "testutil.h"
#include "../../src/board/ws_c5_35/ws_c5_35_imu.h"
#include "../../src/chip/esp32c5_i2c_bus.h"
namespace I = WsC5_35::Imu;
static uint8_t* reg = g_wireRegisters[0x6B];
static void put(int r, int16_t v) { reg[r] = (uint8_t)(v & 0xFF); reg[r + 1] = (uint8_t)((uint16_t)v >> 8); }
static bool near(float a, float b) { return std::fabs(a - b) < 0.001f; }
int main() {
    Esp32C5::I2cBus::begin(27, 26, 100000);
    g_wireRegisterMode = true;
    I::Sample s{};

    reg[0x00] = 0x00;
    CHECK(!I::begin() && !I::present(), "wrong identity rejected");
    CHECK(g_wireWritesLen == 0, "no configuration written on wrong identity");
    CHECK(!I::read(s), "read rejected before setup");
    reg[0x00] = 0x05;
    g_takeResult = pdFALSE;
    CHECK(!I::begin(), "begin reports lock failure");
    g_takeResult = pdTRUE;
    g_wireFailReg = 0x04;
    CHECK(!I::begin() && !I::present(), "configuration write failure rejected");
    g_wireFailReg = -1;
    CHECK(I::begin() && I::present(), "setup succeeds");
    CHECK(reg[0x02] == 0x40 && reg[0x03] == 0x16 && reg[0x04] == 0x56 && reg[0x08] == 0x03,
          "auto-increment, ranges and enables written");

    put(0x35, 0); put(0x37, -4096); put(0x39, 8192);      // 0, -0.5, +1 g
    put(0x3B, 64); put(0x3D, -640); put(0x3F, 0);         // +1, -10, 0 dps
    CHECK(I::read(s), "sample read succeeds");
    CHECK(near(s.ax, 0) && near(s.ay, -0.5f) && near(s.az, 1.0f), "acceleration scaled to g");
    CHECK(near(s.gx, 1.0f) && near(s.gy, -10.0f) && near(s.gz, 0), "angular rate scaled to dps");

    g_wireShortRead = 1;
    I::Sample keep{9, 9, 9, 9, 9, 9};
    CHECK(!I::read(keep) && near(keep.ax, 9), "short read rejected, caller value preserved");
    g_wireShortRead = 0;
    g_takeResult = pdFALSE;
    const unsigned before = g_wireRequestCount;
    CHECK(!I::read(keep) && g_wireRequestCount == before, "lock failure performs no read");
    CHECK(!I::end() && I::present(), "end reports lock failure and keeps state");
    g_takeResult = pdTRUE;
    CHECK(I::end() && !I::present() && reg[0x08] == 0x00, "end disables both sensors");
    CHECK(!I::read(s), "read rejected after end");
    return g_failures ? 1 : 0;
}
