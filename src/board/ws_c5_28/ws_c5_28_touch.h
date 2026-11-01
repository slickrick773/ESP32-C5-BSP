// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-2.8 — CST3530 capacitive touch.
//
// Same two-layer shape as the 3.5"'s FT6336 driver (read() for one raw read,
// poll() for a UI with dropout bridging), so a consumer swaps boards without
// re-learning the touch API. The controller underneath is different: a CST3530
// at 0x58, read with a four-byte register and a repeated START (the FT6336 is a
// single-byte, STOP-based read at 0x38), and — unlike the 3.5" — the proven
// 2.8" read path does NOT re-open the bus per read. Mirrored from Rick's
// working umbravox 2.8" port (device/touch.cpp CST3530 branch + the
// cst3530_sample.h decode); hardware-verified from this BSP (2026-10-31).
#pragma once

#include <stdint.h>

namespace WsC5_28 {
namespace Touch {

// Probe the CST3530 and set GPIO5 (its INT line) as an input. The expander must
// be up (begin() calls Expander::begin() to be safe). Returns true if the chip
// ACKed at 0x58. (The CST3530 has no documented sensitivity register, so unlike
// the FT6336 driver there is no threshold to set.)
bool begin();

// Was the CST3530 found?
bool present();

// One raw read. Fills x,y (native-portrait panel coords, x<240 y<320) and
// returns true on a valid single-contact touch; false on finger-up, a dropped
// read, or a busy bus. Takes the shared-bus lock non-blocking (skips the frame
// rather than stalling behind a GPS transaction).
bool read(uint16_t& x, uint16_t& y);

// UI-facing read. Safe to call as fast as your loop runs: it self-paces the bus
// read to minReadIntervalMs and bridges a dropped or skipped read by holding
// the last contact for up to graceMs, so a drag draws a smooth line and a tap
// does not feel stiff. A real release (no good read for graceMs) returns false.
// Returns native-portrait panel coords. Hardware-verified on the 2.8" board:
// taps AND continuous dragging track cleanly (2026-10-31).
//
// The defaults are tighter than the 3.5"'s FT6336 driver (90/12), from hardware
// measurement on this board: the CST3530 takes a fresh read every ~8 ms with
// effectively zero dropouts and reports new coordinates ~every 15 ms, so an 8 ms
// cadence keeps a fast drag smooth, and it needs far less dropout-bridging than
// the FT6336 (which the 90 ms grace is tuned for). A looser cadence makes a fast
// drag read as angular "connect-the-dots".
bool poll(uint16_t& x, uint16_t& y, uint16_t graceMs = 60, uint16_t minReadIntervalMs = 8);

// Map a native-portrait contact (from read()/poll()) to landscape screen
// coordinates at ROTATION 1: sx = y, sy = (PANEL_W - 1) - x. Provided so the
// mapping lives in one place and matches the CST3530 decode used on hardware.
void toLandscape(uint16_t x, uint16_t y, int& sx, int& sy);

}  // namespace Touch
}  // namespace WsC5_28
