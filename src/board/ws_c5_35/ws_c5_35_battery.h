// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-3.5 — AXP2101 PMU / battery, via XPowersLib.
//
// The AXP2101 is at 0x34 on the shared I2C bus. It answers whether or not a
// cell is attached (reporting USB/charge state either way); with a LiPo on the
// MX1.25 header it also gives percentage and cell voltage. refresh() re-opens
// the bus (the C5 read quirk) before sampling, so the readings come back on the
// shared bus without disturbing touch/GPS. Requires the XPowersLib dependency.
#pragma once

#include <stdint.h>

namespace WsC5_35 {
namespace Battery {

// Initialise the AXP2101 on the already-open shared bus and enable its battery/
// VBUS/system voltage measurements. Returns true if the PMU answered.
bool begin();

// Did the PMU answer at begin()?
bool present();

// Re-open the bus and sample a fresh snapshot into the getters below. Call
// before reading when you want current values (e.g. once a second).
void refresh();

// --- snapshot (valid after refresh) -----------------------------------------
bool     batteryConnected();  // a cell is detected on the header
int      percent();           // 0..100, or -1 if the gauge has no reading
uint16_t voltageMv();         // battery voltage, mV (0 if no cell)
bool     charging();          // actively charging
bool     vbusPresent();       // USB power present
uint16_t vbusMv();            // VBUS voltage, mV

// --- clock backup cell --------------------------------------------------------
// The PMU has a trickle charger on its backup pin, off at power-up. As shipped,
// this board has NO cell on that pin (the vendor schematic shows only a 2.2 uF
// capacitor), so the clock stops when USB and LiPo are both removed, and
// enabling this did not change that in testing. These calls exist for boards
// modified with a rechargeable backup cell. Never enable charging into a
// non-rechargeable cell. Only the enable bit is changed; the charge voltage is
// left at the PMU's power-up setting. Returns false on a lock or bus failure.
bool setBackupCharge(bool enabled);
// Reads the enable bit back from the PMU. `enabled` is unchanged on failure.
bool backupCharge(bool& enabled);

}  // namespace Battery
}  // namespace WsC5_35
