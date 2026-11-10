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

    // Recording: one mode at a time, amp held off, mic registers applied.
    CHECK(A::begin(), "playback configured before recording checks");
    CHECK(!A::beginRecording(), "recording refused while playback is configured");
    CHECK(A::end(), "playback shut down before recording");
    CHECK(!A::beginRecording(0xC8, A::MIC_PGA_MAX + 1) && !A::recording(), "out-of-range gain rejected at setup");
    g_wireFailReg = 0x17;
    CHECK(!A::beginRecording() && !A::recording(), "recording setup write failure rejected");
    g_wireFailReg = -1;
    CHECK(A::beginRecording() && A::recording() && !A::ready(), "recording setup succeeds");
    CHECK(!(g_wireRegisters[0x24][3] & 8), "amp stays off while recording");
    CHECK(g_wireRegisters[0x18][0x14] == 0x1A && g_wireRegisters[0x18][0x17] == 0xC8, "analog mic and ADC volume set");
    CHECK((g_wireRegisters[0x18][0x31] & 0x60) == 0x60 && g_wireRegisters[0x18][0x32] == 0x00, "DAC muted while recording");
    CHECK(!A::begin(), "playback refused while recording");
    CHECK(A::setMicGain(4) && g_wireRegisters[0x18][0x14] == 0x14, "mic gain step reaches codec");
    CHECK(!A::setMicGain(A::MIC_PGA_MAX + 1), "out-of-range mic gain rejected");
    g_takeResult = pdFALSE;
    CHECK(!A::endRecording() && A::recording(), "endRecording lock failure leaves state");
    g_takeResult = pdTRUE;
    CHECK(A::endRecording() && !A::recording(), "endRecording clears recording");
    CHECK(g_wireRegisters[0x18][0x14] == 0x00 && g_wireRegisters[0x18][0x17] == 0x00, "mic input off after endRecording");
    CHECK(A::begin() && g_wireRegisters[0x18][0x04] == 0x20 && g_wireRegisters[0x24][3] == 0x1B, "playback configuration unchanged after a recording session");
    CHECK(A::end(), "final shutdown");
    return g_failures ? 1 : 0;
}
