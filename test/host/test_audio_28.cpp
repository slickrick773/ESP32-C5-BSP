// SPDX-License-Identifier: MIT
#include <cstdio>
#include <Wire.h>
#include <freertos/semphr.h>
#include "testutil.h"
#include "../../src/board/ws_c5_28/ws_c5_28_audio.h"
#include "../../src/board/ws_c5_28/ws_c5_28_expander.h"
namespace A = WsC5_28::Audio;
namespace E = WsC5_28::Expander;
int main() {
    g_wireRegisterMode = true;
    CHECK(E::begin(), "expander initialises");
    CHECK(g_wireRegisters[0x24][3] == 3, "both reset lines released");
    CHECK(!E::setOutput(8, true), "invalid output rejected");
    g_takeResult = pdFALSE;
    const int before = g_wireWriteCount;
    CHECK(!E::setOutput(3, true), "output rejects lock failure");
    CHECK(g_wireWriteCount == before, "lock failure performs no bus write");
    g_takeResult = pdTRUE;
    g_wireEndTransStatus = 2;
    CHECK(!E::setOutput(3, true), "NACK is reported");
    g_wireEndTransStatus = 0;
    CHECK(E::setOutput(4, true), "another output can be set after NACK");
    CHECK(g_wireRegisters[0x24][3] == 0x13, "failed amp bit is not cached; resets preserved");
    CHECK(!A::begin(), "wrong codec ID rejected");
    CHECK(!A::ready() && !(g_wireRegisters[0x24][3] & 8), "amp stays off on wrong ID");
    g_wireRegisterMode = false;
    g_wireReqReturn = 0;
    g_wireReadLen = 0;
    CHECK(!A::begin(), "short codec read rejected");
    CHECK(!A::ready(), "short read does not mark codec ready");
    g_wireRegisterMode = true;
    g_wireRegisters[0x18][0xFD] = 0x83;
    g_wireRegisters[0x18][0xFE] = 0x11;
    g_wireFailReg = 0x32;
    CHECK(!A::begin(), "codec configuration write failure rejected");
    CHECK(!(g_wireRegisters[0x24][3] & 8), "amp stays off on configuration failure");
    g_wireFailReg = -1;
    CHECK(A::begin(), "codec setup succeeds after retry");
    CHECK(g_wireRegisters[0x24][3] == 0x1B, "amp enable preserves resets and unrelated output");
    CHECK(g_wireRegisters[0x18][1] == 0xBF, "codec uses BCLK without MCLK");
    CHECK(g_wireRegisters[0x18][0x04] == 0x20, "16 kHz DAC oversampling matches Espressif coefficient row");
    CHECK(A::setVolume(0x90), "volume write succeeds");
    CHECK(g_wireRegisters[0x18][0x32] == 0x90, "volume reaches codec");
    g_takeResult = pdFALSE;
    CHECK(!A::end(), "shutdown reports lock failure");
    g_takeResult = pdTRUE;
    CHECK(A::end() && !A::ready(), "shutdown disables amp and clears ready");
    CHECK(g_wireRegisters[0x24][3] == 0x13, "shutdown preserves other outputs");
    CHECK(!A::setVolume(0xFF), "volume rejected after shutdown");
    return g_failures ? 1 : 0;
}
