// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-3.5 — CH32V006 I/O expander (0x24).
//
// The expander owns panel power, the ST7796 reset, the FT6336 reset, and the
// backlight PWM. None of these is a GPIO, so the display cannot come up until
// the expander has powered the rail and released the resets. begin() does that,
// in the order the hardware needs, and is idempotent (safe to call again from
// touch bring-up or before tft.init()).
#pragma once

#include <stdint.h>

namespace WsC5_35 {
namespace Expander {

// Open the shared I2C bus, mark the control lines as outputs, bring the rail up
// with both resets released, then pulse each reset. Idempotent after success.
// Returns true if the expander answered. Verified on hardware.
bool begin();

// Was begin() successful?
bool present();

// Backlight duty, 0..255 (expander PWM register). No-op until begin() succeeds.
void backlight(uint8_t duty);

}  // namespace Expander
}  // namespace WsC5_35
