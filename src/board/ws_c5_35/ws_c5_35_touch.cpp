// SPDX-License-Identifier: MIT
// See ws_c5_35_touch.h. Driver shape verified on hardware; see HARDWARE-FINDINGS.
#include "ws_c5_35_touch.h"
#include "ws_c5_35_pins.h"
#include "ws_c5_35_expander.h"
#include <Arduino.h>
#include <Wire.h>

namespace WsC5_35 {
namespace Touch {

static const uint8_t TD_STATUS = 0x02;   // then 6 bytes of point data from 0x03
static const uint8_t CHIP_ID   = 0xA3;   // CHIPER register
static const uint8_t THRESH     = 0x80;  // IDTHRESHOLD: lower = more sensitive

static bool s_present = false;

static bool ftRead(uint8_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(FT6336_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    Wire.requestFrom((int)FT6336_ADDR, (int)len);
    for (uint8_t i = 0; i < len; i++) buf[i] = Wire.available() ? Wire.read() : 0;
    return true;
}

bool present() { return s_present; }

bool begin() {
    // Panel power and the touch reset live on the expander; its begin() is
    // idempotent, so calling it here is safe even if setup() already did.
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

    // 12 gives a responsive bare-glass feel; read() validates each sample so a
    // low threshold cannot let noise register. Raise toward 30-40 if a case or
    // film is added and phantom touches appear.
    Wire.beginTransmission(FT6336_ADDR);
    Wire.write(THRESH);
    Wire.write(12);
    Wire.endTransmission();
    return s_present;
}

bool read(uint16_t& x, uint16_t& y) {
    // The C5's early Wire.begin() leaves the I2C controller unable to complete
    // reads (writes ACK, reads return 0), so re-open the bus on each read and
    // validate the result (1-2 points, on-panel coords). This was the single
    // hardest bug in the C5 bring-up -- do not remove.
    //
    // The read is ALSO intermittently flaky even with the re-open: a finger
    // held still occasionally comes back as no points / a failed transaction.
    // The glitch is transient, so retry a couple of times within this one poll
    // before reporting "no touch". A genuine finger-up fails all tries.
    for (int attempt = 0; attempt < 3; attempt++) {
        Wire.end();
        Wire.begin(I2C_SDA, I2C_SCL, I2C_HZ);

        uint8_t data[7];
        if (ftRead(TD_STATUS, data, 7)) {
            const uint8_t points = data[0] & 0x0F;
            if (points >= 1 && points <= 2) {
                const uint16_t rx = ((uint16_t)(data[1] & 0x0F) << 8) | data[2];
                const uint16_t ry = ((uint16_t)(data[3] & 0x0F) << 8) | data[4];
                if (rx < PANEL_W && ry < PANEL_H) { x = rx; y = ry; return true; }
            }
        }
        // failed transaction / no or off-panel points: ride out the glitch
    }
    return false;
}

}  // namespace Touch
}  // namespace WsC5_35
