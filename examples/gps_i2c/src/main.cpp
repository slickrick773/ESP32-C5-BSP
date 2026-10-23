// SPDX-License-Identifier: MIT
// gps_i2c — prove the LC76G GNSS works over I2C on the shared bus.
//
// Opens the bus through the BSP's shared-I2C core and services the GPS. Prints a
// status line once a second: link health (bytes/cycles/fails) so wiring is
// verifiable before any fix, then satellites and, once fixed, the position.
//
// Console: USB-Serial-JTAG at 115200. Wire: LC76G SDA->27, SCL->26, VCC->3V3,
// GND->GND.
#include <Arduino.h>
#include <chip/esp32c5_i2c_bus.h>
#include <board/ws_c5_35/ws_c5_35_pins.h>
#include <board/ws_c5_35/ws_c5_35_gps.h>

using namespace WsC5_35;

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("[gps] LC76G over I2C on the shared bus (SDA 27 / SCL 26)");

    // GPS needs only the bus, not the display/expander, so open it directly.
    Esp32C5::I2cBus::begin(I2C_SDA, I2C_SCL, I2C_HZ);
    Gps::begin();
}

void loop() {
    Gps::tick(millis());   // self-throttles to ~2 Hz; no touch to defer to here

    static uint32_t last = 0;
    const uint32_t now = millis();
    if (now - last >= 1000) {
        last = now;
        if (Gps::bytesRead() == 0 && Gps::cycles() > 2) {
            Serial.printf("[gps] no NMEA yet (cycles %lu, fails %lu) -- check wiring / I2C mode\n",
                          (unsigned long)Gps::cycles(), (unsigned long)Gps::fails());
        } else if (!Gps::hasFix()) {
            Serial.printf("[gps] searching: %u in view, %lu bytes, %lu cycles (need sky view)\n",
                          Gps::satellitesInView(), (unsigned long)Gps::bytesRead(),
                          (unsigned long)Gps::cycles());
        } else {
            Serial.printf("[gps] FIX: %u sats, HDOP %.2f, %.6f, %.6f\n",
                          Gps::satellites(), Gps::hdop(), Gps::latitude(), Gps::longitude());
        }
    }
}
