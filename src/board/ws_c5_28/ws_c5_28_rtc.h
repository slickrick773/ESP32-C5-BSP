// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-2.8 — real-time clock (PCF85063 at 0x51).
// The driver is shared between the Waveshare C5 boards; see ../../part/pcf85063_rtc.h
// for the API and its test evidence.
//
// The same part is fitted at the same address as on the 3.5-inch board, on
// this board's bus (SDA 0 / SCL 1). Not yet hardware-verified on this board.
#pragma once
#include "../../part/pcf85063_rtc.h"

namespace WsC5_28 {
namespace Rtc = ::C5Parts::Rtc;
}  // namespace WsC5_28
