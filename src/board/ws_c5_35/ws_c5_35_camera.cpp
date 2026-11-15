// SPDX-License-Identifier: MIT
// See ws_c5_35_camera.h.
#include "ws_c5_35_camera.h"
#include "ws_c5_35_camera_regs.h"
#include "ws_c5_35_pins.h"
#include "../../chip/esp32c5_i2c_bus.h"
#include <Arduino.h>
#include <Wire.h>
#include <string.h>

namespace WsC5_35 {
namespace Camera {
namespace Bus = Esp32C5::I2cBus;

static bool s_present = false;

// Callers hold the bus lock.
static bool rawRead(uint8_t reg, uint8_t& value) {
    Bus::reopen();
    Wire.beginTransmission(CAM_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return false;
    if (Wire.requestFrom((int)CAM_ADDR, 1) != 1 || Wire.available() < 1) return false;
    value = (uint8_t)Wire.read();
    return true;
}
static bool rawWrite(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(CAM_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool present() { return s_present; }

bool begin() {
    s_present = false;
    Bus::Guard guard(100);
    if (!guard) return false;
    uint8_t hi = 0, lo = 0;
    if (!rawRead(REG_ID_HIGH, hi) || !rawRead(REG_ID_LOW, lo)) return false;
    s_present = (((uint16_t)hi << 8) | lo) == CHIP_ID;
    return s_present;
}

bool readReg(uint8_t reg, uint8_t& value) {
    if (!s_present) return false;
    Bus::Guard guard(100);
    if (!guard) return false;
    uint8_t v = 0;
    if (!rawRead(reg, v)) return false;
    value = v;
    return true;
}

bool writeReg(uint8_t reg, uint8_t value) {
    if (!s_present) return false;
    Bus::Guard guard(100);
    if (!guard) return false;
    return rawWrite(reg, value);
}

bool writeRegs(const RegValue* table, size_t count) {
    if (!s_present || !table) return false;
    Bus::Guard guard(1000);
    if (!guard) return false;
    for (size_t i = 0; i < count; ++i) {
        if (table[i].reg == REG_DELAY) { delay(table[i].value); continue; }
        if (!rawWrite(table[i].reg, table[i].value)) return false;
    }
    return true;
}

bool configure() {
    return writeRegs(kStartupTable, sizeof(kStartupTable) / sizeof(kStartupTable[0]));
}

// Register 0x09: 0x03 sends pictures, 0x13 is standby.
bool stream(bool on) { return writeReg(0x09, on ? 0x03 : 0x13); }

// Register 0x1e: bit 5 mirrors left-right, bit 4 flips top-bottom.
bool setMirrorFlip(bool mirror, bool flip) {
    if (!s_present) return false;
    Bus::Guard guard(100);
    if (!guard) return false;
    uint8_t v = 0;
    if (!rawRead(0x1E, v)) return false;
    v = (uint8_t)((v & ~0x30) | (mirror ? 0x20 : 0) | (flip ? 0x10 : 0));
    return rawWrite(0x1E, v);
}

bool decodeFrame(const uint8_t* raw, size_t rawBytes, uint8_t* out) {
    static const uint8_t frameHead[4] = {0xFF, 0xFF, 0xFF, 0x00};
    static const uint8_t lineHead[4]  = {0xFF, 0xFF, 0xFF, 0x40};
    if (!raw || !out || rawBytes < RAW_FRAME_BYTES) return false;
    if (memcmp(raw, frameHead, sizeof(frameHead)) != 0) return false;
    const uint8_t* src = raw + FRAME_HEADER_BYTES;
    const size_t lineBytes = (size_t)WIDTH * 2;
    for (int y = 0; y < HEIGHT; ++y) {
        if (memcmp(src, lineHead, sizeof(lineHead)) != 0) return false;
        src += LINE_HEADER_BYTES;
        memmove(out, src, lineBytes);       // out trails src, so in-place is safe
        src += lineBytes;
        out += lineBytes;
    }
    return true;
}

}  // namespace Camera
}  // namespace WsC5_35
