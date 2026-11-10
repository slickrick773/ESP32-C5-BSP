// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-2.8 — 6-axis IMU (QMI8658 at 0x6B).
// The driver is shared between the Waveshare C5 boards; see ../../part/qmi8658_imu.h
// for the API and its test evidence.
//
// The same part is fitted at the same address as on the 3.5-inch board, on
// this board's bus (SDA 0 / SCL 1). Not yet hardware-verified on this board.
#pragma once
#include "../../part/qmi8658_imu.h"

namespace WsC5_28 {
namespace Imu = ::C5Parts::Imu;
}  // namespace WsC5_28
