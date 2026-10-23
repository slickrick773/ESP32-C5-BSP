// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-3.5 — LC76G GNSS over I2C (the crown jewel).
//
// On this board the J3 header UART is not bonded to the chip (see
// docs/HARDWARE-FINDINGS.md), so GPS cannot come in over a UART by wiring alone.
// The header's SDA/SCL DO reach the chip, and the Waveshare LC76G exposes I2C,
// so GPS rides the SAME shared bus as touch/codec/PMU using Waveshare's official
// LC76G_RPI_I2C protocol.
//
// The read is a multi-frame transaction that must not be interleaved with any
// other slave (Quectel), so tick() takes the shared-bus lock for the whole
// cycle; touch, which trylocks non-blocking, simply skips a frame meanwhile.
// tick() self-throttles (~2 Hz), re-opens the bus once per cycle, keeps a short
// inter-step settle, and restores Wire's timeout after each read so the long
// GPS timeout never lingers on the touch reads. All verified on hardware.
#pragma once

#include <stdint.h>

namespace WsC5_35 {
namespace Gps {

// Prepare the GPS (sizes the Wire buffer). The shared bus must already be open
// (Board::begin() / Expander::begin() does that). Call once in setup().
void begin();

// Run one GPS service step. Call every loop; it self-throttles to ~2 Hz, runs
// an atomic I2C read cycle under the bus lock, feeds NMEA to the parser, and
// handles the module's wake. `now` is millis(). Pass a non-zero `busyUntil`
// (e.g. the UI's lastTouch + a grace) to defer the cycle while the screen is
// being touched; 0 never defers.
void tick(uint32_t now, uint32_t busyUntilMs = 0);

// --- Parsed state (updated as NMEA arrives) ---------------------------------
bool     hasFix();            // a valid position fix right now
uint8_t  satellites();        // satellites used in the current solution
uint8_t  satellitesInView();  // satellites seen (from GSV), fix or not
double   latitude();          // decimal degrees (valid only when hasFix())
double   longitude();         // decimal degrees
float    hdop();              // horizontal dilution of precision

// --- Link counters (for debugging the I2C link) ------------------------------
uint32_t bytesRead();         // total NMEA bytes pulled
uint32_t cycles();            // read cycles run
uint32_t fails();             // bus-fail cycles

}  // namespace Gps
}  // namespace WsC5_35
