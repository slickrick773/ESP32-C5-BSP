// SPDX-License-Identifier: MIT
// battery — read the AXP2101 PMU / battery on the shared I2C bus.
//
// Boot: scan the bus (expect 0x34 AXP2101 among the onboard chips; 0x50 too if
// a GPS is wired), then bring up the PMU. Loop: print USB/charge state, and
// percentage + cell voltage when a LiPo is attached.
//
// Console: USB-Serial-JTAG at 115200.
#include <Arduino.h>
#include <Wire.h>
#include <chip/esp32c5_i2c_bus.h>
#include <board/ws_c5_35/ws_c5_35_pins.h>
#include <board/ws_c5_35/ws_c5_35_battery.h>

using namespace WsC5_35;

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("[batt] AXP2101 readout on the shared bus (0x34)");

    Esp32C5::I2cBus::begin(I2C_SDA, I2C_SCL, I2C_HZ);

    Serial.print("[scan] I2C:");
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) Serial.printf(" 0x%02X", a);
    }
    Serial.println("  (expect 0x34 = AXP2101)");

    Battery::begin();
}

void loop() {
    static uint32_t last = 0;
    const uint32_t now = millis();
    if (now - last < 1000) return;
    last = now;

    if (!Battery::present()) { Serial.println("[batt] PMU not found"); return; }

    Battery::refresh();
    if (Battery::batteryConnected()) {
        Serial.printf("[batt] cell %d%%, %u mV, %s | USB %s %u mV\n",
                      Battery::percent(), Battery::voltageMv(),
                      Battery::charging() ? "charging" : "discharging",
                      Battery::vbusPresent() ? "in" : "out", Battery::vbusMv());
    } else {
        Serial.printf("[batt] no cell | USB %s %u mV, %s\n",
                      Battery::vbusPresent() ? "in" : "out", Battery::vbusMv(),
                      Battery::charging() ? "charging" : "idle");
    }
}
