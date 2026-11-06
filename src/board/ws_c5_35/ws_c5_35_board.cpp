// SPDX-License-Identifier: MIT
// See ws_c5_35_board.h.
#include "ws_c5_35_board.h"
#include "../../chip/esp32c5_spi.h"
#include <Arduino.h>

namespace WsC5_35 {

const char* boardName() {
    return "Waveshare ESP32-C5-Touch-LCD-3.5";
}

void prepareSharedSpi() {
    // Startup-only: never reset a bus already owned by display/SD drivers.
    static bool prepared = false;
    if (prepared) return;
    prepared = true;
    Esp32C5::Spi2::resetPeripheral();
    pinMode(LCD_CS, OUTPUT);
    digitalWrite(LCD_CS, HIGH);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
}

bool begin(uint8_t initialBacklight) {
    prepareSharedSpi();
    const bool exio = Expander::begin(initialBacklight);
    Touch::begin();                        // probe FT6336 (reports its own state)
    return exio;
}

}  // namespace WsC5_35
