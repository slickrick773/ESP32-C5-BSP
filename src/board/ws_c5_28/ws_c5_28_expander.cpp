// SPDX-License-Identifier: MIT
// See ws_c5_28_expander.h. Bring-up sequence mirrored from the umbravox 2.8"
// port (the `#if defined(WS_C5_28)` branch of its ws_c5_expander.cpp), which is
// hardware-accepted on the physical 2.8" board, and hardware-verified from this
// BSP (expander 0x24 ready on the real 2.8" board via hello_board_28, 2026-10-31).
#include "ws_c5_28_expander.h"
#include "ws_c5_28_pins.h"
#include "../../chip/esp32c5_i2c_bus.h"
#include <Arduino.h>
#include <Wire.h>

namespace WsC5_28 {
namespace Expander {

static uint8_t s_output = 0;
static bool    s_ready  = false;

static bool exioWrite(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(EXIO_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

static bool exioSet(uint8_t pin, bool level) {
    if (level) s_output |= (uint8_t)(1U << pin);
    else       s_output &= (uint8_t)~(1U << pin);
    return exioWrite(EXIO_OUT_REG, s_output);
}

bool present() { return s_ready; }

bool begin(uint8_t initialBacklight) {
    // Run ONCE. Pulsing EXIO1 (LCD reset) again after the panel is initialised
    // would blank it, so this guard makes a repeat call a safe no-op.
    if (s_ready) return true;

    // Open the shared bus through the C5 I2C core (creates the lock too), so
    // every owner -- expander, touch, GPS -- shares one bus and one
    // reopen/lock discipline. NOTE: SDA 0 / SCL 1 on this board.
    Esp32C5::I2cBus::begin(I2C_SDA, I2C_SCL, I2C_HZ);

    // 2.8" bring-up: there is no EXIO5 panel rail to raise. Mark the lines as
    // outputs with both resets asserted high, clear the backlight PWM, then
    // pulse each reset (active-low) with the panel's own timing. Mirrors the
    // umbravox WS_C5_28 branch exactly (LCD 50/120 ms, touch 100/500 ms).
    s_output = (uint8_t)((1U << EXIO_TP_RST) | (1U << EXIO_LCD_RST));
    bool ok = exioWrite(EXIO_MODE_REG, 0xFF) &&
              exioWrite(EXIO_OUT_REG, s_output) &&
              exioWrite(EXIO_PWM_REG, 0x00);

    ok = ok && exioSet(EXIO_LCD_RST, false); delay(50);
    ok = ok && exioSet(EXIO_LCD_RST, true);  delay(120);
    ok = ok && exioSet(EXIO_TP_RST, false);  delay(100);
    ok = ok && exioSet(EXIO_TP_RST, true);   delay(500);   // CST3530 boot

    // Default remains full brightness. Consumers that must hide panel memory
    // during graphics startup pass 0 and reveal it after the first frame.
    ok = ok && exioWrite(EXIO_PWM_REG, initialBacklight);

    s_ready = ok;
    Serial.printf("[ws-c5-28] I/O expander 0x24 %s\n", ok ? "ready" : "NOT FOUND");
    return ok;
}

bool setOutput(uint8_t pin, bool level) {
    if (!s_ready || pin > 7) return false;
    Esp32C5::I2cBus::Guard g(100);
    if (!g) return false;
    const uint8_t mask = (uint8_t)(1U << pin);
    const uint8_t next = level ? (s_output | mask) : (s_output & ~mask);
    if (!exioWrite(EXIO_OUT_REG, next)) return false;
    s_output = next;
    return true;
}

void backlight(uint8_t duty) {
    if (!s_ready) return;
    // Shared-bus policy: a runtime bus write takes the lock first and performs
    // no bus op if it is not acquired (another owner -- a GPS cycle -- is
    // mid-transaction). A skipped write just leaves the brightness unchanged.
    Esp32C5::I2cBus::Guard g(100);
    if (!g) return;
    exioWrite(EXIO_PWM_REG, duty);
}

}  // namespace Expander
}  // namespace WsC5_28
