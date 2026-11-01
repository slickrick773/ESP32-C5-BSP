// SPDX-License-Identifier: MIT
// See ws_c5_28_board.h.
#include "ws_c5_28_board.h"
#include <Arduino.h>

namespace WsC5_28 {

const char* boardName() {
    return "Waveshare ESP32-C5-Touch-LCD-2.8";
}

bool begin() {
    const bool exio = Expander::begin();   // panel/touch resets + backlight
    Touch::begin();                        // probe CST3530 (reports its own state)
    return exio;
}

}  // namespace WsC5_28
