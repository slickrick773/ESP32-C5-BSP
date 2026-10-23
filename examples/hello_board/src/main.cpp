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
    // Connect consecutive contacts with a line segment so a drag draws a
    // continuous stroke (isolated dots at the sample rate look like a broken
    // trail). Coords are native-portrait; map to the landscape rotation:
    // screen x = touch y, screen y = (W-1) - touch x.
    static int plx = -1, ply = -1;
    static uint16_t px = 0xFFFF, py = 0xFFFF;

    uint16_t x, y;
    if (Touch::poll(x, y)) {
        const int lx = y;
        const int ly = (PANEL_W - 1) - x;
        if (plx >= 0) tft.drawLine(plx, ply, lx, ly, TFT_CYAN);
        else          tft.fillCircle(lx, ly, 2, TFT_CYAN);
        plx = lx; ply = ly;
        if (x != px || y != py) { Serial.printf("P %u,%u\n", x, y); px = x; py = y; }
    } else {
        plx = -1;   // finger up: break the stroke
    }
}
