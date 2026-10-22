// SPDX-License-Identifier: MIT
// See ws_c5_35_expander.h. Bring-up sequence verified on hardware.
#include "ws_c5_35_expander.h"
#include "ws_c5_35_pins.h"
#include <Arduino.h>
#include <Wire.h>

namespace WsC5_35 {
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

bool begin() {
    // Run ONCE. Pulsing EXIO1 (LCD reset) again after the panel is initialised
    // would blank it, so this guard makes a repeat call a safe no-op.
    if (s_ready) return true;

    Wire.begin(I2C_SDA, I2C_SCL, I2C_HZ);

    // Mark the three signals as outputs, then bring the rail up TOGETHER with
    // both resets released -- resetting the touch chip while its rail is off
    // leaves the FT6336 unresponsive.
    const uint8_t ctrl = (uint8_t)((1U << EXIO_TP_RST) | (1U << EXIO_LCD_RST) | (1U << EXIO_POWER));
    bool ok = exioWrite(EXIO_MODE_REG, 0xFF) &&
              exioWrite(EXIO_OUT_REG, 0x00) &&
              exioWrite(EXIO_PWM_REG, 0x00) &&
              exioWrite(EXIO_MODE_REG, ctrl);

    s_output = (uint8_t)((1U << EXIO_TP_RST) | (1U << EXIO_LCD_RST));
    ok = ok && exioWrite(EXIO_OUT_REG, s_output); delay(50);
    s_output = 0x00;
    ok = ok && exioWrite(EXIO_OUT_REG, s_output); delay(50);
    s_output = ctrl;
    ok = ok && exioWrite(EXIO_OUT_REG, s_output); delay(50);

    // Power is on; pulse each reset (active-low) with the rail up.
    ok = ok && exioSet(EXIO_LCD_RST, false); delay(20);
    ok = ok && exioSet(EXIO_LCD_RST, true);  delay(120);
    ok = ok && exioSet(EXIO_TP_RST, false);  delay(20);
    ok = ok && exioSet(EXIO_TP_RST, true);   delay(300);   // FT6336 boot
    ok = ok && exioWrite(EXIO_PWM_REG, 0xFF);              // backlight full

    s_ready = ok;
    Serial.printf("[ws-c5] I/O expander 0x24 %s\n", ok ? "ready" : "NOT FOUND");
    return ok;
}

void backlight(uint8_t duty) {
    if (!s_ready) return;
    exioWrite(EXIO_PWM_REG, duty);
}

}  // namespace Expander
}  // namespace WsC5_35
