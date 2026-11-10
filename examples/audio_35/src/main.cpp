// SPDX-License-Identifier: MIT
// Waveshare 3.5-inch speaker test: three 500 ms tones with 1500 ms silence.
// Clean playback confirmed by the owner on 2026-11-10; then playback stops.
// Initialize the board expander before I2S/codec setup. Adding this step
// changed the listen result from silence to clean playback; the specific
// electrical dependency has not been isolated. No display is rendered.
#include <Arduino.h>
#include <ESP_I2S.h>
#include <math.h>
#include <chip/esp32c5_tasks.h>
#include <chip/esp32c5_i2c_bus.h>
#include <board/ws_c5_35/ws_c5_35_pins.h>
#include <board/ws_c5_35/ws_c5_35_expander.h>
#include <board/ws_c5_35/ws_c5_35_audio.h>
#include "audio_shutdown.h"

namespace Audio = WsC5_35::Audio;
static I2SClass audioI2S;
static int16_t tonePcm[Audio::SAMPLE_RATE / 2];
static void audioTask(void*);
// Exact gain settings used in the clean hardware listen test.
static constexpr float toneAmplitude = 12000.0f;
static constexpr uint8_t codecVolume = 0xE0;

void setup() {
    Serial.begin(115200);
    delay(1500);
    Esp32C5::I2cBus::begin(WsC5_35::I2C_SDA, WsC5_35::I2C_SCL, WsC5_35::I2C_HZ);
    if (!WsC5_35::Expander::begin(0)) {
        Serial.println("[audio_35] board startup FAILED");
        return;
    }
    // Start the I2S master FIRST: the ES8311 clocks off BCLK, so the clocks
    // must be running before the codec is configured. TX only (DIN unused),
    // MCLK physically unwired.
    audioI2S.setPins(Audio::BCLK, Audio::LRCK, Audio::DOUT, -1, Audio::MCLK);
    if (!audioI2S.begin(I2S_MODE_STD, Audio::SAMPLE_RATE,
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
        Serial.println("[audio_35] ESP_I2S begin FAILED");
        return;
    }
    if (!Audio::begin(codecVolume)) {
        Serial.println("[audio_35] codec setup FAILED (no listen test)");
        Audio35Example::shutdown(audioI2S);
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
    if (Esp32C5::createPinnedTask(audioTask, "audio_35", 4096, nullptr, 5) != pdPASS) {
        Serial.println("[audio_35] audio task creation FAILED");
        Audio35Example::shutdown(audioI2S);
        return;
    }
    Serial.println("[audio_35] ES8311 configured; running three nominal 440 Hz beeps "
                   "(note any crackle)");
}

// Precomputed PCM and a priority-5 producer keep the feed path lightweight.
// ESP_I2S writes block on DMA availability; no steady-state logging here.
static void audioTask(void*) {
    uint32_t frame = 0;
    int16_t pcm[256 * 2];
    for (;;) {
        if (frame >= Audio::SAMPLE_RATE * 6) {
            Audio35Example::shutdown(audioI2S);
            Serial.println("[audio_35] three-beep test complete; playback stopped");
            vTaskDelete(nullptr);
        }
        for (size_t i = 0; i < 256; ++i) {
            const uint32_t pos = frame++ % (Audio::SAMPLE_RATE * 2);
            const int16_t sample = pos < Audio::SAMPLE_RATE / 2 ? tonePcm[pos] : 0;
            pcm[2 * i] = pcm[2 * i + 1] = sample;
        }
        const size_t written = audioI2S.write((const uint8_t*)pcm, sizeof(pcm));
        if (written != sizeof(pcm)) {
            Serial.println("[audio_35] short I2S write; stopping playback");
            Audio35Example::shutdown(audioI2S);
            vTaskDelete(nullptr);
        }
    }
}

void loop() {
    // Playback is owned by the audio task; leave the main task idle.
    delay(1000);
}
