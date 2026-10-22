// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-3.5 — board surface.
//
// The one entry point a sketch needs before it touches the display: begin()
// brings the CH32V00x expander up (panel power + resets + backlight), which the
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

// Bring the board's shared hardware up: the expander (so panel power and the
// ST7796 reset are released) and the FT6336 touch probe. Call once in setup()
// BEFORE tft.init(). Returns true if the expander answered; touch presence is
// reported separately via Touch::present(). Verified on hardware.
bool begin();

}  // namespace WsC5_35
