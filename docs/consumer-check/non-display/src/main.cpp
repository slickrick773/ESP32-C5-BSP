// SPDX-License-Identifier: MIT
// Non-display BSP consumer: exercises the whole BSP surface except the display --
// shared I2C bus, board bring-up (CH32V006 expander), FT6336 touch, LC76G GPS,
// AXP2101 battery (which pulls XPowersLib transitively). The BSP is installed as a
// pinned git dependency; headers resolve from the library src/ root, hence the
// <chip/...> and <board/ws_c5_35/...> include paths.
#include <Arduino.h>
#include <Wire.h>
#include <ESP32C5_BSP.h>
#include <chip/esp32c5_i2c_bus.h>
#include <board/ws_c5_35/ws_c5_35_pins.h>
#include <board/ws_c5_35/ws_c5_35_board.h>
#include <board/ws_c5_35/ws_c5_35_touch.h>
#include <board/ws_c5_35/ws_c5_35_gps.h>
#include <board/ws_c5_35/ws_c5_35_battery.h>

using namespace WsC5_35;

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.printf("[consumer] %s via ESP32-C5-BSP %d.%d.%d\n",
                  WsC5_35::boardName(),
                  ESP32C5_BSP_VERSION_MAJOR, ESP32C5_BSP_VERSION_MINOR, ESP32C5_BSP_VERSION_PATCH);

    // The board surface lives directly in WsC5_35 (not a Board:: sub-namespace,
    // unlike Gps/Battery/Touch). begin() opens the shared bus through the chip
    // core and brings up the expander + touch; GPS/battery ride the same bus.
    WsC5_35::begin();
    Gps::begin();
    Battery::begin();
}

void loop() {
    Gps::tick(millis());
    Battery::refresh();

    uint16_t x = 0, y = 0;
    const bool touched = Touch::read(x, y);

    Serial.printf("[consumer] sats=%u inView=%u | batt=%d%% %umV vbus=%d | touch=%d (%u,%u)\n",
                  Gps::satellites(), Gps::satellitesInView(),
                  Battery::percent(), Battery::voltageMv(), (int)Battery::vbusPresent(),
                  (int)touched, x, y);
    delay(1000);
}
