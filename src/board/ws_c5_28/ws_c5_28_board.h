// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-2.8 — board surface.
//
// The one entry point a sketch needs before it touches the display: begin()
// brings the CH32V006 expander up (panel/touch resets + backlight), which the
// ST7789 needs before the display driver initialises, and probes the CST3530
// touch. Like the 3.5" board layer, it does NOT own the display driver itself
// — the example drives the ST7789 with Arduino_GFX (the proven umbravox path);
// the board layer stays free of any one graphics library so an ESP-IDF or
// TFT_eSPI consumer can reuse it.
#pragma once

#include <stdint.h>
#include "ws_c5_28_pins.h"
#include "ws_c5_28_expander.h"
#include "ws_c5_28_touch.h"

namespace WsC5_28 {

// Human-readable board name.
const char* boardName();

// Bring the board's shared hardware up: the expander (so the ST7789 reset is
// released and the backlight is on) and the CST3530 touch probe. Call once in
// setup() BEFORE the display driver initialises. Returns true if the expander
// answered; touch presence is reported separately via Touch::present().
// Hardware-verified on the real 2.8" board (display + CST3530 touch, 2026-10-31).
bool begin();

}  // namespace WsC5_28
