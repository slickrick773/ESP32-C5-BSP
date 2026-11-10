// SPDX-License-Identifier: MIT
// Host test for the 3.5-inch temperature/humidity driver. The mock serves
// bytes from the "register" named by the first command byte, so the ID reply
// lives at 0xEF.. and the measurement reply at 0x78..
#include <cmath>
#include <Wire.h>
#include <freertos/semphr.h>
#include "testutil.h"
#include "../../src/board/ws_c5_35/ws_c5_35_env.h"
#include "../../src/chip/esp32c5_i2c_bus.h"
namespace E = WsC5_35::Env;
static uint8_t* reg = g_wireRegisters[0x70];
static uint8_t crc(uint8_t a, uint8_t b) {
    uint8_t c = 0xFF; const uint8_t d[2] = {a, b};
    for (int i = 0; i < 2; ++i) { c ^= d[i]; for (int k = 0; k < 8; ++k) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x31) : (uint8_t)(c << 1); }
    return c;
}
static void put(int at, uint8_t a, uint8_t b) { reg[at] = a; reg[at + 1] = b; reg[at + 2] = crc(a, b); }
static bool near(float a, float b) { return std::fabs(a - b) < 0.05f; }
int main() {
    Esp32C5::I2cBus::begin(27, 26, 100000);
    g_wireRegisterMode = true;
    g_wireCommandDevice = true;   // two-byte writes select a reply; they do not store
    float t = -1, h = -1;

    CHECK(crc(0xBE, 0xEF) == 0x92, "checksum matches the published test vector");
    put(0xEF, 0x12, 0x34);                      // valid checksum, wrong identity
    CHECK(!E::begin() && !E::present(), "wrong identity rejected");
    CHECK(!E::read(t, h), "read rejected before setup");
    put(0xEF, 0x08, 0x87); reg[0xF1] ^= 0xFF;   // right identity, bad checksum
    CHECK(!E::begin(), "identity with a bad checksum rejected");
    put(0xEF, 0x08, 0x87);                      // the ID observed on hardware
    g_takeResult = pdFALSE;
    CHECK(!E::begin(), "begin reports lock failure");
    g_takeResult = pdTRUE;
    CHECK(E::begin() && E::present(), "sensor detected");

    put(0x78, 0x66, 0x66);                      // 0x6666 -> 25.0 C
    put(0x7B, 0x80, 0x00);                      // 0x8000 -> 50.0 %
    CHECK(E::read(t, h), "measurement succeeds");
    CHECK(near(t, 25.0f) && near(h, 50.0f), "temperature and humidity converted");

    reg[0x7A] ^= 0x01;                          // corrupt the temperature checksum
    float kt = 9, kh = 9;
    CHECK(!E::read(kt, kh) && near(kt, 9) && near(kh, 9), "bad temperature checksum rejected, outputs unchanged");
    put(0x78, 0x66, 0x66); reg[0x7D] ^= 0x01;   // corrupt the humidity checksum
    CHECK(!E::read(kt, kh) && near(kh, 9), "bad humidity checksum rejected");
    put(0x7B, 0x80, 0x00);
    g_wireShortRead = 1;
    CHECK(!E::read(kt, kh), "short read rejected");
    g_wireShortRead = 0;
    g_takeResult = pdFALSE;
    const unsigned before = g_wireRequestCount;
    CHECK(!E::read(kt, kh) && g_wireRequestCount == before, "lock failure performs no read");
    g_takeResult = pdTRUE;
    CHECK(E::read(t, h) && near(t, 25.0f), "reads again after failures");
    return g_failures ? 1 : 0;
}
