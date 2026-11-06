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

unsigned g_pinWriteCount[32] = {};
int g_pinLevel[32] = {};
void digitalWrite(int pin, int value) {
    if (pin >= 0 && pin < 32) { ++g_pinWriteCount[pin]; g_pinLevel[pin] = value; }
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
bool g_wireRegisterMode = false;
uint8_t g_wireRegisters[128][256] = {};
int g_wireFailReg = -1;
MockWireWrite g_wireWrites[256];
unsigned g_wireWritesLen = 0;
static uint8_t wireAddr, wireReg, wireValue;
static int wireBytes;
void MockWire::beginTransmission(uint8_t addr) { wireAddr = addr; wireBytes = 0; }
size_t MockWire::write(uint8_t b) {
    if (wireBytes == 0) wireReg = b;
    else if (wireBytes == 1) wireValue = b;
    ++wireBytes;
    ++g_wireWriteCount;
    return 1;
}
size_t   MockWire::write(const uint8_t*, size_t n) { g_wireWriteCount += (int)n; return n; }
uint8_t MockWire::endTransmission(bool) {
    if (wireBytes > 1 && g_wireWritesLen < 256)
        g_wireWrites[g_wireWritesLen++] = {wireAddr, wireReg, wireValue};
    if (g_wireRegisterMode && wireBytes > 1 && wireReg == g_wireFailReg) return 2;
    if (g_wireRegisterMode && wireBytes > 1 && !g_wireEndTransStatus)
        g_wireRegisters[wireAddr][wireReg] = wireValue;
    return g_wireEndTransStatus;
}
unsigned g_wireRequestCount = 0;
int MockWire::requestFrom(int addr, int) {
    ++g_wireRequestCount;
    g_wireReadPos = 0;
    if (g_wireRegisterMode) {
        g_wireReadBuf[0] = g_wireRegisters[addr][wireReg];
        g_wireReadLen = 1;
        return 1;
    }
    return g_wireReqReturn;
}
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
int g_giveCount  = 0;
SemaphoreHandle_t xSemaphoreCreateMutex()              { return (void*)1; }
int  xSemaphoreTake(SemaphoreHandle_t, TickType_t)     { return g_takeResult; }
void xSemaphoreGive(SemaphoreHandle_t)                 { g_giveCount++; }

// ---- XPowersLib stub state ----
int g_pmuBeginResult = 1;
int g_pmuReadCalls   = 0;
