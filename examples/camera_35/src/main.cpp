// SPDX-License-Identifier: MIT
// Waveshare 3.5-inch camera example (camera-fitted boards only): identify the
// BF3901, load its start-up settings, receive 240x320 RGB565 pictures and
// draw them on the display.
//
// Serial commands (USB, 115200, one per line): `d` sends the last picture as
// base64 between BEGIN/END lines (tools/save_frame.py turns that into a PNG),
// `s` prints the receive counters, `r 1e` reads a camera register and
// `w 1e 09` writes one (hexadecimal).
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <board/ws_c5_35/ws_c5_35_board.h>
#include <board/ws_c5_35/ws_c5_35_camera.h>

using namespace WsC5_35;

static TFT_eSPI tft;
static uint8_t* frame = nullptr;
static bool     haveFrame = false;
static bool     ready = false;

static void status(const char* text, uint16_t colour) {
    tft.fillRect(0, 0, PANEL_W, 40, TFT_BLACK);
    tft.setTextColor(colour, TFT_BLACK);
    tft.drawString(text, 8, 10, 2);
    Serial.printf("[camera_35] %s\n", text);
}

static void sendFrame() {
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (!haveFrame) { Serial.println("[camera_35] no picture yet"); return; }
    Serial.printf("[camera_35] BEGIN %d %d\n", Camera::WIDTH, Camera::HEIGHT);
    char line[77];
    for (size_t at = 0; at < Camera::FRAME_BYTES; at += 57) {   // FRAME_BYTES is a multiple of 3
        size_t n = Camera::FRAME_BYTES - at; if (n > 57) n = 57;
        int o = 0;
        for (size_t i = 0; i < n; i += 3) {
            const uint32_t v = ((uint32_t)frame[at + i] << 16) | ((uint32_t)frame[at + i + 1] << 8) | frame[at + i + 2];
            line[o++] = b64[v >> 18]; line[o++] = b64[(v >> 12) & 63];
            line[o++] = b64[(v >> 6) & 63]; line[o++] = b64[v & 63];
        }
        line[o] = 0;
        Serial.println(line);
    }
    Serial.println("[camera_35] END");
}

static void printStats() {
    const Camera::CaptureStats s = Camera::captureStats();
    Serial.printf("[camera_35] received %u, wrong length %u, wrong header %u, last %u bytes, starts",
                  (unsigned)s.received, (unsigned)s.badLength, (unsigned)s.badHeader, (unsigned)s.lastBytes);
    for (uint8_t b : s.lastHead) Serial.printf(" %02X", b);
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    const bool exio = begin(0);
    delay(1500);
    Serial.printf("\n[camera_35] %s, expander %s\n", boardName(), exio ? "up" : "MISSING");

    tft.init();
    tft.setRotation(0);                           // portrait, as the camera sends it
    tft.invertDisplay(PANEL_NEEDS_INVERSION);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    Expander::backlight(255);

    frame = (uint8_t*)ps_malloc(Camera::FRAME_BYTES);
    if (!frame) { status("no memory for a picture", TFT_RED); return; }
    if (!Camera::begin()) { status("camera NOT FOUND at 0x6E", TFT_RED); return; }
    if (!Camera::configure()) { status("camera settings FAILED", TFT_RED); return; }
    // As loaded, the picture is upside down when the board is held upright.
    if (!Camera::setMirrorFlip(false, false)) { status("camera orientation FAILED", TFT_RED); return; }
    if (!Camera::captureBegin()) { status("receiver start FAILED", TFT_RED); return; }
    if (!Camera::stream(true)) { status("camera start FAILED", TFT_RED); return; }
    status("camera BF3901 found, waiting for a picture", TFT_YELLOW);
    ready = true;
}

void loop() {
    // One command per line.
    static char cmd[16]; static size_t len = 0;
    while (Serial.available()) {
        const char c = (char)Serial.read();
        if (c != '\n' && c != '\r') { if (len < sizeof(cmd) - 1) cmd[len++] = c; continue; }
        cmd[len] = 0; len = 0;
        unsigned reg = 0, value = 0;
        if (cmd[0] == 'd') sendFrame();
        else if (cmd[0] == 's') printStats();
        else if (sscanf(cmd, "r %x", &reg) == 1) {
            uint8_t v = 0;
            if (Camera::readReg((uint8_t)reg, v)) Serial.printf("[camera_35] reg %02X = %02X\n", reg, v);
            else Serial.printf("[camera_35] reg %02X read FAILED\n", reg);
        } else if (sscanf(cmd, "w %x %x", &reg, &value) == 2) {
            Serial.printf("[camera_35] reg %02X <- %02X %s\n", reg, value,
                          Camera::writeReg((uint8_t)reg, (uint8_t)value) ? "ok" : "FAILED");
        }
    }
    if (!ready) { delay(50); return; }

    static uint32_t count = 0, lastReport = 0;
    const uint32_t t0 = millis();
    if (Camera::captureFrame(frame, 1000)) {
        const uint32_t t1 = millis();
        // The camera sends each pixel low byte first, which is how this chip
        // stores a 16-bit value, so the display driver must swap on the way out.
        tft.setSwapBytes(true);
        tft.pushImage((PANEL_W - Camera::WIDTH) / 2, 60, Camera::WIDTH, Camera::HEIGHT, (uint16_t*)frame);
        if (!haveFrame) status("camera BF3901: picture received", TFT_GREEN);
        haveFrame = true;
        ++count;
        if (millis() - lastReport >= 5000) {
            lastReport = millis();
            Serial.printf("[camera_35] pictures %u, last receive %u ms, draw %u ms\n",
                          (unsigned)count, (unsigned)(t1 - t0), (unsigned)(millis() - t1));
        }
    } else if (millis() - lastReport >= 2000) {
        lastReport = millis();
        Serial.println("[camera_35] no picture within 1 s");
        printStats();
    }
}
