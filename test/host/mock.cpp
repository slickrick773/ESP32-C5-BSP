// SPDX-License-Identifier: MIT
// Host-test mock implementations + the test-controlled state the mocks read.
#include <Arduino.h>
#include <Wire.h>
#include <freertos/semphr.h>
#include <string.h>

// ---- shared assert counter ----
int g_failures = 0;

// ---- Arduino pin/logic state ----
int      g_sdaMode              = SDA_HIGH;
int      g_sdaReleaseAtSclWrites = 0;
int      g_sclWriteCount        = 0;
int      g_sdaPin               = 27;
int      g_sclPin               = 26;
uint32_t g_millis               = 0;

void pinMode(int, int) {}
void delay(uint32_t) {}
void delayMicroseconds(uint32_t) {}
uint32_t millis() { return g_millis; }

int digitalRead(int pin) {
    if (pin == g_sdaPin) {
        switch (g_sdaMode) {
            case SDA_STUCK_LOW:     return LOW;
            case SDA_HIGH:          return HIGH;
            case SDA_RELEASE_AFTER: return (g_sclWriteCount >= g_sdaReleaseAtSclWrites) ? HIGH : LOW;
        }
    }
    return HIGH;
}

void digitalWrite(int pin, int) {
    if (pin == g_sclPin) g_sclWriteCount++;
}

int MockSerial::printf(const char*, ...) { return 0; }
MockSerial Serial;

// ---- Wire state ----
int     g_wireReqReturn      = 0;
uint8_t g_wireReadBuf[64]    = {0};
int     g_wireReadLen        = 0;
int     g_wireReadPos        = 0;
uint8_t g_wireEndTransStatus = 0;
int     g_wireBeginCount     = 0;
int     g_wireEndCount       = 0;
int     g_wireWriteCount     = 0;

void     MockWire::begin(int, int, uint32_t) { g_wireBeginCount++; }
void     MockWire::begin(int, int)           { g_wireBeginCount++; }
void     MockWire::begin()                   { g_wireBeginCount++; }
void     MockWire::end()                     { g_wireEndCount++; }
void     MockWire::setBufferSize(size_t)     {}
uint16_t MockWire::getTimeOut()              { return 50; }
void     MockWire::setTimeOut(uint16_t)      {}
void     MockWire::beginTransmission(uint8_t) {}
size_t   MockWire::write(uint8_t)            { g_wireWriteCount++; return 1; }
size_t   MockWire::write(const uint8_t*, size_t n) { g_wireWriteCount += (int)n; return n; }
uint8_t  MockWire::endTransmission(bool)     { return g_wireEndTransStatus; }
int      MockWire::requestFrom(int, int)     { g_wireReadPos = 0; return g_wireReqReturn; }
int      MockWire::available()               { return g_wireReadLen - g_wireReadPos; }
int      MockWire::read() {
    return (g_wireReadPos < g_wireReadLen) ? g_wireReadBuf[g_wireReadPos++] : -1;
}
MockWire Wire;

void mockWireSetResponse(int reqRet, const uint8_t* bytes, int n) {
    g_wireReqReturn = reqRet;
    if (n > (int)sizeof(g_wireReadBuf)) n = (int)sizeof(g_wireReadBuf);
    memcpy(g_wireReadBuf, bytes, (size_t)n);
    g_wireReadLen = n;
    g_wireReadPos = 0;
}

// ---- FreeRTOS semaphore ----
int g_takeResult = pdTRUE;
SemaphoreHandle_t xSemaphoreCreateMutex()              { return (void*)1; }
int  xSemaphoreTake(SemaphoreHandle_t, TickType_t)     { return g_takeResult; }
void xSemaphoreGive(SemaphoreHandle_t)                 {}

// ---- XPowersLib stub state ----
int g_pmuBeginResult = 1;
int g_pmuReadCalls   = 0;
