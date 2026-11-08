// SPDX-License-Identifier: MIT
#include <Arduino.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>
#include <board/ws_c5_28/ws_c5_28_board.h>
#include <board/ws_c5_28/ws_c5_28_battery.h>
#include "ws_c5_28_display.h"
using namespace WsC5_28;
static Arduino_DataBus* bus =
    new Arduino_HWSPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO, &SPI);
static Arduino_GFX* gfx =
    new Arduino_ST7789(bus, LCD_RST, ROTATION, true, PANEL_W, PANEL_H,
                       0, 0, 0, 0, ws28Init, sizeof ws28Init);
static bool screenReady = false;

void setup() {
    Serial.begin(115200); // Never wait for USB: battery-only boot must proceed.
    prepareSharedSpi();
    if (!Expander::begin(0)) return;
    delay(300);
    Battery::begin();
    if (!gfx->begin(40000000)) return;
    screenReady = true;
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextSize(2);
    gfx->setTextColor(RGB565_GREEN);
    gfx->setCursor(10, 10);
    gfx->println("Battery power test");
    gfx->setTextSize(1);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setCursor(10, 185);
    gfx->println("Unplug USB. Watch seconds keep counting.");
    gfx->setCursor(10, 203);
    gfx->println("Voltage estimate only; no charge %.");
    gfx->setCursor(10, 221);
    gfx->println("Power source is not detected by software.");
    Expander::backlight(255);
}
void loop() {
    if (!screenReady) { delay(1000); return; }
    static uint32_t last = 0;
    static bool first = true;
    const uint32_t now = millis();
    if (!first && now - last < 1000) { delay(1); return; }
    first = false;
    last = now;
    Battery::Reading sample;
    const bool valid = Battery::read(sample);
    gfx->fillRect(10, 50, 300, 120, RGB565_BLACK);
    gfx->setTextSize(2);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setCursor(10, 55);
    gfx->printf("Running: %lu s", (unsigned long)(now / 1000));
    gfx->setCursor(10, 90);
    if (valid) {
        gfx->printf("Battery: %u.%03u V", sample.voltageMv / 1000, sample.voltageMv % 1000);
        gfx->setCursor(10, 120);
        gfx->printf("ADC: %u / 1023", sample.raw);
    } else {
        gfx->setTextColor(RGB565_RED);
        gfx->println("ADC read failed");
    }
    gfx->fillCircle(290, 155, 7, (now / 1000) % 2 ? RGB565_GREEN : RGB565_BLUE);
    // Do not let a disconnected USB console block the screen test.
    if (Serial && Serial.availableForWrite() >= 96) {
        if (valid) Serial.printf("[battery_28] running=%lu s raw=%u voltage=%u mV\n",
                                 (unsigned long)(now / 1000), sample.raw, sample.voltageMv);
        else Serial.println("[battery_28] ADC read failed");
    }
}
