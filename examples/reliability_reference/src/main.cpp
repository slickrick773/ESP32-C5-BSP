// SPDX-License-Identifier: MIT
// reliability_reference — the capstone. Touch, the LC76G GPS and the AXP2101
// all share one I2C bus while Wi-Fi scans in the background, and touch stays
// smooth. If the shared-bus discipline works, this app proves it under load.
//
// Design (every lesson from the earlier phases in one place):
//   - the loop is paced (~10 ms) so the display draw never starves the reads;
//   - Touch::poll() self-paces the bus and bridges dropouts;
//   - Gps::tick() takes the bus lock for its multi-frame cycle and is deferred
//     while the screen is being touched (gate on lastTouch), so a tap is never
//     interrupted; touch trylocks non-blocking and skips a frame otherwise;
//   - Wi-Fi scans ASYNC so the radio never blocks the loop;
//   - the dual-band band mode is set AFTER the Wi-Fi driver starts (the C5
//     ordering trap);
//   - battery is sampled every few seconds.
//
// Console: USB-Serial-JTAG at 115200.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <TFT_eSPI.h>
#include <board/ws_c5_35/ws_c5_35_board.h>
#include <board/ws_c5_35/ws_c5_35_gps.h>
#include <board/ws_c5_35/ws_c5_35_battery.h>

using namespace WsC5_35;

static TFT_eSPI tft;
static uint32_t s_lastTouch = 0;
static int      s_wifiAPs   = -1;

static const int SCRIBBLE_TOP = 108;   // touch-draw area below the status block

static void startScan() {
    WiFi.scanDelete();
    WiFi.scanNetworks(true, true);     // async, include hidden
}

static void drawStatus() {
    tft.fillRect(0, 26, tft.width(), SCRIBBLE_TOP - 26, TFT_BLACK);
    char buf[72];
    int yy = 30;

    tft.setTextColor(Gps::hasFix() ? TFT_GREEN : TFT_YELLOW, TFT_BLACK);
    snprintf(buf, sizeof buf, "GPS: %s  sats %u  in view %u",
             Gps::hasFix() ? "FIX" : "search", Gps::satellites(), Gps::satellitesInView());
    tft.drawString(buf, 8, yy, 2); yy += 24;

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    if (s_wifiAPs >= 0) snprintf(buf, sizeof buf, "WiFi: %d APs (dual-band scan)", s_wifiAPs);
    else                snprintf(buf, sizeof buf, "WiFi: scanning...");
    tft.drawString(buf, 8, yy, 2); yy += 24;

    if (Battery::batteryConnected())
        snprintf(buf, sizeof buf, "Batt: %d%%  %u mV  %s",
                 Battery::percent(), Battery::voltageMv(),
                 Battery::charging() ? "charging" : "discharging");
    else
        snprintf(buf, sizeof buf, "Batt: no cell  USB %s", Battery::vbusPresent() ? "in" : "out");
    tft.drawString(buf, 8, yy, 2);
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.printf("[cap] %s\n", boardName());

    begin();            // expander + touch, on the shared bus
    Gps::begin();
    Battery::begin();

    tft.init();
    tft.setRotation(1);
    tft.invertDisplay(PANEL_NEEDS_INVERSION);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("C5-BSP reliability reference", 8, 6, 2);
    tft.drawFastHLine(0, SCRIBBLE_TOP - 2, tft.width(), TFT_DARKGREY);

    // Wi-Fi: start the driver, THEN set dual-band mode (the C5 ordering trap --
    // set_band_mode before start silently leaves you single-band).
    WiFi.mode(WIFI_STA);
    esp_wifi_set_band_mode(WIFI_BAND_MODE_AUTO);
    startScan();

    drawStatus();
}

void loop() {
    static uint32_t lastTick = 0, lastStatus = 0, lastBatt = 0;
    static int plx = -1, ply = -1;        // previous contact, for a connected stroke
    const uint32_t now = millis();
    if (now - lastTick < 10) return;      // pace the loop (Ph.2 lesson)
    lastTick = now;

    // Touch first, full priority. Connect consecutive contacts into a stroke
    // (isolated dots look like a dotted line at the sample rate).
    uint16_t x, y;
    if (Touch::poll(x, y)) {
        s_lastTouch = now;
        const int lx = y;
        const int ly = (PANEL_W - 1) - x;
        if (ly >= SCRIBBLE_TOP) {
            if (plx >= 0) tft.drawLine(plx, ply, lx, ly, TFT_CYAN);
            else          tft.fillCircle(lx, ly, 2, TFT_CYAN);
            plx = lx; ply = ly;
        }
    } else {
        plx = -1;                         // finger up: break the stroke
    }

    // While a finger is down, do ONLY touch -- the background work (a GPS bus
    // cycle, the status fillRect, a battery read) is what made the stroke janky.
    // It all resumes the moment the finger lifts; a scribble is brief.
    const bool touching = (uint32_t)(now - s_lastTouch) < 150;
    if (touching) return;

    Gps::tick(now, 0);                    // not touching: run the GPS cycle freely

    const int r = WiFi.scanComplete();    // async: adopt a result and rescan
    if (r >= 0)       { s_wifiAPs = r; startScan(); }
    else if (r == -2) { startScan(); }

    if (now - lastBatt >= 3000)   { lastBatt = now; Battery::refresh(); }
    if (now - lastStatus >= 500)  { lastStatus = now; drawStatus(); }
}
