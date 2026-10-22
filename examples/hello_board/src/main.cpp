// SPDX-License-Identifier: MIT
// hello_board — the minimal proof the BSP brings this board up:
//   1. the CH32V00x expander powers the panel and releases the ST7796 reset,
//   2. TFT_eSPI (patched for the C5) paints the screen and the board name,
//   3. the FT6336 touch is read and each touch point is drawn and printed.
//
// Console: USB-Serial-JTAG at 115200.
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <board/ws_c5_35/ws_c5_35_board.h>

using namespace WsC5_35;

static TFT_eSPI tft;

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.printf("[hello] %s\n", boardName());

    // Expander FIRST: it powers the rail and releases the ST7796 reset, both of
    // which the panel needs before tft.init(). Also probes the FT6336.
    const bool exio = begin();
    Serial.printf("[hello] expander %s, touch %s\n",
                  exio ? "up" : "MISSING",
                  Touch::present() ? "present" : "missing");

    tft.init();
    tft.setRotation(1);                           // landscape
    tft.invertDisplay(PANEL_NEEDS_INVERSION);     // ST7796 wants inversion on
    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("ESP32-C5-BSP", 10, 10, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(boardName(), 10, 44, 2);
    tft.drawString(Touch::present() ? "touch: FT6336 OK" : "touch: NOT FOUND", 10, 66, 2);
    tft.drawString("tap the screen", 10, 92, 2);
}

void loop() {
    uint16_t x, y;
    if (Touch::read(x, y)) {
        // Touch is reported in native-portrait coords (0..319 x, 0..479 y);
        // map to the landscape rotation used above so the dot lands under the
        // finger: landscape x = touch y, landscape y = (PANEL_W-1) - touch x.
        const int lx = y;
        const int ly = (PANEL_W - 1) - x;
        tft.fillCircle(lx, ly, 3, TFT_CYAN);
        Serial.printf("[touch] raw x=%u y=%u -> screen %d,%d\n", x, y, lx, ly);
        delay(20);
    }
}
