// SPDX-License-Identifier: MIT
// See ws_c5_35_expander.h. Bring-up sequence verified on hardware.
#include "ws_c5_35_expander.h"
#include "ws_c5_35_pins.h"
#include "../../chip/esp32c5_i2c_bus.h"
#include <Arduino.h>
#include <Wire.h>

namespace WsC5_35 {
namespace Expander {

static uint16_t s_output = 0;      // output latch; the high byte is E8..E15
static uint16_t s_mode   = 0;      // 1 = output
static bool     s_wide   = false;  // header lines in use: send both bytes
static bool     s_ready  = false;

static bool exioWrite(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(EXIO_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

static bool exioSet(uint8_t pin, bool level) {
    if (level) s_output |= (uint16_t)(1U << pin);
    else       s_output &= (uint16_t)~(1U << pin);
    return exioWrite(EXIO_OUT_REG, (uint8_t)s_output);
}

// Both bytes, low byte first. Used only once a header line is in use, so
// start-up and boards that never touch the header keep the one-byte writes.
static bool exioWrite16(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(EXIO_ADDR);
    Wire.write(reg);
    Wire.write((uint8_t)(value & 0xFF));
    Wire.write((uint8_t)(value >> 8));
    return Wire.endTransmission() == 0;
}

static bool pushOutput() {
    return s_wide ? exioWrite16(EXIO_OUT_REG, s_output)
                  : exioWrite(EXIO_OUT_REG, (uint8_t)s_output);
}

// Caller holds the bus lock.
static bool readInputs(uint16_t& value) {
    Esp32C5::I2cBus::reopen();
    Wire.beginTransmission(EXIO_ADDR);
    Wire.write(EXIO_IN_REG);
    if (Wire.endTransmission(true) != 0) return false;
    if (Wire.requestFrom((int)EXIO_ADDR, 2) != 2) return false;
    const uint8_t lo = (uint8_t)Wire.read();
    const uint8_t hi = (uint8_t)Wire.read();
    value = (uint16_t)(lo | (hi << 8));
    return true;
}

static bool headerLine(uint8_t line) { return line >= HEADER_FIRST && line <= HEADER_LAST; }

bool present() { return s_ready; }

bool begin(uint8_t initialBacklight) {
    // Run ONCE. Pulsing EXIO1 (LCD reset) again after the panel is initialised
    // would blank it, so this guard makes a repeat call a safe no-op.
    if (s_ready) return true;

    // Open the shared bus through the C5 I2C core (creates the lock too), so
    // every owner -- expander, touch, codec, PMU, GPS -- shares one bus and one
    // reopen/lock discipline.
    Esp32C5::I2cBus::begin(I2C_SDA, I2C_SCL, I2C_HZ);

    // Mark the three signals as outputs, then drive all three high TOGETHER
    // before pulsing the resets. This order is the one verified on hardware.
    // EXIO5 is the speaker amplifier enable, not a power rail, so the speaker
    // amplifier is on from here.
    const uint8_t ctrl = (uint8_t)((1U << EXIO_TP_RST) | (1U << EXIO_LCD_RST) | (1U << EXIO_AMP_EN));
    // Suppress retained PWM before changing output modes or levels.
    // Otherwise those changes can expose panel RAM before the later PWM write.
    bool ok = exioWrite(EXIO_PWM_REG, 0x00) &&
              exioWrite(EXIO_MODE_REG, 0xFF) &&
              exioWrite(EXIO_OUT_REG, 0x00) &&
              exioWrite(EXIO_PWM_REG, 0x00) &&
              exioWrite(EXIO_MODE_REG, ctrl);

    s_output = (uint8_t)((1U << EXIO_TP_RST) | (1U << EXIO_LCD_RST));
    ok = ok && exioWrite(EXIO_OUT_REG, (uint8_t)s_output); delay(50);
    s_output = 0x00;
    ok = ok && exioWrite(EXIO_OUT_REG, (uint8_t)s_output); delay(50);
    s_output = ctrl;
    ok = ok && exioWrite(EXIO_OUT_REG, (uint8_t)s_output); delay(50);
    s_mode = ctrl;
    s_wide = false;

    // Pulse each reset (active-low).
    ok = ok && exioSet(EXIO_LCD_RST, false); delay(20);
    ok = ok && exioSet(EXIO_LCD_RST, true);  delay(120);
    ok = ok && exioSet(EXIO_TP_RST, false);  delay(20);
    ok = ok && exioSet(EXIO_TP_RST, true);   delay(300);   // FT6336 boot
    ok = ok && exioWrite(EXIO_PWM_REG, initialBacklight);

    s_ready = ok;
    Serial.printf("[ws-c5] I/O expander 0x24 %s\n", ok ? "ready" : "NOT FOUND");
    return ok;
}

void backlight(uint8_t duty) {
    if (!s_ready) return;
    // Shared-bus policy: this is a runtime bus write, so take the lock first and
    // perform no bus operation if it is not acquired (another owner -- a GPS
    // cycle, a battery refresh -- is mid-transaction). A skipped backlight write
    // just leaves the brightness unchanged until the next call.
    Esp32C5::I2cBus::Guard g(100);
    if (!g) return;
    exioWrite(EXIO_PWM_REG, duty);
}

bool setSpeakerAmp(bool on) {
    if (!s_ready) return false;
    Esp32C5::I2cBus::Guard g(100);
    if (!g) return false;
    const uint16_t before = s_output;
    if (on) s_output |= (uint16_t)(1U << EXIO_AMP_EN);
    else    s_output &= (uint16_t)~(1U << EXIO_AMP_EN);
    if (pushOutput()) return true;
    s_output = before;
    return false;
}

bool powerButton(bool& pressed) {
    if (!s_ready) return false;
    Esp32C5::I2cBus::Guard g(100);
    if (!g) return false;
    uint16_t in = 0;
    if (!readInputs(in)) return false;
    pressed = (in >> EXIO_PWR_KEY) & 1U;
    return true;
}

bool headerMode(uint8_t line, bool output) {
    if (!s_ready || !headerLine(line)) return false;
    Esp32C5::I2cBus::Guard g(100);
    if (!g) return false;
    const uint16_t modeBefore = s_mode, outBefore = s_output;
    const bool wideBefore = s_wide;
    s_wide = true;
    s_output &= (uint16_t)~(1U << line);          // a new output starts low
    if (output) s_mode |= (uint16_t)(1U << line);
    else        s_mode &= (uint16_t)~(1U << line);
    if (exioWrite16(EXIO_OUT_REG, s_output) && exioWrite16(EXIO_MODE_REG, s_mode)) return true;
    s_mode = modeBefore; s_output = outBefore; s_wide = wideBefore;
    return false;
}

bool headerWrite(uint8_t line, bool level) {
    if (!s_ready || !headerLine(line) || !(s_mode & (1U << line))) return false;
    Esp32C5::I2cBus::Guard g(100);
    if (!g) return false;
    const uint16_t before = s_output;
    if (level) s_output |= (uint16_t)(1U << line);
    else       s_output &= (uint16_t)~(1U << line);
    if (pushOutput()) return true;
    s_output = before;
    return false;
}

bool headerRead(uint8_t line, bool& level) {
    if (!s_ready || !headerLine(line)) return false;
    Esp32C5::I2cBus::Guard g(100);
    if (!g) return false;
    uint16_t in = 0;
    if (!readInputs(in)) return false;
    level = (in >> line) & 1U;
    return true;
}

}  // namespace Expander
}  // namespace WsC5_35
