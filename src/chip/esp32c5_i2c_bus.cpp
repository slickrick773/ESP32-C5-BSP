// SPDX-License-Identifier: MIT
// See esp32c5_i2c_bus.h. The reopen quirk and timeout-restore are verified on
// hardware; the lock mirrors the fleet's proven shared-bus mutex.
#include "esp32c5_i2c_bus.h"
#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace Esp32C5 {
namespace I2cBus {

static int      s_sda = -1, s_scl = -1;
static uint32_t s_hz  = 100000;
static bool     s_up  = false;
static SemaphoreHandle_t s_mux = nullptr;

void begin(int sda, int scl, uint32_t hz) {
    s_sda = sda; s_scl = scl; s_hz = hz;
    if (!s_mux) s_mux = xSemaphoreCreateMutex();
    Wire.begin(sda, scl, hz);
    s_up = true;
}

void reopen() {
    if (!s_up) return;
    Wire.end();
    Wire.begin(s_sda, s_scl, s_hz);
}

bool recover() {
    if (!s_up) return false;
    // If SDA is high the bus is not wedged by a stuck slave; nothing to do.
    pinMode(s_sda, INPUT_PULLUP);
    pinMode(s_scl, INPUT_PULLUP);
    if (digitalRead(s_sda) == HIGH) { reopen(); return false; }

    // Pulse SCL up to 9 times so a mid-byte slave can finish and release SDA.
    pinMode(s_scl, OUTPUT_OPEN_DRAIN);
    for (int i = 0; i < 9 && digitalRead(s_sda) == LOW; i++) {
        digitalWrite(s_scl, LOW);  delayMicroseconds(5);
        digitalWrite(s_scl, HIGH); delayMicroseconds(5);
    }
    reopen();
    return true;
}

bool lock(uint32_t timeoutMs) {
    if (!s_mux) return true;   // not initialised: single owner, nothing to serialise
    const TickType_t ticks = (timeoutMs == UINT32_MAX) ? portMAX_DELAY
                                                       : pdMS_TO_TICKS(timeoutMs);
    return xSemaphoreTake(s_mux, ticks) == pdTRUE;
}

void unlock() {
    if (s_mux) xSemaphoreGive(s_mux);
}

TimeoutGuard::TimeoutGuard(uint16_t timeoutMs) : saved_(Wire.getTimeOut()) {
    Wire.setTimeOut(timeoutMs);
}
TimeoutGuard::~TimeoutGuard() {
    Wire.setTimeOut(saved_);
}

}  // namespace I2cBus
}  // namespace Esp32C5
