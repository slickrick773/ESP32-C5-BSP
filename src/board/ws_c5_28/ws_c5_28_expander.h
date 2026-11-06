// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-2.8 — CH32V006 I/O expander (0x24).
//
// The same expander chip as the 3.5", at the same address, but this board
// wires it differently: there is NO EXIO5 panel-power rail here, so the
// expander owns the ST7789 reset (EXIO1), CST3530 touch reset (EXIO0),
// speaker amplifier enable (EXIO3), and backlight PWM. None of these is a GPIO, so the display
// cannot come up until the expander has released the resets. begin() does that
// in the order the hardware needs (mirrored from the owner's working umbravox 2.8"
// port), and is idempotent.
#pragma once

#include <stdint.h>

namespace WsC5_28 {
namespace Expander {

// Open the shared I2C bus, mark the control lines as outputs, then pulse the
// LCD reset and the touch reset (the 2.8"'s specific timing), and set the
// requested initial backlight duty. Idempotent after success. Returns true if the expander
// answered. Ported from the umbravox 2.8" port; hardware-verified from this BSP
// (expander 0x24 brings the panel up on the real 2.8" board, 2026-10-31).
bool begin(uint8_t initialBacklight = 255);

// Set one output latch bit (0..7), preserving all other outputs.
// Requires begin(); returns false on invalid pin, lock timeout, or I2C failure.
bool setOutput(uint8_t pin, bool level);

// Was begin() successful?
bool present();

// Backlight duty, 0..255 (expander PWM register). No-op until begin() succeeds.
void backlight(uint8_t duty);

}  // namespace Expander
}  // namespace WsC5_28
