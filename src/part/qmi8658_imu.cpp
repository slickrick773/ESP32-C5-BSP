// SPDX-License-Identifier: MIT
// See qmi8658_imu.h.
#include "qmi8658_imu.h"
#include "../chip/esp32c5_i2c_bus.h"
#include <Wire.h>

namespace C5Parts {
namespace Imu {
namespace Bus = Esp32C5::I2cBus;

static bool s_present = false;
static constexpr float ACCEL_LSB_PER_G = 8192.0f;    // ±4 g
static constexpr float GYRO_LSB_PER_DPS = 64.0f;     // ±512 dps

static bool writeReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}
static bool readRegs(uint8_t reg, uint8_t* out, int n) {
    Wire.beginTransmission(ADDR);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return false;
    if (Wire.requestFrom((int)ADDR, n) != n || Wire.available() < n) return false;
    for (int i = 0; i < n; ++i) out[i] = (uint8_t)Wire.read();
    return true;
}

bool present() { return s_present; }

bool begin() {
    s_present = false;
    Bus::Guard guard(100);
    if (!guard) return false;
    Bus::reopen();
    uint8_t who = 0;
    if (!readRegs(0x00, &who, 1) || who != WHO_AM_I) return false;
    bool ok = writeReg(0x02, 0x40);          // CTRL1: register auto-increment
    if (ok) ok = writeReg(0x03, 0x16);       // CTRL2: accel ±4 g, 112 Hz
    if (ok) ok = writeReg(0x04, 0x56);       // CTRL3: gyro ±512 dps, 112 Hz
    if (ok) ok = writeReg(0x08, 0x03);       // CTRL7: accel + gyro on
    s_present = ok;
    return ok;
}

bool read(Sample& out) {
    if (!s_present) return false;
    Bus::Guard guard(100);
    if (!guard) return false;
    Bus::reopen();
    uint8_t r[12];
    if (!readRegs(0x35, r, 12)) return false;   // AX_L .. GZ_H, little-endian
    auto word = [&](int i) { return (int16_t)(r[i] | (r[i + 1] << 8)); };
    out.ax = word(0) / ACCEL_LSB_PER_G;
    out.ay = word(2) / ACCEL_LSB_PER_G;
    out.az = word(4) / ACCEL_LSB_PER_G;
    out.gx = word(6) / GYRO_LSB_PER_DPS;
    out.gy = word(8) / GYRO_LSB_PER_DPS;
    out.gz = word(10) / GYRO_LSB_PER_DPS;
    return true;
}

bool end() {
    Bus::Guard guard(100);
    if (!guard) return false;
    Bus::reopen();
    s_present = false;
    return writeReg(0x08, 0x00);             // CTRL7: both sensors off
}
}  // namespace Imu
}  // namespace C5Parts
