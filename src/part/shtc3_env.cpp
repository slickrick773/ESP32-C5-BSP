// SPDX-License-Identifier: MIT
// See shtc3_env.h.
#include "shtc3_env.h"
#include "../chip/esp32c5_i2c_bus.h"
#include <Arduino.h>
#include <Wire.h>

namespace C5Parts {
namespace Env {
namespace Bus = Esp32C5::I2cBus;

static bool s_present = false;

static bool command(uint8_t hi, uint8_t lo) {
    Wire.beginTransmission(ADDR);
    Wire.write(hi);
    Wire.write(lo);
    return Wire.endTransmission() == 0;
}
static bool readBytes(uint8_t* out, int n) {
    if (Wire.requestFrom((int)ADDR, n) != n || Wire.available() < n) return false;
    for (int i = 0; i < n; ++i) out[i] = (uint8_t)Wire.read();
    return true;
}
// Sensirion CRC-8: polynomial 0x31, initial value 0xFF, over two bytes.
static uint8_t crc8(const uint8_t* d) {
    uint8_t crc = 0xFF;
    for (int i = 0; i < 2; ++i) {
        crc ^= d[i];
        for (int b = 0; b < 8; ++b) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
    return crc;
}

bool present() { return s_present; }

bool begin() {
    s_present = false;
    Bus::Guard guard(100);
    if (!guard) return false;
    Bus::reopen();
    command(0x35, 0x17);                     // wake (ignored if already awake)
    delay(1);
    uint8_t id[3];
    if (!command(0xEF, 0xC8) || !readBytes(id, 3)) return false;
    if (crc8(id) != id[2]) return false;
    // SHTC3 identity: bits 11 and 5..0 of the ID word are fixed (xxxx 1xxx xx00 0111).
    s_present = ((((uint16_t)id[0] << 8) | id[1]) & 0x083F) == 0x0807;
    command(0xB0, 0x98);                     // back to sleep
    return s_present;
}

bool read(float& temperatureC, float& humidityPercent) {
    if (!s_present) return false;
    Bus::Guard guard(100);
    if (!guard) return false;
    Bus::reopen();
    command(0x35, 0x17);                     // wake
    delay(1);
    uint8_t d[6];
    // Normal mode, temperature first, no clock stretching; conversion ~12 ms.
    bool ok = command(0x78, 0x66);
    if (ok) { delay(15); ok = readBytes(d, 6); }
    command(0xB0, 0x98);                     // sleep, even after a failure
    if (!ok || crc8(d) != d[2] || crc8(d + 3) != d[5]) return false;
    temperatureC = -45.0f + 175.0f * (float)(((uint16_t)d[0] << 8) | d[1]) / 65536.0f;
    humidityPercent = 100.0f * (float)(((uint16_t)d[3] << 8) | d[4]) / 65536.0f;
    return true;
}
}  // namespace Env
}  // namespace C5Parts
