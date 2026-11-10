// SPDX-License-Identifier: MIT
// Waveshare 3.5-inch audio self-test: the board plays a short 1 kHz tone on
// its own speaker while recording its own microphone, then checks that the
// tone came back. No listener, cable or second device is needed.
//
// It runs I2S in both directions at once. In the playback configuration the
// codec's ADC stays enabled, the microphone arrives on the left I2S slot, and
// the right slot carries the codec's digital copy of what was sent to the DAC.
// The test uses a quiet level (PCM peak 3000, codec volume 0xC0) that
// measured undistorted; it is not the audio_35 listening level.
#include <Arduino.h>
#include <ESP_I2S.h>
#include <math.h>
#include <stdlib.h>
#include <chip/esp32c5_i2c_bus.h>
#include <board/ws_c5_35/ws_c5_35_pins.h>
#include <board/ws_c5_35/ws_c5_35_expander.h>
#include <board/ws_c5_35/ws_c5_35_audio.h>

namespace Audio = WsC5_35::Audio;
static I2SClass i2s;
static constexpr float kToneHz = 1000.0f;
static constexpr int kAmplitude = 3000;
static constexpr uint8_t kVolume = 0xC0;
static constexpr int kHalf = Audio::SAMPLE_RATE / 2;   // 0.5 s silence, then 0.5 s tone
static constexpr int kSkip = 1500;                     // settle after each half starts
static constexpr int kWindow = kHalf - 2 * kSkip + 1000;
static int16_t tonePcm[kHalf];
static int16_t mic[2 * kHalf], ref[2 * kHalf];
static int16_t tx[256 * 2], rx[256 * 2];

// Amplitude of one frequency in a block of samples (Goertzel).
static double level(const int16_t* x, int n, double hz) {
    const double c = 2.0 * cos(2.0 * 3.141592653589793 * hz / Audio::SAMPLE_RATE);
    double s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i) { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) * 2.0 / n;
}

static void stop() {
    if (!Audio::end()) Serial.println("[audio_selftest_35] codec mute unconfirmed");
    if (!i2s.end()) Serial.println("[audio_selftest_35] I2S shutdown failed");
}

static bool check(const char* what, bool ok) {
    Serial.printf("[audio_selftest_35]   %-46s %s\n", what, ok ? "ok" : "FAIL");
    return ok;
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Esp32C5::I2cBus::begin(WsC5_35::I2C_SDA, WsC5_35::I2C_SCL, WsC5_35::I2C_HZ);
    if (!WsC5_35::Expander::begin(0)) {
        Serial.println("[audio_selftest_35] board startup FAILED");
        return;
    }
    for (int n = 0; n < kHalf; ++n) {
        float gain = 1.0f;
        if (n < 80) gain = n / 80.0f;
        if (kHalf - n < 80) gain = (kHalf - n) / 80.0f;
        tonePcm[n] = (int16_t)(kAmplitude * gain * sinf(2.0f * 3.14159265f * kToneHz * n / Audio::SAMPLE_RATE));
    }
    // I2S master in both directions, then the codec (it clocks off BCLK).
    i2s.setPins(Audio::BCLK, Audio::LRCK, Audio::DOUT, Audio::DIN, Audio::MCLK);
    if (!i2s.begin(I2S_MODE_STD, Audio::SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                   I2S_SLOT_MODE_STEREO)) {
        Serial.println("[audio_selftest_35] ESP_I2S begin FAILED");
        return;
    }
    if (!Audio::begin(kVolume)) {
        Serial.println("[audio_selftest_35] codec setup FAILED");
        stop();
        return;
    }
    // About 0.25 s of settling, then silence for frames 0..kHalf-1 and the tone
    // for kHalf..2*kHalf-1, writing and reading one block at a time.
    bool io = true;
    for (int f = -4096; io && f < 2 * kHalf; f += 256) {
        for (int i = 0; i < 256; ++i) {
            const int n = f + i;
            tx[2 * i] = tx[2 * i + 1] = (n >= kHalf && n < 2 * kHalf) ? tonePcm[n - kHalf] : 0;
        }
        io = i2s.write((const uint8_t*)tx, sizeof(tx)) == sizeof(tx) &&
             i2s.readBytes((char*)rx, sizeof(rx)) == sizeof(rx);
        for (int i = 0; io && i < 256; ++i) {
            const int n = f + i;
            if (n >= 0 && n < 2 * kHalf) { mic[n] = rx[2 * i]; ref[n] = rx[2 * i + 1]; }
        }
    }
    stop();
    if (!io) {
        Serial.println("[audio_selftest_35] short I2S transfer; result: FAIL");
        return;
    }

    const double refQuiet = level(ref + kSkip, kWindow, kToneHz);
    const double refTone  = level(ref + kHalf + kSkip, kWindow, kToneHz);
    const double micQuiet = level(mic + kSkip, kWindow, kToneHz);
    const double micTone  = level(mic + kHalf + kSkip, kWindow, kToneHz);
    const double micH2    = level(mic + kHalf + kSkip, kWindow, 2 * kToneHz);
    const double micH3    = level(mic + kHalf + kSkip, kWindow, 3 * kToneHz);
    int peak = 0;
    for (int i = kHalf + kSkip; i < kHalf + kSkip + kWindow; ++i) if (abs(mic[i]) > peak) peak = abs(mic[i]);

    Serial.printf("[audio_selftest_35] sent to DAC (right slot): quiet %.1f, tone %.1f (expected %d)\n",
                  refQuiet, refTone, kAmplitude);
    Serial.printf("[audio_selftest_35] microphone (left slot):   quiet %.1f, tone %.1f, peak %d\n",
                  micQuiet, micTone, peak);
    Serial.printf("[audio_selftest_35] microphone harmonics:     2nd %.1f%%, 3rd %.1f%% of the tone\n",
                  micTone > 0 ? 100.0 * micH2 / micTone : 0.0, micTone > 0 ? 100.0 * micH3 / micTone : 0.0);

    bool pass = true;
    pass &= check("digital path carries the tone (within 10%)", fabs(refTone - kAmplitude) < 0.10 * kAmplitude);
    pass &= check("digital path silent before the tone", refQuiet < 20.0);
    pass &= check("microphone hears the tone (level over 200)", micTone > 200.0);
    pass &= check("tone at least 20x the quiet level", micTone > 20.0 * (micQuiet > 1.0 ? micQuiet : 1.0));
    pass &= check("microphone not clipped", peak < 32000);
    Serial.printf("[audio_selftest_35] result: %s\n", pass ? "PASS" : "FAIL");
}

void loop() { delay(1000); }
