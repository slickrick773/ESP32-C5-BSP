// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-2.8 — CH32V006 I/O expander (0x24).
//
// The same expander chip as the 3.5", at the same address, but this board
// wires it differently: there is NO EXIO5 panel-power rail here, so the
// expander owns only the ST7789 reset (EXIO1) and the CST3530 touch reset
// (EXIO0), plus the backlight PWM. None of these is a GPIO, so the display
// cannot come up until the expander has released the resets. begin() does that
// in the order the hardware needs (mirrored from Rick's working umbravox 2.8"
// port), and is idempotent.
#pragma once

#include <stdint.h>

namespace WsC5_28 {
namespace Expander {

// Open the shared I2C bus, mark the control lines as outputs, then pulse the
// LCD reset and the touch reset (the 2.8"'s specific timing), and bring the
// backlight up. Idempotent after success. Returns true if the expander
// answered. Ported from the umbravox 2.8" port; pending BSP hardware-check.
bool begin();

// Was begin() successful?
bool present();

// Backlight duty, 0..255 (expander PWM register). No-op until begin() succeeds.
void backlight(uint8_t duty);

}  // namespace Expander
}  // namespace WsC5_28
