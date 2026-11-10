// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-2.8 — temperature/humidity sensor (SHTC3 at 0x70).
// The driver is shared between the Waveshare C5 boards; see ../../part/shtc3_env.h
// for the API and its test evidence.
//
// The same part is fitted at the same address as on the 3.5-inch board, on
// this board's bus (SDA 0 / SCL 1). Not yet hardware-verified on this board.
#pragma once
#include "../../part/shtc3_env.h"

namespace WsC5_28 {
namespace Env = ::C5Parts::Env;
}  // namespace WsC5_28
