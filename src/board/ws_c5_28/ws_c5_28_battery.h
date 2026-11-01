// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>

namespace WsC5_28 {
namespace Battery {
struct Reading {
    uint16_t raw;        // Rounded mean of eight 10-bit ADC samples (0..1023).
    uint16_t voltageMv;  // Nominal estimate: 3.3 V reference, 3:1 divider.
};
// Call once during setup; opens the shared bus without changing expander outputs.
// Does not prove a battery is connected or that the expander answers.
void begin();
// Read expander 0x24 register 0x06. Returns false on missing begin, lock failure,
// I2C failure, short response or invalid ADC value. Leaves result unchanged on
// failure. Releases the shared bus between samples; call from a control task.
// Voltage alone does not establish presence, charge state or remaining percent.
bool read(Reading& result);
}  // namespace Battery
}  // namespace WsC5_28
