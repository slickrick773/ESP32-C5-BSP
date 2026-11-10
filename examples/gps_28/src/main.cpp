// SPDX-License-Identifier: MIT
// Waveshare 2.8-inch GPS test: LC76G over I2C on the shared bus (SDA 0 /
// SCL 1), with the touch controller read alongside so that any interference
// between the two shows up.
//
// NOT yet hardware-verified. The LC76G uses addresses 0x50, 0x54 and, when
// waking or recovering, 0x58. On this board 0x58 is also the CST3530 touch
// controller. The driver is therefore started with begin(false) so it never
// addresses 0x58; whether the module then stays awake, and whether touch
// reads stay correct with the module on the bus, is what this example is for.
// Build with -DGPS28_USE_WAKE_ADDRESS=1 to try the stock behaviour instead.
//
// Wire the LC76G to SDA 0, SCL 1, 3V3 and GND. No display is rendered.
#include <Arduino.h>
#include <Wire.h>
#include <board/ws_c5_28/ws_c5_28_board.h>
#include <board/ws_c5_28/ws_c5_28_gps.h>

#ifndef GPS28_USE_WAKE_ADDRESS
#define GPS28_USE_WAKE_ADDRESS 0
#endif

using namespace WsC5_28;

void setup() {
    Serial.begin(115200);
    delay(1500);
    // Board::begin() opens the shared bus, resets and probes the touch
    // controller, and keeps the backlight off.
    if (!WsC5_28::begin(0)) Serial.println("[gps_28] board startup FAILED");
    Serial.printf("[gps_28] touch controller %s; GPS wake address 0x58 %s\n",
                  Touch::present() ? "found" : "not found (it may only answer while touched)",
                  GPS28_USE_WAKE_ADDRESS ? "USED" : "avoided");
    Serial.print("[gps_28] bus scan:");
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) Serial.printf(" 0x%02X", a);
    }
    Serial.println("  (expect 0x50 = LC76G)");
    Gps::begin(GPS28_USE_WAKE_ADDRESS != 0);
}

void loop() {
    static uint32_t lastTouchAt = 0, lastPrint = 0, touches = 0;
    static uint16_t tx = 0, ty = 0;
    const uint32_t now = millis();

    uint16_t x = 0, y = 0;
    if (Touch::poll(x, y)) {
        if (!lastTouchAt || now - lastTouchAt > 150) ++touches;
        lastTouchAt = now; tx = x; ty = y;
    }
    // Defer GPS cycles while the screen is being touched, as the 3.5-inch
    // capstone does.
    Gps::tick(now, lastTouchAt ? lastTouchAt + 200 : 0);

    if (now - lastPrint >= 1000) {
        lastPrint = now;
        if (Gps::bytesRead() == 0 && Gps::cycles() > 2)
            Serial.printf("[gps_28] no NMEA yet (cycles %lu, fails %lu) -- check wiring / I2C mode\n",
                          (unsigned long)Gps::cycles(), (unsigned long)Gps::fails());
        else if (!Gps::hasFix())
            Serial.printf("[gps_28] searching: %u in view, %lu bytes, %lu cycles, %lu fails\n",
                          Gps::satellitesInView(), (unsigned long)Gps::bytesRead(),
                          (unsigned long)Gps::cycles(), (unsigned long)Gps::fails());
        else
            Serial.printf("[gps_28] FIX: %u sats, HDOP %.2f, %.6f, %.6f (%lu bytes, %lu fails)\n",
                          Gps::satellites(), Gps::hdop(), Gps::latitude(), Gps::longitude(),
                          (unsigned long)Gps::bytesRead(), (unsigned long)Gps::fails());
        Serial.printf("[gps_28] touch: %lu touches, last at %u,%u%s\n", (unsigned long)touches, tx, ty,
                      lastTouchAt && now - lastTouchAt < 1000 ? " (touching)" : "");
    }
    delay(2);
}
