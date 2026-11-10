// SPDX-License-Identifier: MIT
// Waveshare 3.5-inch combined display + touch + audio sample. The scrolling
// list and redraw path are the touch_redraw sample's. TONE plays one short
// 440 Hz tone; MIC captures one second and shows the left-channel level.
// Each audio action starts I2S, runs to completion on the loop task (touch
// and redraw pause meanwhile), then stops I2S again. Audio startup order and
// settings are those of the standalone audio_35 and mic_35 examples.
#include <Arduino.h>
#include <ESP_I2S.h>
#include <TFT_eSPI.h>
#include <cmath>
#include <cstdio>
#include <board/ws_c5_35/ws_c5_35_audio.h>
#include "boards/ws_c5_35.h"
#include "graphics/tft_espi_display.h"
#include "sample_ui/presentation.h"
#include "sample_ui/scroll.h"
#include "sample_ui/slick.h"

namespace {
using namespace sample_ui;

enum : Action { Tone = 1, Mic = 2 };
namespace Audio = WsC5_35::Audio;
constexpr ListRow rows[] = {
    {1, "DISPLAY", "ST7796", 101},
    {2, "TOUCH", "FT6336", 102},
    {3, "CODEC", "ES8311", 103},
    {4, "SPEAKER", "TAP TONE", 104},
    {5, "MICROPHONE", "TAP MIC", 105},
    {6, "SAMPLE RATE", "16 KHZ", 106},
    {7, "I2S", "ONE DIRECTION AT A TIME", 107},
    {8, "TOUCH INPUT", "DRAG TO SCROLL", 108},
    {9, "REDRAW", "CHANGED REGION", 109},
    {10, "END OF LIST", "", 110},
};
constexpr auto rowCount = sizeof(rows) / sizeof(rows[0]);

boards::WsC535 board;
TFT_eSPI tft;
graphics::TftEsPiDisplay display(tft);
Input touchInput;
ScrollInput scrollInput(touchInput);
char toneLabel[24] = "TONE";
char micLabel[24] = "MIC";
ListModel model{"TOUCH AND AUDIO", rows, rowCount, 0, 0,
                {{{toneLabel, Tone}, {micLabel, Mic}}}, 0};
I2SClass i2s;
int16_t pcm[256 * 2];
constexpr uint32_t toneFrames = Audio::SAMPLE_RATE * 2 / 5;   // 400 ms
int16_t tonePcm[toneFrames];
uint32_t tones = 0;
ListScreen screen(model);
uint16_t strip[480 * 8];
Renderer renderer(strip, sizeof(strip) / sizeof(strip[0]));
bool ready = false;
bool revealed = false;
int pendingOffset = -1;

void repaint(Rect damage = {}) {
    ready = renderer.begin(screen, board.landscape, slick, 8, damage);
}

// The tone is rendered once at startup. Computing it sample by sample while
// playing took about 24 ms per 16 ms block on this chip (measured), which
// starved the I2S stream and made the tone audibly pulse.
void renderTone() {
    const uint32_t ramp = Audio::SAMPLE_RATE / 200;
    for (uint32_t n = 0; n < toneFrames; ++n) {
        float gain = 1.0f;
        if (n < ramp) gain = static_cast<float>(n) / ramp;
        if (toneFrames - n < ramp) gain = static_cast<float>(toneFrames - n) / ramp;
        tonePcm[n] = static_cast<int16_t>(
            12000.0f * gain * sinf(2.0f * 3.14159265f * 440.0f * n / Audio::SAMPLE_RATE));
    }
}

// One 400 ms 440 Hz tone at the audio_35 settings (PCM peak 12000, codec
// volume 0xE0), then ~100 ms of zeros so the tail leaves the DMA buffer.
bool playTone() {
    i2s.setPins(Audio::BCLK, Audio::LRCK, Audio::DOUT, -1, Audio::MCLK);
    if (!i2s.begin(I2S_MODE_STD, Audio::SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                   I2S_SLOT_MODE_STEREO)) return false;
    bool ok = Audio::begin(0xE0);
    const uint32_t total = toneFrames + Audio::SAMPLE_RATE / 10;
    for (uint32_t f = 0; ok && f < total; f += 256) {
        for (uint32_t i = 0; i < 256; ++i) {
            const uint32_t n = f + i;
            pcm[2 * i] = pcm[2 * i + 1] = n < toneFrames ? tonePcm[n] : 0;
        }
        ok = i2s.write(reinterpret_cast<const uint8_t*>(pcm), sizeof(pcm)) == sizeof(pcm);
    }
    if (!Audio::end()) ok = false;
    if (!i2s.end()) ok = false;
    return ok;
}

// One second from the microphone at the mic_35 settings. Reports the AC RMS
// of the left I2S slot (where the mono ADC data arrives) and whether any
// sample reached full scale.
bool captureLevel(uint32_t& rms, bool& clipped) {
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
        ok = i2s.readBytes(reinterpret_cast<char*>(pcm), sizeof(pcm)) == sizeof(pcm);
        if (!ok || f < settle) continue;
        for (uint32_t i = 0; i < 256; ++i) {
            const int32_t s = pcm[2 * i];
            sum += s;
            sumSq += static_cast<int64_t>(s) * s;
            if (s <= -32768 || s >= 32767) clipped = true;
            ++n;
        }
    }
    if (!Audio::endRecording()) ok = false;
    if (!i2s.end()) ok = false;
    if (ok && n) {
        const double mean = static_cast<double>(sum) / n;
        const double var = static_cast<double>(sumSq) / n - mean * mean;
        rms = static_cast<uint32_t>(sqrt(var > 0 ? var : 0));
    }
    return ok && n;
}

void apply(Action action) {
    if (action == Tone) {
        const bool ok = playTone();
        if (ok) ++tones;
        if (ok) std::snprintf(toneLabel, sizeof(toneLabel), "TONE %lu",
                              static_cast<unsigned long>(tones));
        else std::snprintf(toneLabel, sizeof(toneLabel), "TONE FAIL");
        Serial.printf("[touch_audio] tone %s\n", ok ? "played" : "FAILED");
    } else if (action == Mic) {
        uint32_t rms = 0;
        bool clipped = false;
        const bool ok = captureLevel(rms, clipped);
        if (ok) std::snprintf(micLabel, sizeof(micLabel), "MIC %lu%s",
                              static_cast<unsigned long>(rms), clipped ? " CLIP" : "");
        else std::snprintf(micLabel, sizeof(micLabel), "MIC FAIL");
        Serial.printf("[touch_audio] mic %s rms %lu%s\n", ok ? "ok" : "FAILED",
                      static_cast<unsigned long>(rms), clipped ? " clipped" : "");
    } else {
        model.selected = static_cast<ItemId>(action - 100);
    }
}
}

void setup() {
    Serial.begin(115200);
    if (!board.begin()) {
        Serial.println("Board initialization failed");
        return;
    }
    display.begin(board.rotation, board.inverted);
    renderTone();
    repaint();
}

void loop() {
    if (!ready) {
        delay(10);
        return;
    }

    const auto layout = screen.layout(board.landscape);
    PointerEvent event{};
    if (board.touchEvent(millis(), touchInput.generation(), event)) {
        const auto result = scrollInput.dispatch(
            event, screen.hits(board.landscape), layout.viewport,
            model.scrollOffset, listScrollLimit(board.landscape, rowCount), revealed);
        if (result.scrolling) pendingOffset = result.offset;
        if (result.action == Tone || result.action == Mic ||
            (result.action && !renderer.busy())) {
            const bool interrupted = renderer.busy();
            renderer.cancel();
            apply(result.action);
            const bool footer = result.action == Tone || result.action == Mic;
            repaint(interrupted ? Rect{} : footer ? layout.frame.footer : layout.viewport);
        }
    }

    if (ready && renderer.busy() && renderer.step(display) == Step::Complete && !revealed) {
        board.show();
        revealed = true;
    }
    if (ready && !renderer.busy() && pendingOffset >= 0) {
        const int offset = pendingOffset;
        pendingOffset = -1;
        if (offset != model.scrollOffset) {
            model.scrollOffset = offset;
            repaint(layout.viewport);
        }
    }
    delay(1);
}
