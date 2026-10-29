// SPDX-License-Identifier: MIT
// Host-test mock of the Arduino core surface used by the BSP. Only what the
// drivers under test reference. Pin/logic state is test-controlled (see mock.cpp).
#pragma once
#include <stdint.h>
#include <stddef.h>

#define HIGH 1
#define LOW  0
#define INPUT             0x0
#define OUTPUT            0x1
#define INPUT_PULLUP      0x2
#define OUTPUT_OPEN_DRAIN 0x3

// Test-controlled hardware state (defined in mock.cpp).
enum SdaMode { SDA_STUCK_LOW = 0, SDA_HIGH = 1, SDA_RELEASE_AFTER = 2 };
extern int      g_sdaMode;                // one of SdaMode
extern int      g_sdaReleaseAtSclWrites;  // SDA_RELEASE_AFTER: HIGH once SCL writes >= this
extern int      g_sclWriteCount;          // incremented on each digitalWrite(SCL)
extern int      g_sdaPin;                 // pin treated as SDA (default 27)
extern int      g_sclPin;                 // pin treated as SCL (default 26)
extern uint32_t g_millis;

void     pinMode(int pin, int mode);
int      digitalRead(int pin);
void     digitalWrite(int pin, int val);
void     delay(uint32_t ms);
void     delayMicroseconds(uint32_t us);
uint32_t millis();

struct MockSerial { int printf(const char* fmt, ...); };
extern MockSerial Serial;
