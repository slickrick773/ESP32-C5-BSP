// SPDX-License-Identifier: MIT
// See ws_c5_28_touch.h. Read path + decode mirrored from the umbravox 2.8"
// port (device/touch.cpp CST3530 branch, device/cst3530_sample.h), which is
// hardware-accepted on the physical 2.8" board. Pending BSP hardware-check.
#include "ws_c5_28_touch.h"
#include "ws_c5_28_pins.h"
#include "ws_c5_28_expander.h"
#include "../../chip/esp32c5_i2c_bus.h"
#include <Arduino.h>
#include <Wire.h>

namespace WsC5_28 {
namespace Touch {

namespace Bus = Esp32C5::I2cBus;

static bool s_present = false;

// Outcome of decoding one 9-byte packet, kept distinct (as in the umbravox
// port) so a genuine finger-up (Lift) is told apart from a garbage/short read
// (Fail). read() reports both as "no touch"; the distinction documents intent
// and leaves room for an immediate-release refinement once verified on hw.
enum class Decoded : uint8_t { Touch, Lift, Fail };

// Decode Waveshare's CST3530 first-contact packet (register 0xD0070000, 9
// bytes). Bit layout from cst3530_sample.h:
//   p[3]&0x0F     = contact count (0..5)
//   p[8]&0xF0     = finger-down flag
//   nx = ((p[7]&0x0F)<<8) | p[4]      native X (0..239)
//   ny = ((p[7]&0xF0)<<4) | p[5]      native Y (0..319)
static Decoded decodePacket(const uint8_t* p, uint16_t& nx, uint16_t& ny) {
    const unsigned count = p[3] & 0x0F;
    if (count > 5) return Decoded::Fail;
    if (count == 0 || !(p[8] & 0xF0)) return Decoded::Lift;
    const uint16_t x = (uint16_t)(((p[7] & 0x0F) << 8) | p[4]);
    const uint16_t y = (uint16_t)(((p[7] & 0xF0) << 4) | p[5]);
    if (x >= PANEL_W || y >= PANEL_H) return Decoded::Fail;
    nx = x; ny = y;
    return Decoded::Touch;
}

// Write a 32-bit CST3530 register address, MSB first. `stop` false leaves the
// bus held for a repeated START (the read); true finishes with a STOP.
static bool writeReg(uint32_t reg, bool stop) {
    Wire.beginTransmission(CST3530_ADDR);
    for (int shift = 24; shift >= 0; shift -= 8) Wire.write((uint8_t)(reg >> shift));
    return Wire.endTransmission(stop) == 0;
}

// One sensor read: address the contact register with a repeated START, pull the
// 9-byte packet, then acknowledge the packet with a STOP write (the CST3530
// expects every complete packet to be acked). Returns the decoded outcome.
//
// Note: NO per-read Wire.end()/begin() reopen here. The 3.5"'s FT6336 needs
// that C5 workaround; the 2.8"'s CST3530 repeated-START read is proven to work
// without it on the umbravox 2.8" board, so this path mirrors that. To be
// re-confirmed on BSP hardware.
static Decoded sample(uint16_t& nx, uint16_t& ny) {
    uint8_t data[9];
    if (!writeReg(CST3530_REG_READ, false)) return Decoded::Fail;
    if (Wire.requestFrom((int)CST3530_ADDR, 9) != 9) {
        while (Wire.available()) Wire.read();
        writeReg(CST3530_REG_ACK, true);          // still ack so the chip rearms
        return Decoded::Fail;
    }
    for (uint8_t i = 0; i < 9; i++) data[i] = (uint8_t)Wire.read();
    if (!writeReg(CST3530_REG_ACK, true)) return Decoded::Fail;
    return decodePacket(data, nx, ny);
}

bool present() { return s_present; }

void toLandscape(uint16_t x, uint16_t y, int& sx, int& sy) {
    sx = y;
    sy = (PANEL_W - 1) - (int)x;
}

bool begin() {
    // The touch reset lives on the expander; its begin() is idempotent and also
    // opens the shared bus (via the I2C core).
    Expander::begin();

    pinMode(CST3530_INT_GPIO, INPUT);

    Wire.beginTransmission(CST3530_ADDR);
    s_present = (Wire.endTransmission() == 0);
    Serial.printf("[ws-c5-28] CST3530 on I2C 0x58 %s\n", s_present ? "(OK)" : "(not found)");
    return s_present;
}

bool read(uint16_t& x, uint16_t& y) {
    if (!s_present) return false;

    // Skip this frame rather than stall if another owner (e.g. a GPS cycle)
    // holds the bus -- poll()'s grace bridges the gap so it is not felt.
    if (!Bus::lock(0)) return false;

    uint16_t nx = 0, ny = 0;
    const Decoded d = sample(nx, ny);

    Bus::unlock();

    if (d != Decoded::Touch) return false;
    x = nx; y = ny;
    return true;
}

bool poll(uint16_t& x, uint16_t& y, uint16_t graceMs, uint16_t minReadIntervalMs) {
    static uint16_t s_lastX = 0, s_lastY = 0;
    static uint32_t s_lastOk = 0, s_lastRead = 0;

    const uint32_t now = millis();
    // Pace the bus: hit the CST3530 at most every minReadIntervalMs. Between
    // real reads we bridge with the last contact below.
    if ((uint32_t)(now - s_lastRead) >= minReadIntervalMs) {
        s_lastRead = now;
        if (read(x, y)) {
            s_lastX = x; s_lastY = y; s_lastOk = now;
            return true;
        }
    }
    // Bridge a dropped or skipped read: hold the last contact for graceMs so a
    // drag does not let go and a tap does not feel stiff. A real release (no
    // good read for graceMs) outlasts it and reads as finger-up.
    if (s_lastOk && (uint32_t)(now - s_lastOk) < graceMs) {
        x = s_lastX; y = s_lastY;
        return true;
    }
    return false;
}

}  // namespace Touch
}  // namespace WsC5_28
