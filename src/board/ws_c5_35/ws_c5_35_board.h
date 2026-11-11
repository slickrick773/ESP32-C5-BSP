// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-3.5 — board surface.
//
// The one entry point a sketch needs before it touches the display: begin()
// brings the CH32V006 expander up (resets + amplifier enable + selected backlight), which the
// ST7796 needs before tft.init(). It does NOT own the display driver itself --
// the display is TFT_eSPI in the sketch, configured by the example's user-setup
// header, so the hardware core stays free of any one graphics library and an
// ESP-IDF wrapper can reuse it later.
#pragma once

#include <stdint.h>
#include "ws_c5_35_pins.h"
#include "ws_c5_35_expander.h"
#include "ws_c5_35_touch.h"

namespace WsC5_35 {

// Human-readable board name.
const char* boardName();

// Reset SPI2 and drive both shared-bus chip selects inactive. Call before any
// display or SD transaction. Board::begin() calls this automatically.
// Startup-only, single-caller API: subsequent calls do not touch the live bus.
void prepareSharedSpi();

// Bring the board's shared hardware up: reset/deselect shared SPI, then the expander (so the
// ST7796 reset is released) and the FT6336 touch probe. Call once in setup()
// BEFORE tft.init(). Returns true if the expander answered; touch presence is
// reported separately via Touch::present(). The default full-brightness path is
// hardware-verified; pass 0 to reveal only after the first frame.
bool begin(uint8_t initialBacklight = 255);

}  // namespace WsC5_35
