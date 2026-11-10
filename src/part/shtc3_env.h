// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-3.5 — onboard temperature and humidity sensor
// at I2C 0x70 on the shared bus. It answers the SHTC3 identity command with a
// matching ID (0x0887 observed), and this driver uses that part's commands.
//
// The sensor sits on the board next to the processor and display, so it
// reports the board's temperature, which reads warmer than the room.
//
// Initialize the shared bus (Esp32C5::I2cBus::begin) first. Every call takes
// the bus lock and returns false without touching the bus if it is not free.
#pragma once
#include <stdint.h>

namespace C5Parts {
namespace Env {
constexpr uint8_t ADDR = 0x70;

// Wake the sensor and check its identity. Returns true if it matched.
bool begin();
bool present();
// One measurement (about 15 ms). Both checksums are verified; on any bus or
// checksum error the outputs are left unchanged and false is returned.
bool read(float& temperatureC, float& humidityPercent);
}  // namespace Env
}  // namespace C5Parts
