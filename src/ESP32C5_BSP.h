// SPDX-License-Identifier: MIT
// ESP32-C5-BSP — umbrella header.
//
// The thing that makes the ESP32-C5 stable: a board-support package that
// packages hardware problems already solved on real C5 boards, so other C5
// firmware authors stop re-solving them.
//
// Two layers:
//   chip/   reusable across any ESP32-C5 (single-core pinning, dual-band
//           band-switch ordering, USB-Serial-JTAG console, 5 GHz channel quirks)
//   board/  board-specific drivers; one hardware-proven board today, the
//           Waveshare ESP32-C5-Touch-LCD-3.5 (board/ws_c5_35)
//
// A capability is only exposed here once it has been observed on real hardware.
// See docs/HARDWARE-FINDINGS.md for the three levels of proof and every claim's
// verification tag.
//
// This file intentionally declares no APIs yet — drivers land phase by phase,
// each re-verified on hardware, and this header will include them as they do.
#pragma once

#define ESP32C5_BSP_VERSION_MAJOR 0
#define ESP32C5_BSP_VERSION_MINOR 4
#define ESP32C5_BSP_VERSION_PATCH 0
