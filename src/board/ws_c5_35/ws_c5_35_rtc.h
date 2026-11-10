// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-3.5 — real-time clock (PCF85063 at 0x51).
// The driver is shared between the Waveshare C5 boards; see ../../part/pcf85063_rtc.h
// for the API and its test evidence.
#pragma once
#include "../../part/pcf85063_rtc.h"

namespace WsC5_35 {
namespace Rtc = ::C5Parts::Rtc;
}  // namespace WsC5_35
