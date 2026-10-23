// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-3.5 — FT6336 capacitive touch.
//
// Two layers:
//   read()  one raw logical read of the FT6336. Takes the shared-bus lock
//           non-blocking (so it skips a frame rather than stalling behind a GPS
//           transaction), re-opens the bus (the C5 read quirk), and retries the
//           transient glitch within the one call. Returns a point or nothing.
//   poll()  what a UI should call. Same read, plus a short grace window that
//           holds the last contact across a dropped read, so a drag does not
//           "let go" and a tap does not feel stiff. This dropout bridging is the
//           touch-break story -- see docs/HARDWARE-FINDINGS.md. Verified on hw.
#pragma once

#include <stdint.h>

namespace WsC5_35 {
namespace Touch {

// Probe the FT6336 and set a responsive threshold. The expander must be up
// (begin() calls Expander::begin() to be safe). Returns true if it answered.
bool begin();

// Was the FT6336 found?
bool present();

// One raw read. Fills x,y (native-portrait panel coords) and returns true on a
// valid 1-/2-finger touch; false on finger-up, a dropped read, or a busy bus.
bool read(uint16_t& x, uint16_t& y);

// UI-facing read. Safe to call as fast as your loop runs: it self-paces the
// actual bus read to minReadIntervalMs (the FT6336 + the per-read bus re-open
// cannot be hammered -- polling the raw read() flat out starves the chip and a
// drag collapses to a single point), and bridges a dropped or skipped read by
// holding the last contact for up to graceMs, so a drag draws a smooth line and
// a tap does not feel stiff. A real release (no good read for graceMs) returns
// false. Returns native-portrait panel coords. Verified on hardware.
bool poll(uint16_t& x, uint16_t& y, uint16_t graceMs = 90, uint16_t minReadIntervalMs = 12);

}  // namespace Touch
}  // namespace WsC5_35
