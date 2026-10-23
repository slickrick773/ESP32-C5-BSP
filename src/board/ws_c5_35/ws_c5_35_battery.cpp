// SPDX-License-Identifier: MIT
// See ws_c5_35_battery.h.
#include "ws_c5_35_battery.h"
#include "ws_c5_35_pins.h"
#include "../../chip/esp32c5_i2c_bus.h"
#include <Arduino.h>
#include <Wire.h>
#define XPOWERS_CHIP_AXP2101   // selects the AXP2101 class (XPowersPMU) in XPowersLib
#include <XPowersLib.h>

namespace WsC5_35 {
namespace Battery {

namespace Bus = Esp32C5::I2cBus;

static XPowersPMU s_pmu;
static bool s_present = false;

// cached snapshot
static bool     s_batt = false;
static int      s_pct  = -1;
static uint16_t s_vmv  = 0;
static bool     s_chg  = false;
static bool     s_vbus = false;
static uint16_t s_vbmv = 0;

bool present() { return s_present; }

bool begin() {
    // Try a few times with a bus re-open between: the C5's I2C read quirk can
    // make the PMU's chip-id read fail right after a fresh Wire.begin.
    for (int attempt = 0; attempt < 3 && !s_present; attempt++) {
        Bus::reopen();
        s_present = s_pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);
    }
    Serial.printf("[ws-c5] AXP2101 0x34 %s\n", s_present ? "ready" : "NOT FOUND");
    if (!s_present) return false;

    // Turn on the measurements we read below.
    s_pmu.enableBattDetection();
    s_pmu.enableBattVoltageMeasure();
    s_pmu.enableVbusVoltageMeasure();
    s_pmu.enableSystemVoltageMeasure();
    return true;
}

void refresh() {
    if (!s_present) return;
    Bus::reopen();   // C5 read quirk: re-open before the read burst
    s_batt = s_pmu.isBatteryConnect();
    s_pct  = s_pmu.getBatteryPercent();   // -1 when the gauge has no reading
    s_vmv  = s_pmu.getBattVoltage();
    s_chg  = s_pmu.isCharging();
    s_vbus = s_pmu.isVbusIn();
    s_vbmv = s_pmu.getVbusVoltage();
}

bool     batteryConnected() { return s_batt; }
int      percent()          { return s_pct;  }
uint16_t voltageMv()        { return s_vmv;  }
bool     charging()         { return s_chg;  }
bool     vbusPresent()      { return s_vbus; }
uint16_t vbusMv()           { return s_vbmv; }

}  // namespace Battery
}  // namespace WsC5_35
