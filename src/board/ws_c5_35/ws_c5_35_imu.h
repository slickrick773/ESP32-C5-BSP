// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-3.5 — onboard 6-axis IMU at I2C 0x6B on the
// shared bus. It reports WHO_AM_I 0x05, the QMI8658 identity; the register
// map used here is that part's.
//
// Initialize the shared bus (Esp32C5::I2cBus::begin) first. Every call takes
// the bus lock and returns false without touching the bus if it is not free.
#pragma once
#include <stdint.h>

namespace WsC5_35 {
namespace Imu {
constexpr uint8_t ADDR = 0x6B;
constexpr uint8_t WHO_AM_I = 0x05;

struct Sample {
    float ax, ay, az;   // acceleration, g
    float gx, gy, gz;   // angular rate, degrees per second
};

// Check the identity and enable the accelerometer (±4 g) and gyroscope
// (±512 dps) at a nominal 112 Hz. Returns false on a wrong identity or a bus
// error, leaving present() false.
bool begin();
bool present();
// Read the latest accelerometer and gyroscope values. Axis orientation
// relative to the board has not been characterised.
bool read(Sample& out);
// Disable both sensors.
bool end();
}  // namespace Imu
}  // namespace WsC5_35
