// SPDX-License-Identifier: MIT
// See ws_c5_35_board.h.
#include "ws_c5_35_board.h"
#include <Arduino.h>

namespace WsC5_35 {

const char* boardName() {
    return "Waveshare ESP32-C5-Touch-LCD-3.5";
}

bool begin() {
    const bool exio = Expander::begin();   // panel power + resets + backlight
    Touch::begin();                        // probe FT6336 (reports its own state)
    return exio;
}

}  // namespace WsC5_35
