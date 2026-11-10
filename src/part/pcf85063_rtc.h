// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-3.5 — onboard real-time clock at I2C 0x51 on
// the shared bus. The register behaviour observed on hardware (seconds at
// 0x04 with an oscillator-stop flag in bit 7, BCD fields through 0x0A) is
// consistent with a PCF85063; the part marking has not been read.
//
// Initialize the shared bus (Esp32C5::I2cBus::begin) first. Every call takes
// the bus lock and returns false without touching the bus if it is not free.
#pragma once
#include <stdint.h>

namespace C5Parts {
namespace Rtc {
constexpr uint8_t ADDR = 0x51;

struct DateTime {
    uint16_t year;     // 2000..2099
    uint8_t  month;    // 1..12
    uint8_t  day;      // 1..31
    uint8_t  weekday;  // 0..6, 0 = Sunday
    uint8_t  hour;     // 0..23
    uint8_t  minute;   // 0..59
    uint8_t  second;   // 0..59
};

// Probe the clock. Returns true if it answered.
bool begin();
bool present();
// Read the time. integrityLost reports the oscillator-stop flag: the clock
// lost power or stopped since it was last set, so the time is not trustworthy.
// Fails on a bus error, on invalid BCD, or if the seconds rolled over on both
// of two read attempts.
bool read(DateTime& out, bool& integrityLost);
// Set the time (24-hour mode) and clear the oscillator-stop flag. Rejects
// out-of-range fields without writing.
bool set(const DateTime& value);
}  // namespace Rtc
}  // namespace C5Parts
