// SPDX-License-Identifier: MIT
// Whole-board check for the Waveshare ESP32-C5-Touch-LCD boards. Runs once
// after reset, exercises every onboard device the BSP drives, and prints one
// table: PASS, FAIL, or SKIP where the check does not apply (no card fitted,
// no GPS wired). Nothing is drawn; the backlight stays off.
//
// The same source builds for both boards: board_check_35 defines
// BOARD_CHECK_35 and board_check_28 defines BOARD_CHECK_28.
//
// Keep the board still and reasonably quiet while it runs (about 6 seconds):
// the IMU check expects 1 g at rest and the audio check plays a short quiet
// tone and listens for it. On the 2.8-inch board the speaker must be plugged
// in for the audio check to pass.
#include <Arduino.h>
#include <ESP_I2S.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <chip/esp32c5_i2c_bus.h>

#if defined(BOARD_CHECK_28)
#include <board/ws_c5_28/ws_c5_28_board.h>
#include <board/ws_c5_28/ws_c5_28_audio.h>
#include <board/ws_c5_28/ws_c5_28_battery.h>
#include <board/ws_c5_28/ws_c5_28_rtc.h>
#include <board/ws_c5_28/ws_c5_28_imu.h>
#include <board/ws_c5_28/ws_c5_28_env.h>
namespace B = WsC5_28;
#define TAG "[board_check_28]"
#else
#include <board/ws_c5_35/ws_c5_35_board.h>
#include <board/ws_c5_35/ws_c5_35_audio.h>
#include <board/ws_c5_35/ws_c5_35_battery.h>
#include <board/ws_c5_35/ws_c5_35_rtc.h>
#include <board/ws_c5_35/ws_c5_35_imu.h>
#include <board/ws_c5_35/ws_c5_35_env.h>
namespace B = WsC5_35;
#define TAG "[board_check_35]"
#endif

enum class Result { Pass, Fail, Skip };
static int passed = 0, failed = 0, skipped = 0;

static void report(const char* what, Result r, const char* detail) {
    const char* word = r == Result::Pass ? "PASS" : r == Result::Fail ? "FAIL" : "SKIP";
    if (r == Result::Pass) ++passed; else if (r == Result::Fail) ++failed; else ++skipped;
    Serial.printf(TAG "  %-4s  %-22s %s\n", word, what, detail);
}

// ---- audio: play a quiet 1 kHz tone and hear it on the board's own mic ------
static I2SClass i2s;
static constexpr int kHalf = B::Audio::SAMPLE_RATE / 2, kSkip = 1500, kWindow = kHalf - 2 * kSkip + 1000;
static int16_t toneBuf[kHalf], mic[2 * kHalf], ref[2 * kHalf], tx[512], rx[512];

static double level(const int16_t* x, int n, double hz) {
    const double c = 2.0 * cos(2.0 * 3.141592653589793 * hz / B::Audio::SAMPLE_RATE);
    double s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i) { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) * 2.0 / n;
}

static void checkAudio() {
    char detail[96];
    for (int n = 0; n < kHalf; ++n) {
        float g = 1.0f;
        if (n < 80) g = n / 80.0f;
        if (kHalf - n < 80) g = (kHalf - n) / 80.0f;
        toneBuf[n] = (int16_t)(3000 * g * sinf(2.0f * 3.14159265f * 1000.0f * n / B::Audio::SAMPLE_RATE));
    }
    i2s.setPins(B::Audio::BCLK, B::Audio::LRCK, B::Audio::DOUT, B::Audio::DIN, B::Audio::MCLK);
    if (!i2s.begin(I2S_MODE_STD, B::Audio::SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
        report("audio codec", Result::Fail, "I2S did not start");
        return;
    }
    if (!B::Audio::begin(0xC0)) {
        i2s.end();
        report("audio codec", Result::Fail, "codec did not answer or configure");
        return;
    }
    report("audio codec", Result::Pass, "ES8311 configured");
    bool io = true;
    for (int f = -4096; io && f < 2 * kHalf; f += 256) {
        for (int i = 0; i < 256; ++i) {
            const int n = f + i;
            tx[2 * i] = tx[2 * i + 1] = (n >= kHalf && n < 2 * kHalf) ? toneBuf[n - kHalf] : 0;
        }
        io = i2s.write((const uint8_t*)tx, sizeof(tx)) == sizeof(tx) &&
             i2s.readBytes((char*)rx, sizeof(rx)) == sizeof(rx);
        for (int i = 0; io && i < 256; ++i) {
            const int n = f + i;
            if (n >= 0 && n < 2 * kHalf) { mic[n] = rx[2 * i]; ref[n] = rx[2 * i + 1]; }
        }
    }
    B::Audio::end();
    i2s.end();
    if (!io) { report("speaker to microphone", Result::Fail, "short I2S transfer"); return; }
    const double refTone = level(ref + kHalf + kSkip, kWindow, 1000), micQuiet = level(mic + kSkip, kWindow, 1000),
                 micTone = level(mic + kHalf + kSkip, kWindow, 1000);
    int peak = 0;
    for (int i = kHalf + kSkip; i < kHalf + kSkip + kWindow; ++i) if (abs(mic[i]) > peak) peak = abs(mic[i]);
    const bool ok = fabs(refTone - 3000) < 300 && micTone > 200 && micTone > 20 * (micQuiet > 1 ? micQuiet : 1) && peak < 32000;
    snprintf(detail, sizeof(detail), "sent %.0f, heard %.0f (quiet %.1f)%s", refTone, micTone, micQuiet,
             ok ? "" : "; is a speaker connected?");
    report("speaker to microphone", ok ? Result::Pass : Result::Fail, detail);
}

static void checkSd() {
    char detail[96];
    SPI.begin(B::LCD_SCLK, B::LCD_MISO, B::LCD_MOSI);
    if (!SD.begin(B::SD_CS, SPI, 4000000)) {
        report("microSD", Result::Skip, "no card mounted");
        return;
    }
    static const char payload[] = "ESP32-C5-BSP board_check write/read 0123456789\n";
    const char* path = "/bsp_board_check.txt";
    bool ok = false;
    File f = SD.open(path, FILE_WRITE);
    if (f) {
        const size_t wrote = f.print(payload);
        f.close();
        f = SD.open(path, FILE_READ);
        char back[sizeof(payload)] = {};
        const size_t got = f ? f.readBytes(back, sizeof(back) - 1) : 0;
        if (f) f.close();
        ok = wrote == strlen(payload) && got == wrote && memcmp(back, payload, got) == 0;
    }
    const bool removed = SD.remove(path);
    snprintf(detail, sizeof(detail), "%llu MB card, write/read-back %s", SD.cardSize() / (1024ULL * 1024ULL),
             ok && removed ? "matched" : "FAILED");
    SD.end();
    report("microSD", ok && removed ? Result::Pass : Result::Fail, detail);
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    char detail[96];
    Serial.printf(TAG " %s\n", B::boardName());

    report("I/O expander", B::begin(0) ? Result::Pass : Result::Fail, "CH32V006 at 0x24");
    report("touch controller", B::Touch::present() ? Result::Pass : Result::Fail,
           B::Touch::present() ? "answered" : "did not answer");

#if defined(BOARD_CHECK_28)
    B::Battery::begin();
    B::Battery::Reading batt{};
    if (B::Battery::read(batt)) {
        snprintf(detail, sizeof(detail), "battery input reads about %u mV (not a charge level)", batt.voltageMv);
        report("battery monitor", Result::Pass, detail);
    } else {
        report("battery monitor", Result::Fail, "expander ADC read failed");
    }
#else
    if (B::Battery::begin()) {
        B::Battery::refresh();
        snprintf(detail, sizeof(detail), "AXP2101: USB %s, LiPo %s", B::Battery::vbusPresent() ? "present" : "absent",
                 B::Battery::batteryConnected() ? "connected" : "not connected");
        report("power management", Result::Pass, detail);
    } else {
        report("power management", Result::Fail, "AXP2101 did not answer");
    }
#endif

    // Clock: two reads a little over a second apart must differ by 1 or 2 s.
    B::Rtc::DateTime a{}, b{};
    bool lostA = false, lostB = false;
    if (B::Rtc::begin() && B::Rtc::read(a, lostA)) {
        delay(1100);
        const bool again = B::Rtc::read(b, lostB);
        const int step = again ? (b.second - a.second + 60) % 60 : -1;
        snprintf(detail, sizeof(detail), "advanced %d s%s", step, lostB ? "; time not set since power loss" : "");
        report("real-time clock", step == 1 || step == 2 ? Result::Pass : Result::Fail, detail);
    } else {
        report("real-time clock", Result::Fail, "PCF85063 did not answer or returned invalid time");
    }

    B::Imu::Sample s{};
    if (B::Imu::begin()) {
        // The first samples after enabling read zero; wait for real data.
        bool got = false;
        float mag = 0;
        for (int tries = 0; tries < 12 && mag < 0.1f; ++tries) {
            delay(60);
            got = B::Imu::read(s);
            mag = sqrtf(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
        }
        snprintf(detail, sizeof(detail), "%.2f g total at rest", mag);
        report("IMU", got && mag > 0.8f && mag < 1.2f ? Result::Pass : Result::Fail, detail);
        B::Imu::end();
    } else {
        report("IMU", Result::Fail, "QMI8658 did not answer or identify");
    }

    float tempC = 0, humidity = 0;
    if (B::Env::begin() && B::Env::read(tempC, humidity)) {
        snprintf(detail, sizeof(detail), "%.1f C, %.1f %%", tempC, humidity);
        report("temperature/humidity", tempC > -20 && tempC < 70 && humidity >= 0 && humidity <= 100 ? Result::Pass : Result::Fail, detail);
    } else {
        report("temperature/humidity", Result::Fail, "SHTC3 did not answer or failed its checksum");
    }

    checkAudio();
    checkSd();

    // Optional GPS: only say whether a module answers on the bus.
    bool gps = false;
    {
        Esp32C5::I2cBus::Guard guard(100);
        if (guard) {
            Esp32C5::I2cBus::reopen();
            Wire.beginTransmission(0x50);
            gps = Wire.endTransmission() == 0;
        }
    }
    report("GPS module", gps ? Result::Pass : Result::Skip, gps ? "LC76G answers at 0x50" : "none wired");

    Serial.println(TAG "  ----  display                not checked: it cannot be read back");
    Serial.printf(TAG " %d passed, %d failed, %d skipped\n", passed, failed, skipped);
    Serial.printf(TAG " result: %s\n", failed ? "FAIL" : "PASS");
}

void loop() { delay(1000); }
