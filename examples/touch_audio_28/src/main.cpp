// SPDX-License-Identifier: MIT
// Waveshare 2.8-inch combined display + touch + audio sample.
//
// Two on-screen buttons: TONE plays a one-second 440 Hz tone from a
// background task while the screen keeps updating (a running counter shows
// it), and MIC captures one second on the loop task and shows the level.
// Playback and capture are one at a time.
//
// Display: Arduino_GFX with the ST7789 init from hello_board_28. Audio level:
// the audio_28 listening level (PCM peak 1000, codec volume 0xE0).
#include <Arduino.h>
#include <ESP_I2S.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>
#include <atomic>
#include <math.h>
#include <chip/esp32c5_tasks.h>
#include <board/ws_c5_28/ws_c5_28_board.h>
#include <board/ws_c5_28/ws_c5_28_audio.h>
#include "ws_c5_28_display.h"

using namespace WsC5_28;

static constexpr int32_t SPI_HZ = 40000000;
static Arduino_DataBus* bus =
    new Arduino_HWSPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO, &SPI);
static Arduino_GFX* gfx =
    new Arduino_ST7789(bus, LCD_RST, ROTATION, true /*IPS*/, PANEL_W, PANEL_H,
                       0, 0, 0, 0, ws28Init, sizeof ws28Init);

// Screen is 320x240 in landscape.
static constexpr int kButtonY = 70, kButtonH = 110, kToneX = 10, kMicX = 165, kButtonW = 145;
static constexpr int kStatusY = 196;

static I2SClass i2s;
static int16_t pcm[256 * 2];
static constexpr uint32_t kToneFrames = Audio::SAMPLE_RATE;   // one second
static int16_t tonePcm[kToneFrames];
enum : int { ToneIdle, TonePlaying, ToneDone, ToneFailed };
static std::atomic<int> toneState{ToneIdle};
static TaskHandle_t toneTask = nullptr;
static uint32_t tones = 0;

// Rendered once at startup: generating samples while streaming is slower than
// real time on this chip and makes the tone pulse.
static void renderTone() {
    const uint32_t ramp = Audio::SAMPLE_RATE / 200;
    for (uint32_t n = 0; n < kToneFrames; ++n) {
        float gain = 1.0f;
        if (n < ramp) gain = (float)n / ramp;
        if (kToneFrames - n < ramp) gain = (float)(kToneFrames - n) / ramp;
        tonePcm[n] = (int16_t)(1000.0f * gain * sinf(2.0f * 3.14159265f * 440.0f * n / Audio::SAMPLE_RATE));
    }
}

static bool playTone() {
    i2s.setPins(Audio::BCLK, Audio::LRCK, Audio::DOUT, -1, Audio::MCLK);
    if (!i2s.begin(I2S_MODE_STD, Audio::SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                   I2S_SLOT_MODE_STEREO)) return false;
    bool ok = Audio::begin(0xE0);
    const uint32_t total = kToneFrames + Audio::SAMPLE_RATE / 10;
    for (uint32_t f = 0; ok && f < total; f += 256) {
        for (uint32_t i = 0; i < 256; ++i) {
            const uint32_t n = f + i;
            pcm[2 * i] = pcm[2 * i + 1] = n < kToneFrames ? tonePcm[n] : 0;
        }
        ok = i2s.write((const uint8_t*)pcm, sizeof(pcm)) == sizeof(pcm);
    }
    if (!Audio::end()) ok = false;     // amplifier off
    if (!i2s.end()) ok = false;
    return ok;
}

static void toneTaskMain(void*) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        toneState.store(playTone() ? ToneDone : ToneFailed);
    }
}

static bool captureLevel(uint32_t& rms, bool& clipped) {
    i2s.setPins(Audio::BCLK, Audio::LRCK, -1, Audio::DIN, Audio::MCLK);
    if (!i2s.begin(I2S_MODE_STD, Audio::SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                   I2S_SLOT_MODE_STEREO)) return false;
    bool ok = Audio::beginRecording();
    int64_t sum = 0;
    uint64_t sumSq = 0;
    uint32_t n = 0;
    clipped = false;
    const uint32_t settle = Audio::SAMPLE_RATE / 4;
    for (uint32_t f = 0; ok && f < settle + Audio::SAMPLE_RATE; f += 256) {
        ok = i2s.readBytes((char*)pcm, sizeof(pcm)) == sizeof(pcm);
        if (!ok || f < settle) continue;
        for (uint32_t i = 0; i < 256; ++i) {
            const int32_t s = pcm[2 * i];
            sum += s;
            sumSq += (int64_t)s * s;
            if (s <= -32768 || s >= 32767) clipped = true;
            ++n;
        }
    }
    if (!Audio::endRecording()) ok = false;
    if (!i2s.end()) ok = false;
    if (ok && n) {
        const double mean = (double)sum / n;
        const double var = (double)sumSq / n - mean * mean;
        rms = (uint32_t)sqrt(var > 0 ? var : 0);
    }
    return ok && n;
}

static void drawButton(int x, const char* label, uint16_t color) {
    gfx->fillRoundRect(x, kButtonY, kButtonW, kButtonH, 10, color);
    gfx->setTextColor(RGB565_BLACK);
    gfx->setTextSize(3);
    gfx->setCursor(x + 28, kButtonY + 42);
    gfx->print(label);
}

static void status(const char* text) {
    gfx->fillRect(0, kStatusY, 320, 20, RGB565_BLACK);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(10, kStatusY);
    gfx->print(text);
}

void setup() {
    Serial.begin(115200);
    const bool exio = begin(0);
    delay(300);
    Serial.printf("[touch_audio_28] expander %s, touch %s\n", exio ? "up" : "MISSING",
                  Touch::present() ? "present" : "missing");
    if (!gfx->begin(SPI_HZ)) {
        Serial.println("[touch_audio_28] gfx->begin() FAILED");
        for (;;) delay(1000);
    }
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextColor(RGB565_GREEN);
    gfx->setTextSize(2);
    gfx->setCursor(10, 10);
    gfx->print("TOUCH AND AUDIO");
    drawButton(kToneX, "TONE", RGB565_ORANGE);
    drawButton(kMicX, "MIC", RGB565_CYAN);
    status("tap a button");
    renderTone();
    if (Esp32C5::createPinnedTask(toneTaskMain, "tone", 4096, nullptr, 5, &toneTask) != pdPASS)
        Serial.println("[touch_audio_28] tone task creation FAILED; TONE disabled");
    Expander::backlight(255);
}

void loop() {
    static uint32_t lastSample = 0, lastTick = 0, ticks = 0;
    static bool wasDown = false;
    const uint32_t now = millis();

    // A counter redrawn ten times a second: visible proof the display keeps
    // running while a tone plays.
    if (now - lastTick >= 100) {
        lastTick = now;
        char buf[16];
        snprintf(buf, sizeof(buf), "%6lu", (unsigned long)++ticks);
        gfx->fillRect(230, 40, 90, 18, RGB565_BLACK);
        gfx->setTextColor(RGB565_YELLOW);
        gfx->setTextSize(2);
        gfx->setCursor(230, 40);
        gfx->print(buf);
    }

    const int state = toneState.load();
    if (state == ToneDone || state == ToneFailed) {
        char buf[32];
        if (state == ToneDone) snprintf(buf, sizeof(buf), "tone %lu played", (unsigned long)++tones);
        else snprintf(buf, sizeof(buf), "tone FAILED");
        status(buf);
        Serial.printf("[touch_audio_28] tone %s\n", state == ToneDone ? "played" : "FAILED");
        toneState.store(ToneIdle);
    }

    if (now - lastSample < 8) return;
    lastSample = now;
    uint16_t x, y;
    const bool down = Touch::poll(x, y);
    if (down && !wasDown) {
        int sx, sy;
        Touch::toLandscape(x, y, sx, sy);
        const bool inRow = sy >= kButtonY && sy < kButtonY + kButtonH;
        if (inRow && sx >= kToneX && sx < kToneX + kButtonW) {
            if (toneState.load() == ToneIdle && toneTask) {
                toneState.store(TonePlaying);
                status("playing...");
                xTaskNotifyGive(toneTask);
            }
        } else if (inRow && sx >= kMicX && sx < kMicX + kButtonW) {
            if (toneState.load() == ToneIdle) {
                status("listening...");
                uint32_t rms = 0;
                bool clipped = false;
                const bool ok = captureLevel(rms, clipped);
                char buf[32];
                if (ok) snprintf(buf, sizeof(buf), "mic %lu%s", (unsigned long)rms, clipped ? " CLIP" : "");
                else snprintf(buf, sizeof(buf), "mic FAILED");
                status(buf);
                Serial.printf("[touch_audio_28] mic %s rms %lu%s\n", ok ? "ok" : "FAILED",
                              (unsigned long)rms, clipped ? " clipped" : "");
            }
        } else {
            Serial.printf("[touch_audio_28] tap at %d,%d (outside the buttons)\n", sx, sy);
        }
    }
    wasDown = down;
}
