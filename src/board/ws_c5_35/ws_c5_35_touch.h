// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-3.5 — FT6336 capacitive touch.
//
// Reads a single touch point off the FT6336 on the shared I2C bus. The read is
// deliberately defensive: the C5's I2C controller leaves reads unable to
// complete unless the bus is re-opened per read, and even then a held finger
// occasionally returns nothing, so read() re-opens and retries within one poll.
// Why this shape is necessary is documented in docs/HARDWARE-FINDINGS.md
// ("shared I2C bus discipline"). Verified on hardware.
#pragma once

#include <stdint.h>

namespace WsC5_35 {
namespace Touch {

// Probe the FT6336 (reads the chip id) and set a responsive threshold. The
// expander must already be up (begin() calls Expander::begin() to be safe).
// Returns true if the FT6336 answered. Verified on hardware.
bool begin();

// Was the FT6336 found?
bool present();

// Read one touch point. Returns true and fills x,y (panel coordinates, native
// portrait: 0..PANEL_W-1, 0..PANEL_H-1) when a valid 1- or 2-finger touch is
// present; false on finger-up or a failed transaction.
bool read(uint16_t& x, uint16_t& y);

}  // namespace Touch
}  // namespace WsC5_35
