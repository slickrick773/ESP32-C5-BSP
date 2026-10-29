// SPDX-License-Identifier: MIT
// Display BSP consumer: the BSP brings the board up (CH32V006 expander powers the
// panel and releases the ST7796 reset) and TFT_eSPI (patched for the C5) paints
// the screen; the FT6336 touch is read and drawn. Mirrors examples/hello_board,
// but installs the BSP as a pinned git dependency instead of a symlink.
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
    Serial.printf("[consumer] %s\n", boardName());

    // Expander FIRST: it powers the rail and releases the ST7796 reset, both of
    // which the panel needs before tft.init(). Also probes the FT6336.
    const bool exio = begin();
    Serial.printf("[consumer] expander %s, touch %s\n",
                  exio ? "up" : "MISSING",
                  Touch::present() ? "present" : "missing");

    tft.init();
    tft.setRotation(1);                           // landscape
    tft.invertDisplay(PANEL_NEEDS_INVERSION);     // ST7796 wants inversion on
    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("ESP32-C5-BSP consumer", 10, 10, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(boardName(), 10, 44, 2);
    tft.drawString(Touch::present() ? "touch: FT6336 OK" : "touch: NOT FOUND", 10, 66, 2);
}

void loop() {
    static uint32_t last = 0;
    const uint32_t now = millis();
    if (now - last < 10) return;
    last = now;

    static int plx = -1, ply = -1;
    uint16_t x, y;
    if (Touch::poll(x, y)) {
        const int lx = y;
        const int ly = (PANEL_W - 1) - x;
        if (plx >= 0) tft.drawLine(plx, ply, lx, ly, TFT_CYAN);
        else          tft.fillCircle(lx, ly, 2, TFT_CYAN);
        plx = lx; ply = ly;
    } else {
        plx = -1;
    }
}
