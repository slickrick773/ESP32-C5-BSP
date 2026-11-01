// SPDX-License-Identifier: MIT
// hello_board_28 — the minimal proof the BSP brings the 2.8" board up:
//   1. the CH32V006 expander releases the ST7789 reset and the CST3530 reset
//      and turns the backlight on,
//   2. Arduino_GFX (ST7789, the proven umbravox path) paints the screen and the
//      board name — with the MSB-first RAMCTRL byte-order fix in ws28Init[],
//   3. the CST3530 touch is read and each touch point is drawn and printed.
//
// Display driver: Arduino_GFX (moononournation/GFX Library for Arduino),
// reproducing Rick's hardware-accepted umbravox 2.8" bring-up exactly. The BSP
// board layer (src/board/ws_c5_28) is driver-agnostic; the Arduino_GFX
// dependency is confined to THIS example. A TFT_eSPI variant is a separate,
// later-validated example.
//
// Console: USB-Serial-JTAG at 115200.
#include <Arduino.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>
#include <board/ws_c5_28/ws_c5_28_board.h>
#include "ws_c5_28_display.h"   // the proven ST7789 init array (Arduino_GFX form)

using namespace WsC5_28;

// 40 MHz: the clock the umbravox 2.8" port runs this panel at.
static constexpr int32_t SPI_HZ = 40000000;

// Arduino's SPI class (Arduino_HWSPI), NOT Arduino_GFX's register-level ESP32
// bus (Arduino_ESP32SPI has no C5 branch) — so the panel and the microSD card
// share one owner of the bus. Pins from the BSP: SCLK 6 / MOSI 7 / MISO 8,
// CS 10, DC 9. RST is -1 (the reset is on the expander, released by begin()).
static Arduino_DataBus* bus =
    new Arduino_HWSPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO, &SPI);

// ST7789, native 240x320 portrait, ROTATION 1 -> 320x240 landscape. IPS=true
// (INVON) matches the 0x21 the panel wants; ws28Init[] is the panel's exact
// power/gamma/RAMCTRL sequence (byte-order fix included).
static Arduino_GFX* gfx =
    new Arduino_ST7789(bus, LCD_RST, ROTATION, true /*IPS*/, PANEL_W, PANEL_H,
                       0, 0, 0, 0, ws28Init, sizeof ws28Init);

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.printf("[hello28] %s\n", boardName());

    // Expander FIRST: it releases the ST7789 reset and turns the backlight on,
    // both of which the panel needs before the display driver initialises. Also
    // probes the CST3530.
    const bool exio = begin();
    Serial.printf("[hello28] expander %s, touch %s\n",
                  exio ? "up" : "MISSING",
                  Touch::present() ? "present" : "missing");

    // Deselect the microSD (CS 23) before the first display transaction: it
    // shares the SPI bus, and a floating CS can corrupt the panel's first write.
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    // The panel's reset is already released by the expander; Arduino_GFX just
    // runs ws28Init[] over SPI. (If a soft reset ever leaves the panel blank,
    // the fix is an SPI2 peripheral-block reset before begin() — see the
    // umbravox 2.8" port; not needed from a clean power-up.)
    if (!gfx->begin(SPI_HZ)) {
        Serial.println("[hello28] gfx->begin() FAILED");
        for (;;) delay(1000);
    }
    gfx->fillScreen(RGB565_BLACK);

    gfx->setTextColor(RGB565_GREEN);
    gfx->setCursor(10, 10);
    gfx->setTextSize(2);
    gfx->println("ESP32-C5-BSP");
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(10, 36);
    gfx->println(boardName());
    gfx->setCursor(10, 50);
    gfx->println(Touch::present() ? "touch: CST3530 OK" : "touch: NOT FOUND");
    gfx->setCursor(10, 64);
    gfx->println("tap the screen");
}

void loop() {
    // Sample at ~125 Hz (every 8 ms). On the 2.8" the CST3530 takes a fresh
    // read every 8 ms with effectively zero dropouts (measured on hardware), and
    // new coordinates arrive ~every 15 ms; sampling this tight keeps a fast drag
    // to a few px between points, so the stroke is smooth. A slacker ~10-12 ms
    // cadence stretches the gaps and the drag reads as angular connect-the-dots.
    static uint32_t last = 0;
    const uint32_t now = millis();
    if (now - last < 8) return;
    last = now;

    // Connect consecutive contacts with a line segment so a drag draws a
    // continuous stroke. Coords are native-portrait; Touch::toLandscape maps
    // them to the rotation-1 screen (sx = y, sy = (PANEL_W-1) - x).
    static int plx = -1, ply = -1;

    uint16_t x, y;
    if (Touch::poll(x, y)) {
        int lx, ly;
        Touch::toLandscape(x, y, lx, ly);
        if (plx >= 0) gfx->drawLine(plx, ply, lx, ly, RGB565_CYAN);
        else          gfx->fillCircle(lx, ly, 2, RGB565_CYAN);
        plx = lx; ply = ly;
        // Throttled serial report so a tap/drag can be checked over USB without
        // the panel (~5 Hz). Native CST3530 coords (x<240, y<320) then the
        // landscape screen coords (x<320, y<240).
        static uint32_t lastPrint = 0;
        if (now - lastPrint >= 200) {
            lastPrint = now;
            Serial.printf("[hello28] touch native=(%u,%u) screen=(%d,%d)\n", x, y, lx, ly);
        }
    } else {
        plx = -1;   // finger up: break the stroke
    }
}
