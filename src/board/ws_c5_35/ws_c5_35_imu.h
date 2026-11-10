// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-3.5 — 6-axis IMU (QMI8658 at 0x6B).
// The driver is shared between the Waveshare C5 boards; see ../../part/qmi8658_imu.h
// for the API and its test evidence.
#pragma once
#include "../../part/qmi8658_imu.h"

namespace WsC5_35 {
namespace Imu = ::C5Parts::Imu;
}  // namespace WsC5_35
