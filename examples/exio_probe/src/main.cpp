// SPDX-License-Identifier: MIT
// Diagnostic: reads by default; serial 0 explicitly writes documented PWM=0.
// Raw responses do not establish register width, reset coupling or optical state.
#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_random.h>
#include <esp_attr.h>
#include <stddef.h>
#include <string.h>

static constexpr char APP_TAG[] = "exio_probe_v5";
static constexpr uint8_t REGS[] = {2, 3, 4, 5, 6};
struct Sample {
    uint8_t address, reg, requested, raw[2];
    int pointerStatus, reported, stored;
    int64_t startUs, endUs;
};
struct Capture { Sample exio[5], pmu; };
static Capture early;
static int64_t setupUs;
static uint32_t bootId, sequence;
static esp_reset_reason_t reason;
static bool busReady;
struct SavedBoot {
    Capture capture;
    int64_t setup;
    uint32_t id;
    int reset;
};
struct History {
    uint32_t magic, checksum, count;
    SavedBoot boots[4];
};
// No flash writes. RTC retention across each reset type must be observed,
// not assumed. A checksum failure starts a new history instead of decoding RAM.
RTC_NOINIT_ATTR static History history;
static bool historyWasValid;
static uint32_t historyChecksum() {
    const auto* data = reinterpret_cast<const uint8_t*>(&history.count);
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < sizeof(History) - offsetof(History, count); ++i)
        h = (h ^ data[i]) * 16777619u;
    return h;
}
static void saveBoot() {
    historyWasValid = history.magic == 0x45584935u && history.count <= 4 &&
                      history.checksum == historyChecksum();
    if (!historyWasValid) memset(&history, 0, sizeof(history));
    if (history.count == 4) {
        memmove(&history.boots[0], &history.boots[1], 3 * sizeof(SavedBoot));
        history.count = 3;
    }
    SavedBoot& b = history.boots[history.count++];
    b.capture = early; b.setup = setupUs; b.id = bootId; b.reset = (int)reason;
    history.checksum = historyChecksum();
    history.magic = 0x45584935u;
}

static Sample readRaw(uint8_t address, uint8_t reg, uint8_t count) {
    Sample s{};
    s.address = address; s.reg = reg; s.requested = count;
    s.pointerStatus = -1; s.reported = -1;
    s.startUs = esp_timer_get_time();
    if (busReady) {
        Wire.beginTransmission(address);
        const size_t queued = Wire.write(reg);
        if (queued == 1) {
            s.pointerStatus = Wire.endTransmission(false);
            if (s.pointerStatus == 0) {
                s.reported = Wire.requestFrom((int)address, (int)count);
                while (Wire.available() && s.stored < 2) {
                    const int b = Wire.read();
                    if (b < 0) break;
                    s.raw[s.stored++] = (uint8_t)b;
                }
            }
        } else {
            Wire.endTransmission(true);
            s.pointerStatus = -2;
        }
    }
    s.endUs = esp_timer_get_time();
    return s;
}
static Capture sample() {
    Capture c{};
    for (unsigned i = 0; i < sizeof(REGS); ++i)
        c.exio[i] = readRaw(0x24, REGS[i], 2);
    c.pmu = readRaw(0x34, 0x00, 1); // report raw STATUS1; not proof of supply isolation
    return c;
}
static void printSample(const Sample& s, int64_t origin) {
    const bool valid = s.pointerStatus == 0 && s.reported == s.requested &&
                       s.stored == s.requested;
    Serial.printf(" addr=%02X reg=%02X requested=%u ptr_et=%d reported=%d stored=%d valid=%u"
                  " timer_start_us=%lld timer_end_us=%lld setup_delta_us=%lld raw=",
                  s.address, s.reg, s.requested, s.pointerStatus, s.reported,
                  s.stored, valid, (long long)s.startUs, (long long)s.endUs,
                  (long long)(s.startUs - origin));
    for (int i = 0; i < s.stored; ++i) Serial.printf("%02X", s.raw[i]);
    Serial.println();
}
static void printCapture(const char* label, const Capture& c, uint32_t id, int reset, int64_t origin) {
    Serial.printf("[%s] boot=%08lx seq=%lu sample=%s reset_reason=%d setup_timer_us=%lld\n",
                  APP_TAG, (unsigned long)id, (unsigned long)sequence,
                  label, reset, (long long)origin);
    for (const auto& s : c.exio) printSample(s, origin);
    printSample(c.pmu, origin);
}
static void printHistory() {
    Serial.printf("[%s] current_boot=%08lx history_valid_on_entry=%u history_count=%lu\n",
                  APP_TAG, (unsigned long)bootId, historyWasValid, (unsigned long)history.count);
    for (uint32_t i = 0; i < history.count; ++i) {
        const SavedBoot& b = history.boots[i];
        printCapture("EARLY_HISTORY", b.capture, b.id, b.reset, b.setup);
    }
}
void setup() {
    setupUs = esp_timer_get_time(); // ESP timer epoch, NOT ROM entry or power application
    reason = esp_reset_reason();
    busReady = Wire.begin(27, 26, 100000);
    Wire.setTimeOut(50);
    early = sample(); // before Serial setup/waits; replayed without resampling
    bootId = esp_random();
    saveBoot();
    Serial.begin(115200);
    printHistory();
}
void loop() {
    if (Serial.available()) {
        const int ch = Serial.read();
        if (ch == 'r') {
            Serial.printf("[%s] boot=%08lx requested=esp_restart\n", APP_TAG, (unsigned long)bootId);
            Serial.flush();
            esp_restart();
         } else if (ch == '0') {
            const int64_t start = esp_timer_get_time();
            int status = -1;
            if (busReady) {
                Wire.beginTransmission(0x24);
                Wire.write((uint8_t)0x05);
                Wire.write((uint8_t)0x00);
                status = Wire.endTransmission(true);
            }
            Serial.printf("[%s] CONTROL PWM_ZERO status=%d timer_start_us=%lld timer_end_us=%lld\n",
                          APP_TAG, status, (long long)start, (long long)esp_timer_get_time());
            const Sample one = readRaw(0x24, 0x05, 1);
            const Sample two = readRaw(0x24, 0x05, 2);
            printSample(one, setupUs);
            printSample(two, setupUs);
        } else if (ch == 'e') {
            printHistory();
        }
    }
    static uint32_t last;
    const uint32_t now = millis();
    if (now - last >= 2000) {
        last = now; ++sequence;
        printHistory();
        const Capture current = sample();
        printCapture("CURRENT", current, bootId, (int)reason, setupUs);
    }
    delay(5);
}
