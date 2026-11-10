// SPDX-License-Identifier: MIT
// Waveshare 3.5-inch microphone capture test: three 1-second windows from the
// onboard mic through the ES8311 ADC, with per-channel level statistics
// printed over serial, then capture stops. No audio is stored or played, and
// no display is rendered (backlight off).
//
// Hardware-tested 2026-11-10 for level response only: the left-channel level
// stayed low in silent runs and rose when sound was made. One separate
// 3-second speech capture at these settings was judged clear by the owner.
// Startup order matches audio_35: board expander/shared bus, I2S, then codec.
#include <Arduino.h>
#include <ESP_I2S.h>
#include <math.h>
#include <chip/esp32c5_i2c_bus.h>
#include <board/ws_c5_35/ws_c5_35_pins.h>
#include <board/ws_c5_35/ws_c5_35_expander.h>
#include <board/ws_c5_35/ws_c5_35_audio.h>

namespace Audio = WsC5_35::Audio;
static I2SClass micI2S;
static constexpr size_t kFrames = 256;
static int16_t pcm[kFrames * 2];   // interleaved L/R
static constexpr int kWindows = 3;

struct ChannelStats {
    int32_t min = 32767, max = -32768;
    int64_t sum = 0;
    uint64_t sumSq = 0;
    uint32_t n = 0;
    void add(int16_t s) {
        if (s < min) min = s;
        if (s > max) max = s;
        sum += s;
        sumSq += (int64_t)s * s;
        ++n;
    }
    // The mono ADC arrived on the left slot only in the 2026-11-10 hardware
    // run, so a constant right slot is expected; a constant left slot is not.
    void print(const char* name, bool expectSignal) const {
        const double mean = n ? (double)sum / n : 0.0;
        const double var = n ? (double)sumSq / n - mean * mean : 0.0;
        const char* note = "";
        if (min == max) note = expectSignal ? "  <-- FLAT (no signal)" : "  (unused slot)";
        else if (min <= -32768 || max >= 32767) note = "  <-- CLIPPED";
        Serial.printf("[mic_35]   %s min %ld max %ld mean %.1f rms(ac) %.1f%s\n",
                      name, (long)min, (long)max, mean, sqrt(var > 0 ? var : 0), note);
    }
};

static void shutdown() {
    if (!Audio::endRecording())
        Serial.println("[mic_35] codec mic-off unconfirmed");
    if (!micI2S.end())
        Serial.println("[mic_35] I2S shutdown failed");
}

// Reads exactly `frames` stereo frames; false on a short read.
static bool readFrames(size_t frames) {
    const size_t want = frames * 2 * sizeof(int16_t);
    return micI2S.readBytes((char*)pcm, want) == want;
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Esp32C5::I2cBus::begin(WsC5_35::I2C_SDA, WsC5_35::I2C_SCL, WsC5_35::I2C_HZ);
    if (!WsC5_35::Expander::begin(0)) {
        Serial.println("[mic_35] board startup FAILED");
        return;
    }
    // I2S master first: the ES8311 clocks off BCLK. RX only (DOUT unused),
    // MCLK physically unwired.
    micI2S.setPins(Audio::BCLK, Audio::LRCK, -1, Audio::DIN, Audio::MCLK);
    if (!micI2S.begin(I2S_MODE_STD, Audio::SAMPLE_RATE,
                      I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
        Serial.println("[mic_35] ESP_I2S begin FAILED");
        return;
    }
    if (!Audio::beginRecording()) {
        Serial.println("[mic_35] codec setup FAILED (no capture)");
        shutdown();
        return;
    }
    // Discard ~250 ms while the ADC and its DC-offset cancellation settle.
    for (uint32_t f = 0; f < Audio::SAMPLE_RATE / 4; f += kFrames) {
        if (!readFrames(kFrames)) {
            Serial.println("[mic_35] short I2S read during settle; stopping");
            shutdown();
            return;
        }
    }
    Serial.println("[mic_35] ES8311 ADC configured; capturing 3 x 1 s. "
                   "Stay quiet for window 1, then speak or clap.");
    for (int w = 1; w <= kWindows; ++w) {
        ChannelStats left, right;
        for (uint32_t f = 0; f < Audio::SAMPLE_RATE; f += kFrames) {
            if (!readFrames(kFrames)) {
                Serial.println("[mic_35] short I2S read; stopping");
                shutdown();
                return;
            }
            for (size_t i = 0; i < kFrames; ++i) {
                left.add(pcm[2 * i]);
                right.add(pcm[2 * i + 1]);
            }
        }
        Serial.printf("[mic_35] window %d/%d\n", w, kWindows);
        left.print("L", true);
        right.print("R", false);
    }
    shutdown();
    Serial.println("[mic_35] capture complete; stopped. Level statistics only: "
                   "this does not confirm audio quality.");
}

void loop() { delay(1000); }
