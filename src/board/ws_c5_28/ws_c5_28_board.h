// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-2.8 — board surface.
//
// The one entry point a sketch needs before it touches the display: begin()
// brings the CH32V006 expander up (panel/touch resets + selected backlight), which the
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

// Reset SPI2 and drive both shared-bus chip selects inactive. Call before any
// display or SD transaction. Board::begin() calls this automatically.
// Startup-only, single-caller API: subsequent calls do not touch the live bus.
void prepareSharedSpi();

// Bring the board's shared hardware up: reset/deselect shared SPI, then release
// the ST7789 reset, set the requested backlight duty, and probe CST3530. Call in
// setup() BEFORE the display driver initialises. Returns true if the expander
// answered; touch presence is reported separately via Touch::present().
// The default path is hardware-verified on the real 2.8" board (2026-10-31).
bool begin(uint8_t initialBacklight = 255);

}  // namespace WsC5_28
