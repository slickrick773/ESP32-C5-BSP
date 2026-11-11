// SPDX-License-Identifier: MIT
// Waveshare 3.5-inch self-checking test for four board features:
//   1. charge current  (automatic)
//   2. header lines    (needs one jumper wire between E8 and E9)
//   3. speaker amplifier switch (listen: tone, silence, tone)
//   4. PWR button      (one short press within 15 s)
// Each step prints a result line and the screen ends on a summary.
#include <Arduino.h>
#include <ESP_I2S.h>
#include <math.h>
#include <TFT_eSPI.h>
#include <chip/esp32c5_tasks.h>
#include <board/ws_c5_35/ws_c5_35_board.h>
#include <board/ws_c5_35/ws_c5_35_audio.h>
#include <board/ws_c5_35/ws_c5_35_battery.h>

using namespace WsC5_35;

static TFT_eSPI tft;
static I2SClass i2s;
static volatile bool toneOn = false;
static int row = 0;

static void heading(const char* text) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(text, 10, 8, 4);
    row = 0;
}
static void result(const char* name, const char* verdict, uint16_t colour, const char* detail) {
    Serial.printf("[extras_35] %s: %s (%s)\n", name, verdict, detail);
    const int y = 50 + row++ * 62;
    tft.setTextColor(colour, TFT_BLACK);
    char line[48];
    snprintf(line, sizeof line, "%s: %s", name, verdict);
    tft.drawString(line, 10, y, 4);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(detail, 10, y + 28, 2);
}
static void prompt(const char* text) {
    tft.fillRect(0, 296, 480, 24, TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(text, 10, 298, 2);
}

// 1. Read the charge current, set another step, read it back, put it back.
static void checkCharge() {
    char d[64];
    uint16_t start = 0, seen = 0;
    if (!Battery::begin() || !Battery::chargeCurrentMa(start)) {
        result("Charge current", "FAIL", TFT_RED, "power chip did not answer");
        return;
    }
    const uint16_t trial = start == 300 ? 200 : 300;
    const bool set = Battery::setChargeCurrentMa(trial) && Battery::chargeCurrentMa(seen) && seen == trial;
    const bool rejected = !Battery::setChargeCurrentMa(250);
    uint16_t end = 0;
    const bool restored = Battery::setChargeCurrentMa(start) && Battery::chargeCurrentMa(end) && end == start;
    snprintf(d, sizeof d, "was %u mA, set %u read %u, back to %u", start, trial, seen, end);
    const bool ok = set && rejected && restored;
    result("Charge current", ok ? "PASS" : "FAIL", ok ? TFT_GREEN : TFT_RED, d);
}

// 2. One wire between E8 and E9: drive one, read the other, then swap.
static void checkHeader() {
    int good = 0, total = 0, lowSeen = 0;
    for (int dir = 0; dir < 2; dir++) {
        const uint8_t drive = dir ? 9 : 8, sense = dir ? 8 : 9;
        if (!Expander::headerMode(sense, false) || !Expander::headerMode(drive, true)) continue;
        for (int i = 0; i < 20; i++) {
            const bool level = i & 1;
            bool got = !level;
            total++;
            if (!Expander::headerWrite(drive, level)) continue;
            delay(30);
            if (!Expander::headerRead(sense, got)) continue;
            if (got == level) good++;
            if (!got) lowSeen++;
        }
        Expander::headerMode(drive, false);
    }
    char d[64];
    snprintf(d, sizeof d, "%d of %d readings matched", good, total);
    if (total == 40 && good == 40)      result("Header E8-E9", "PASS", TFT_GREEN, d);
    else if (total == 40 && lowSeen == 0) result("Header E8-E9", "NO WIRE", TFT_YELLOW, "fit a jumper from E8 to E9, press RST");
    else                                 result("Header E8-E9", "FAIL", TFT_RED, d);
}

static void toneTask(void*) {
    int16_t period[16];   // one cycle of 1 kHz at 16 kHz, built once
    for (int i = 0; i < 16; i++) period[i] = (int16_t)(4500.0f * sinf(2.0f * 3.14159265f * i / 16.0f));
    int16_t pcm[256 * 2];
    for (;;) {
        for (int i = 0; i < 256; i++) pcm[2 * i] = pcm[2 * i + 1] = toneOn ? period[i & 15] : 0;
        i2s.write((const uint8_t*)pcm, sizeof(pcm));
    }
}

// 3. Tone for 2 s, amplifier off for 2 s (tone still playing), on for 2 s.
static void checkAmp() {
    i2s.setPins(Audio::BCLK, Audio::LRCK, Audio::DOUT, -1, Audio::MCLK);
    bool ok = i2s.begin(I2S_MODE_STD, Audio::SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    ok = ok && Audio::begin(0xE0);
    ok = ok && Esp32C5::createPinnedTask(toneTask, "tone", 4096, nullptr, 5) == pdPASS;
    if (!ok) { result("Speaker switch", "FAIL", TFT_RED, "audio did not start"); return; }
    toneOn = true;
    prompt("listen: TONE");            Serial.println("[extras_35] amp on");  delay(2000);
    const bool off = Expander::setSpeakerAmp(false);
    prompt("listen: SILENCE");         Serial.println("[extras_35] amp off"); delay(2000);
    const bool on = Expander::setSpeakerAmp(true);
    prompt("listen: TONE");            Serial.println("[extras_35] amp on");  delay(2000);
    toneOn = false;
    delay(100);
    Audio::end();
    prompt("");
    result("Speaker switch", off && on ? "SENT" : "FAIL", off && on ? TFT_CYAN : TFT_RED,
           off && on ? "you should have heard tone, silence, tone" : "expander write failed");
}

// 4. One short press inside 15 s.
static void checkButton() {
    prompt("press PWR once, briefly (15 s)");
    bool pressed = false, sawDown = false, sawUp = false;
    int fails = 0;
    const uint32_t t0 = millis();
    while (millis() - t0 < 15000 && !(sawDown && sawUp)) {
        if (!Expander::powerButton(pressed)) fails++;
        else if (pressed) sawDown = true;
        else if (sawDown) sawUp = true;
        delay(40);
    }
    prompt("");
    char d[48];
    snprintf(d, sizeof d, "%d failed reads", fails);
    if (sawDown && sawUp) result("PWR button", "PASS", TFT_GREEN, d);
    else if (fails)       result("PWR button", "FAIL", TFT_RED, d);
    else                  result("PWR button", "NO PRESS", TFT_YELLOW, "not pressed within 15 s");
}

void setup() {
    Serial.begin(115200);
    const bool up = begin(0);
    tft.init();
    tft.setRotation(1);
    tft.invertDisplay(PANEL_NEEDS_INVERSION);
    heading("extras_35");
    delay(150);
    Expander::backlight(255);
    Serial.printf("\n[extras_35] expander %s\n", up ? "up" : "MISSING");
    if (!up) { result("Board", "FAIL", TFT_RED, "expander did not answer"); return; }
    checkCharge();
    checkHeader();
    checkAmp();
    checkButton();
    Serial.println("[extras_35] done");
}

void loop() { delay(1000); }
