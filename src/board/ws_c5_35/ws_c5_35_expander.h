// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-3.5 — CH32V006 I/O expander (0x24).
//
// The expander owns the ST7796 reset, the FT6336 reset, the backlight PWM and
// the speaker amplifier enable (EXIO5). None of these is a GPIO, so the display
// cannot come up until the expander has released the resets. begin() does that,
// in the order the hardware needs, and is idempotent (safe to call again from
// touch bring-up or before tft.init()).
#pragma once

#include <stdint.h>

namespace WsC5_35 {
namespace Expander {

// Open the shared I2C bus, mark the control lines as outputs, drive both
// resets and the amplifier enable high together, then pulse each reset. Idempotent after success.
// Returns true if the expander answered. `initialBacklight` defaults to the
// legacy full-brightness behaviour; pass 0 to keep the panel dark until the
// first complete frame is ready. Verified on hardware with the default value.
bool begin(uint8_t initialBacklight = 255);

// Was begin() successful?
bool present();

// Backlight duty, 0..255 (expander PWM register). No-op until begin() succeeds.
void backlight(uint8_t duty);

// Every call below needs begin() to have succeeded, takes the shared-bus lock
// for up to 100 ms, and returns false (changing nothing) on a lock timeout, a
// bus failure or a bad argument.

// Speaker amplifier enable (EXIO5). begin() leaves it on. Off silences the
// speaker without touching the codec or I2S.
bool setSpeakerAmp(bool on);

// PWR button (EXIO6). `pressed` is true while the button is held. The power
// chip switches the board off on a long press; this call does not change that.
bool powerButton(bool& pressed);

// Header lines E8..E15 (EXIO8..EXIO15). They start as inputs, and an undriven
// input reads high. Logic levels only: output voltage and drive strength have
// not been measured, so do not power a load from them.
constexpr uint8_t HEADER_FIRST = 8;
constexpr uint8_t HEADER_LAST  = 15;
bool headerMode(uint8_t line, bool output);
bool headerWrite(uint8_t line, bool level);   // line must already be an output
bool headerRead(uint8_t line, bool& level);

}  // namespace Expander
}  // namespace WsC5_35
