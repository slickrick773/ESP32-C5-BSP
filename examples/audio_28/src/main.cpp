// SPDX-License-Identifier: MIT
// 440 Hz for 500 ms, then 1500 ms silence. the owner confirmed audible beeps on hardware, 2026-10-31.
#include <Arduino.h>
#include <ESP_I2S.h>
#include <math.h>
#include <chip/esp32c5_tasks.h>
#include <board/ws_c5_28/ws_c5_28_audio.h>
#include <board/ws_c5_28/ws_c5_28_expander.h>
#include "audio_shutdown.h"

namespace Audio = WsC5_28::Audio;
static I2SClass audioI2S;
static int16_t tonePcm[Audio::SAMPLE_RATE / 2];
static void audioTask(void*);
// the owner confirmed crackle-free playback at this peak with the audio task.
static constexpr float toneAmplitude = 1000.0f;

void setup() {
    Serial.begin(115200);
    delay(1500);
    if (!WsC5_28::Expander::begin() ||
        !WsC5_28::Expander::setOutput(Audio::PA_CTRL, false)) {
        Serial.println("[audio_28] expander/amp-disable FAILED");
        return;
    }
    // TX only: DIN=27 is documented but unused. MCLK is physically unwired.
    audioI2S.setPins(Audio::BCLK, Audio::LRCK, Audio::DOUT, -1, Audio::MCLK);
    if (!audioI2S.begin(I2S_MODE_STD, Audio::SAMPLE_RATE,
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
        Serial.println("[audio_28] ESP_I2S begin FAILED");
        return;
    }
    if (!Audio::begin(0xE0)) {
        Serial.println("[audio_28] codec/amp setup FAILED (no listen test)");
        Audio28Example::shutdown(audioI2S);
        return;
    }
    for (uint32_t i = 0; i < Audio::SAMPLE_RATE / 2; ++i) {
        const uint32_t ramp = Audio::SAMPLE_RATE / 200;
        const uint32_t remaining = Audio::SAMPLE_RATE / 2 - i;
        float gain = 1.0f;
        if (i < ramp) gain = (float)i / ramp;
        if (remaining < ramp) gain = (float)remaining / ramp;
        tonePcm[i] = (int16_t)(toneAmplitude * gain *
            sinf(2.0f * 3.14159265358979323846f * 440.0f * i / Audio::SAMPLE_RATE));
    }
    if (Esp32C5::createPinnedTask(audioTask, "audio_28", 4096, nullptr, 5) != pdPASS) {
        Serial.println("[audio_28] audio task creation FAILED");
        Audio28Example::shutdown(audioI2S);
        return;
    }
    Serial.println("[audio_28] ES8311 ready; listen for repeating 440 Hz beeps");
}

// Precomputed PCM and a priority-5 producer keep the feed path lightweight.
// ESP_I2S writes block on DMA availability; no steady-state logging here.
static void audioTask(void*) {
    uint32_t frame = 0;
    int16_t pcm[256 * 2];
    for (;;) {
        for (size_t i = 0; i < 256; ++i) {
            const uint32_t pos = frame++ % (Audio::SAMPLE_RATE * 2);
            const int16_t sample = pos < Audio::SAMPLE_RATE / 2 ? tonePcm[pos] : 0;
            pcm[2 * i] = pcm[2 * i + 1] = sample;
        }
        const size_t written = audioI2S.write((const uint8_t*)pcm, sizeof(pcm));
        if (written != sizeof(pcm)) {
            Serial.println("[audio_28] short I2S write; stopping playback");
            Audio28Example::shutdown(audioI2S);
            vTaskDelete(nullptr);
        }
    }
}

void loop() {
    // Playback is owned by the audio task; leave the main task idle.
    delay(1000);
}
