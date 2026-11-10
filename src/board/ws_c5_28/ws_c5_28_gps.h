// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-2.8 — LC76G GNSS over I2C.
// The driver is shared between the Waveshare C5 boards; see ../../part/lc76g_gps.h
// for the API and its test evidence.
//
// On this board the touch controller is at 0x58, an address the LC76G also
// uses when waking. Call Gps::begin(false) so the driver never addresses 0x58.
// GPS on this board is not yet hardware-verified.
#pragma once
#include "../../part/lc76g_gps.h"

namespace WsC5_28 {
namespace Gps = ::C5Parts::Gps;
}  // namespace WsC5_28
