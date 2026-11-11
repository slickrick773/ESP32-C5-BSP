// SPDX-License-Identifier: MIT
// Host tests for the 3.5-inch speaker-amplifier, PWR-button, header-line and
// charge-current calls. Compiles against the REAL expander + battery drivers.
#include <cstdio>
#include <Arduino.h>
#include <Wire.h>
#include <XPowersLib.h>
#include <freertos/semphr.h>
#include "testutil.h"
#include "../../src/chip/esp32c5_i2c_bus.h"
#include "../../src/board/ws_c5_35/ws_c5_35_pins.h"
#include "../../src/board/ws_c5_35/ws_c5_35_battery.h"
#include "../../src/board/ws_c5_35/ws_c5_35_expander.h"

namespace Ex = WsC5_35::Expander;
namespace Bat = WsC5_35::Battery;
static const MockWireWrite& last() { return g_wireWrites[g_wireWritesLen - 1]; }

int main() {
    g_failures = 0;
    g_wireRegisterMode = true;
    g_takeResult = pdTRUE;
    bool b = false;

    std::printf("Calls refuse to run before begin():\n");
    CHECK(!Ex::setSpeakerAmp(false) && !Ex::powerButton(b) && !Ex::headerMode(8, true) &&
          !Ex::headerWrite(8, true) && !Ex::headerRead(8, b), "all expander calls return false");

    CHECK(Ex::begin(0), "expander begin succeeds");
    const unsigned afterBegin = g_wireWritesLen;

    std::printf("Speaker amplifier:\n");
    CHECK(Ex::setSpeakerAmp(false), "amp off succeeds");
    CHECK(last().reg == 0x03 && last().value == 0x03 && last().bytes == 2, "amp off clears only EXIO5, one data byte");
    CHECK(Ex::setSpeakerAmp(true) && last().value == 0x23, "amp on restores EXIO5 with resets still high");
    g_takeResult = pdFALSE;
    const unsigned n = g_wireWritesLen;
    CHECK(!Ex::setSpeakerAmp(false) && g_wireWritesLen == n, "no bus write when the lock fails");
    g_takeResult = pdTRUE;
    g_wireEndTransStatus = 2;
    CHECK(!Ex::setSpeakerAmp(false), "a bus failure reports false");
    g_wireEndTransStatus = 0;
    CHECK(Ex::setSpeakerAmp(true) && last().value == 0x23, "state is intact after the failed write");

    std::printf("PWR button:\n");
    g_wireRegisters[0x24][0x04] = 0x77; g_wireRegisters[0x24][0x05] = 0x00;
    CHECK(Ex::powerButton(b) && b, "bit 6 high reads as pressed");
    g_wireRegisters[0x24][0x04] = 0x37;
    CHECK(Ex::powerButton(b) && !b, "bit 6 low reads as released");
    g_wireShortRead = 1; b = true;
    CHECK(!Ex::powerButton(b) && b, "a short read fails and leaves the result alone");
    g_wireShortRead = 0;
    g_wireRegisters[0x24][0x05] = 0x00;

    std::printf("Header lines:\n");
    CHECK(!Ex::headerMode(7, true) && !Ex::headerMode(16, true), "lines outside E8..E15 are rejected");
    CHECK(!Ex::headerWrite(8, true), "writing a line that is still an input is rejected");
    CHECK(Ex::headerMode(8, true), "E8 becomes an output");
    CHECK(last().reg == 0x02 && last().value == 0x23 && last().value2 == 0x01 && last().bytes == 3,
          "mode goes out as two bytes with the start-up lines kept as outputs");
    CHECK(Ex::headerWrite(8, true) && last().reg == 0x03 && last().value == 0x23 && last().value2 == 0x01,
          "E8 high keeps the low byte");
    CHECK(Ex::setSpeakerAmp(false) && last().bytes == 3 && last().value == 0x03 && last().value2 == 0x01,
          "amp off now sends both bytes and keeps E8 high");
    CHECK(Ex::setSpeakerAmp(true), "amp back on");
    g_wireRegisters[0x24][0x04] = 0x37; g_wireRegisters[0x24][0x05] = 0x02;
    CHECK(Ex::headerRead(9, b) && b, "E9 reads high from the second input byte");
    CHECK(Ex::headerRead(10, b) && !b, "E10 reads low");
    g_wireRegisters[0x24][0x05] = 0x00;
    CHECK(Ex::headerMode(8, false) && last().reg == 0x02 && last().value2 == 0x00, "E8 back to input");
    CHECK(g_wireWritesLen > afterBegin, "writes were recorded");

    std::printf("Charge current:\n");
    uint16_t mA = 7;
    CHECK(!Bat::setChargeCurrentMa(400) && !Bat::chargeCurrentMa(mA) && mA == 7, "refused before Battery::begin()");
    g_pmuBeginResult = 1;
    CHECK(Bat::begin(), "battery begin succeeds");
    CHECK(Bat::chargeCurrentMa(mA) && mA == 200, "code 8 reads as 200 mA");
    CHECK(Bat::setChargeCurrentMa(400) && g_pmuChargeCode == 10, "400 mA is code 10");
    CHECK(Bat::setChargeCurrentMa(1000) && g_pmuChargeCode == 16, "1000 mA is code 16");
    CHECK(Bat::setChargeCurrentMa(125) && g_pmuChargeCode == 5, "125 mA is code 5");
    CHECK(Bat::setChargeCurrentMa(0) && g_pmuChargeCode == 0, "0 mA is code 0");
    CHECK(!Bat::setChargeCurrentMa(50) && !Bat::setChargeCurrentMa(250) && !Bat::setChargeCurrentMa(1100) &&
          !Bat::setChargeCurrentMa(410) && g_pmuChargeCode == 0, "values off the chip's steps are rejected unchanged");
    g_pmuChargeWriteOk = false;
    CHECK(!Bat::setChargeCurrentMa(300) && g_pmuChargeCode == 0, "a failed write reports false");
    g_pmuChargeWriteOk = true;
    g_takeResult = pdFALSE;
    CHECK(!Bat::setChargeCurrentMa(300) && g_pmuChargeCode == 0, "no write when the lock fails");
    g_takeResult = pdTRUE;
    g_pmuChargeCode = 20; mA = 7;
    CHECK(!Bat::chargeCurrentMa(mA) && mA == 7, "an unknown code is reported as a failure");

    std::printf(g_failures ? "FAILED (%d)\n" : "all passed\n", g_failures);
    return g_failures;
}
