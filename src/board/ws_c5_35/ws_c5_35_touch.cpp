// SPDX-License-Identifier: MIT
// See ws_c5_35_touch.h. Driver shape verified on hardware; see HARDWARE-FINDINGS.
#include "ws_c5_35_touch.h"
#include "ws_c5_35_pins.h"
#include "ws_c5_35_expander.h"
#include "../../chip/esp32c5_i2c_bus.h"
#include <Arduino.h>
#include <Wire.h>

namespace WsC5_35 {
namespace Touch {

namespace Bus = Esp32C5::I2cBus;

static const uint8_t TD_STATUS = 0x02;   // then 6 bytes of point data from 0x03
static const uint8_t CHIP_ID   = 0xA3;   // CHIPER register
static const uint8_t THRESH    = 0x80;   // IDTHRESHOLD: lower = more sensitive

static bool s_present = false;

static bool ftRead(uint8_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(FT6336_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    Wire.requestFrom((int)FT6336_ADDR, (int)len);
    for (uint8_t i = 0; i < len; i++) buf[i] = Wire.available() ? Wire.read() : 0;
    return true;
}

// Parse one TD_STATUS read into a validated on-panel point.
static bool parsePoint(uint16_t& x, uint16_t& y) {
    uint8_t data[7];
    if (!ftRead(TD_STATUS, data, 7)) return false;
    const uint8_t points = data[0] & 0x0F;
    if (points < 1 || points > 2) return false;
    const uint16_t rx = ((uint16_t)(data[1] & 0x0F) << 8) | data[2];
    const uint16_t ry = ((uint16_t)(data[3] & 0x0F) << 8) | data[4];
    if (rx >= PANEL_W || ry >= PANEL_H) return false;
    x = rx; y = ry;
    return true;
}

bool present() { return s_present; }

bool begin() {
    // Panel power and the touch reset live on the expander; its begin() is
    // idempotent and also opens the shared bus (via the I2C core).
    Expander::begin();

    uint8_t chipId = 0;
    Wire.beginTransmission(FT6336_ADDR);
    Wire.write(CHIP_ID);
    if (Wire.endTransmission(false) == 0) {
        Wire.requestFrom((int)FT6336_ADDR, 1);
        if (Wire.available()) chipId = Wire.read();
    }
    s_present = (chipId == FT6336_ID);
    Serial.printf("[ws-c5] FT6336 id 0x%02X %s\n", chipId, s_present ? "(OK)" : "(not found)");

    // 12 gives a responsive bare-glass feel; the validation in parsePoint means
    // a low threshold cannot let noise register. Raise toward 30-40 if a case or
    // film is added and phantom touches appear.
    Wire.beginTransmission(FT6336_ADDR);
    Wire.write(THRESH);
    Wire.write(12);
    Wire.endTransmission();
    return s_present;
}

bool read(uint16_t& x, uint16_t& y) {
    // Skip this frame rather than stall if another owner (e.g. a GPS cycle)
    // holds the bus -- poll()'s grace bridges the gap so it is not felt.
    if (!Bus::lock(0)) return false;

    // The C5's I2C controller leaves reads unable to complete until the bus is
    // re-opened; do it ONCE at the top of the transaction (not per retry --
    // that was the Phase-1 stiffness: three full Wire.end()/begin() per read).
    Bus::reopen();

    bool ok = parsePoint(x, y);
    if (!ok) {
        // The FT6336 intermittently drops a single read even with a finger held.
        // One cheap retry (no second reopen) rides out that transient without
        // paying the reopen cost again; a genuine finger-up fails this too.
        ok = parsePoint(x, y);
    }

    Bus::unlock();
    return ok;
}

bool poll(uint16_t& x, uint16_t& y, uint16_t graceMs, uint16_t minReadIntervalMs) {
    static uint16_t s_lastX = 0, s_lastY = 0;
    static uint32_t s_lastOk = 0, s_lastRead = 0;

    const uint32_t now = millis();
    // Pace the bus: hit the FT6336 at most every minReadIntervalMs. Hammering
    // the re-open starves the chip (~80 Hz is smooth; flat out collapses a drag
    // to one point). Between real reads we bridge with the last contact below.
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
}  // namespace WsC5_35
