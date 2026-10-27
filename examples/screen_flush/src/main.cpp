// SPDX-License-Identifier: MIT
// screen_flush — clear LCD image retention on the Waveshare ESP32-C5-Touch-LCD-3.5.
//
// A long static, high-contrast screen (e.g. a firmware's menu left on for hours)
// can leave a faint ghost on this ST7796 panel. That's image *retention*, not
// burn-in — the liquid crystals get "stuck", and it's reversible. This sketch
// drives the whole panel through solid full-screen colours (white/black flips
// plus R/G/B) so the pixels are exercised evenly and the ghost relaxes out.
//
// Flash it, let it run ~10-20 min, then reflash your normal firmware. Full-screen
// white<->black inversion does most of the work; the colour passes clear any
// per-subpixel sticking. Console: USB-Serial-JTAG at 115200.
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <board/ws_c5_35/ws_c5_35_board.h>

using namespace WsC5_35;

static TFT_eSPI tft;

// Hold per colour, ms. ~350 ms gives brisk white/black inversions without
// strobing the panel. Lower = more inversions/min; higher = gentler.
static const uint16_t HOLD_MS = 350;

// A pass that alternates white/black (the retention-breaker) between the primaries.
static const uint16_t kFlush[] = {
    TFT_WHITE, TFT_BLACK, TFT_RED,   TFT_BLACK,
    TFT_WHITE, TFT_BLACK, TFT_GREEN, TFT_BLACK,
    TFT_WHITE, TFT_BLACK, TFT_BLUE,  TFT_BLACK,
};
static const size_t kFlushN = sizeof(kFlush) / sizeof(kFlush[0]);

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.printf("[flush] %s — screen retention flush\n", boardName());

    // Expander FIRST: it powers the rail and releases the ST7796 reset, both of
    // which the panel needs before tft.init() (same order as hello_board).
    const bool exio = begin();
    Serial.printf("[flush] expander %s\n", exio ? "up" : "MISSING");

    tft.init();
    tft.setRotation(1);                           // landscape
    tft.invertDisplay(PANEL_NEEDS_INVERSION);     // ST7796 wants inversion on
    tft.fillScreen(TFT_BLACK);
    Serial.printf("[flush] cycling full-screen colours (%u ms each). "
                  "Run ~10-20 min, then reflash your firmware.\n", HOLD_MS);
}

void loop() {
    static size_t i = 0;
    static uint32_t passes = 0;

    tft.fillScreen(kFlush[i]);
    delay(HOLD_MS);

    if (++i >= kFlushN) {
        i = 0;
        passes++;
        Serial.printf("[flush] pass %lu done (~%lu s elapsed)\n",
                      (unsigned long)passes,
                      (unsigned long)(passes * kFlushN * HOLD_MS / 1000));
    }
}
