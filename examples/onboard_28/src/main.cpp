// SPDX-License-Identifier: MIT
// Waveshare 2.8-inch onboard devices readout: real-time clock, 6-axis IMU,
// temperature/humidity sensor and the BOOT button, printed over serial once
// per second. Button presses and releases are printed as they happen. No
// display is rendered (backlight off).
//
// If the clock reports that it lost power, it is set to this firmware's build
// time so that it can be seen counting; that is not the real time.
//
// NOT yet hardware-verified on this board. The drivers are shared with the
// 3.5-inch board, where they are hardware-tested; this board carries the same
// parts at the same addresses on its own bus (SDA 0 / SCL 1). This board has
// no AXP2101, so no power-source report is printed.
#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <board/ws_c5_28/ws_c5_28_pins.h>
#include <board/ws_c5_28/ws_c5_28_expander.h>
#include <board/ws_c5_28/ws_c5_28_rtc.h>
#include <board/ws_c5_28/ws_c5_28_imu.h>
#include <board/ws_c5_28/ws_c5_28_env.h>

namespace Rtc = WsC5_28::Rtc;
namespace Imu = WsC5_28::Imu;
namespace Env = WsC5_28::Env;

static Rtc::DateTime buildTime() {
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char mon[4] = {};
    int day = 1, year = 2026, h = 0, m = 0, s = 0;
    sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &h, &m, &s);
    const char* at = strstr(months, mon);
    const int month = at ? (int)(at - months) / 3 + 1 : 1;
    // Sakamoto's method for the day of the week (0 = Sunday).
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = year - (month < 3);
    const int wd = (y + y / 4 - y / 100 + y / 400 + t[month - 1] + day) % 7;
    return {(uint16_t)year, (uint8_t)month, (uint8_t)day, (uint8_t)wd,
            (uint8_t)h, (uint8_t)m, (uint8_t)s};
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    // Expander::begin() opens the shared bus (SDA 0 / SCL 1) and keeps the
    // backlight off.
    if (!WsC5_28::Expander::begin(0)) Serial.println("[onboard_28] expander startup FAILED");
    pinMode(WsC5_28::BOOT_BUTTON, INPUT_PULLUP);

    Serial.printf("[onboard_28] RTC 0x51 %s\n", Rtc::begin() ? "found" : "NOT FOUND");
    Rtc::DateTime now{};
    bool lost = false;
    if (Rtc::present() && Rtc::read(now, lost)) {
        if (lost) {
            const bool set = Rtc::set(buildTime());
            Serial.printf("[onboard_28] RTC had lost power; set to build time: %s\n", set ? "ok" : "FAILED");
        } else {
            Serial.printf("[onboard_28] RTC kept its time: %04u-%02u-%02u %02u:%02u:%02u\n", now.year,
                          now.month, now.day, now.hour, now.minute, now.second);
        }
    }
    Serial.printf("[onboard_28] temperature/humidity sensor 0x70 %s\n", Env::begin() ? "found" : "NOT FOUND");
    Serial.printf("[onboard_28] IMU 0x6B %s\n", Imu::begin() ? "found, enabled" : "NOT FOUND");
}

void loop() {
    static bool wasDown = false;
    // First report one second after startup, once the IMU has produced data.
    static uint32_t lastPrint = millis();
    const bool down = digitalRead(WsC5_28::BOOT_BUTTON) == LOW;
    if (down != wasDown) {
        wasDown = down;
        Serial.printf("[onboard_28] BOOT button %s\n", down ? "PRESSED" : "released");
    }
    if (millis() - lastPrint >= 1000) {
        lastPrint = millis();
        Rtc::DateTime t{};
        bool lost = false;
        if (Rtc::read(t, lost))
            Serial.printf("[onboard_28] rtc %04u-%02u-%02u %02u:%02u:%02u%s\n", t.year, t.month,
                          t.day, t.hour, t.minute, t.second, lost ? " (integrity lost)" : "");
        else
            Serial.println("[onboard_28] rtc read failed");
        Imu::Sample s{};
        if (Imu::read(s))
            Serial.printf("[onboard_28] imu accel g %.2f %.2f %.2f |a| %.2f  gyro dps %.1f %.1f %.1f\n",
                          s.ax, s.ay, s.az, sqrtf(s.ax * s.ax + s.ay * s.ay + s.az * s.az),
                          s.gx, s.gy, s.gz);
        else
            Serial.println("[onboard_28] imu read failed");
        float tempC = 0, humidity = 0;
        if (Env::read(tempC, humidity))
            Serial.printf("[onboard_28] board temperature %.1f C, humidity %.1f %%\n", tempC, humidity);
        else
            Serial.println("[onboard_28] temperature/humidity read failed");
    }
    delay(10);
}
