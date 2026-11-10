// SPDX-License-Identifier: MIT
// Host test for the 3.5-inch ES8311 driver. Unlike the 2.8-inch board, this
// board has no amp-enable pin, so the driver owns only the codec (no expander).
#include <cstdio>
#include <Wire.h>
#include <freertos/semphr.h>
#include "testutil.h"
#include "../../src/board/ws_c5_35/ws_c5_35_audio.h"
#include "../../src/chip/esp32c5_i2c_bus.h"
namespace A = WsC5_35::Audio;
int main() {
    // Create the shared-bus mutex so lock() actually serialises (otherwise an
    // uninitialised bus reports a single owner and never contends).
    Esp32C5::I2cBus::begin(27, 26, 100000);
    g_wireRegisterMode = true;

    // Wrong codec ID is rejected before any configuration.
    CHECK(!A::begin(), "wrong codec ID rejected");
    CHECK(!A::ready(), "driver not ready on wrong ID");

    // A short codec read is rejected (no register-backed reply).
    g_wireRegisterMode = false;
    g_wireReqReturn = 0;
    g_wireReadLen = 0;
    CHECK(!A::begin(), "short codec read rejected");
    CHECK(!A::ready(), "short read does not mark codec ready");

    // Present the correct ES8311 ID at 0x18.
    g_wireRegisterMode = true;
    g_wireRegisters[0x18][0xFD] = 0x83;
    g_wireRegisters[0x18][0xFE] = 0x11;

    // A failed configuration write (DAC volume register) is rejected.
    g_wireFailReg = 0x32;
    CHECK(!A::begin(), "codec configuration write failure rejected");
    CHECK(!A::ready(), "configuration failure does not mark ready");
    g_wireFailReg = -1;

    // A lock failure is rejected without touching the codec.
    g_takeResult = pdFALSE;
    CHECK(!A::begin(), "begin reports lock failure");
    g_takeResult = pdTRUE;

    // Successful setup.
    CHECK(A::begin(), "codec setup succeeds");
    CHECK(A::ready(), "driver ready after setup");
    CHECK(g_wireRegisters[0x18][0x01] == 0xBF, "codec clocks from BCLK (no MCLK)");
    CHECK(g_wireRegisters[0x18][0x04] == 0x10, "DAC oversampling preserves the hardware-tested 3.5-inch configuration");
    CHECK(g_wireRegisters[0x18][0x09] == 0x0C, "serial input is 16-bit I2S");
    CHECK(g_wireRegisters[0x18][0x32] == 0xC0, "default DAC volume reaches codec");
    CHECK((g_wireRegisters[0x18][0x31] & 0x60) == 0, "DAC is unmuted after setup");

    // Volume updates reach the codec.
    CHECK(A::setVolume(0x90), "volume write succeeds");
    CHECK(g_wireRegisters[0x18][0x32] == 0x90, "volume reaches codec");

    // Shutdown mutes and zeros the DAC (no amp pin to cut) and clears ready.
    g_takeResult = pdFALSE;
    CHECK(!A::end(), "shutdown reports lock failure");
    g_takeResult = pdTRUE;
    CHECK(A::end() && !A::ready(), "shutdown mutes codec and clears ready");
    CHECK((g_wireRegisters[0x18][0x31] & 0x60) == 0x60, "shutdown mutes the DAC");
    CHECK(g_wireRegisters[0x18][0x32] == 0x00, "shutdown zeros DAC volume");
    CHECK(!A::setVolume(0xFF), "volume rejected after shutdown");
    CHECK(A::begin(), "codec can be initialized again after shutdown");
    g_wireFailReg = 0x32;
    CHECK(!A::end(), "shutdown reports failed volume write");
    CHECK(!A::ready(), "failed shutdown transaction clears ready");
    CHECK(!A::setVolume(0xFF), "volume rejected after failed shutdown");
    g_wireFailReg = -1;

    // Recording: refused while playback is configured (one mode at a time).
    CHECK(A::begin(), "playback configured before recording checks");
    CHECK(!A::beginRecording(), "recording refused while playback is configured");
    CHECK(A::end(), "playback shut down before recording");

    // Recording setup: DAC muted, analog mic on, ADC volume applied.
    g_wireFailReg = 0x17;
    CHECK(!A::beginRecording(), "recording setup write failure rejected");
    CHECK(!A::recording(), "failed recording setup does not mark recording");
    g_wireFailReg = -1;
    g_takeResult = pdFALSE;
    CHECK(!A::beginRecording(), "beginRecording reports lock failure");
    g_takeResult = pdTRUE;
    CHECK(A::beginRecording(), "recording setup succeeds");
    CHECK(A::recording() && !A::ready(), "recording mode set, playback not ready");
    CHECK(g_wireRegisters[0x18][0x01] == 0xBF, "recording clocks from BCLK (no MCLK)");
    CHECK(g_wireRegisters[0x18][0x0A] == 0x0C, "serial output is 16-bit I2S");
    CHECK(g_wireRegisters[0x18][0x14] == 0x1A, "analog mic input selected");
    CHECK(g_wireRegisters[0x18][0x17] == 0xC8, "default ADC volume reaches codec");
    CHECK((g_wireRegisters[0x18][0x31] & 0x60) == 0x60, "DAC muted while recording");
    CHECK(g_wireRegisters[0x18][0x32] == 0x00, "DAC volume zero while recording");
    CHECK(!A::begin(), "playback refused while recording");
    CHECK(A::setMicGain(4) && g_wireRegisters[0x18][0x14] == 0x14, "mic gain step reaches codec");
    CHECK(!A::setMicGain(A::MIC_PGA_MAX + 1) && g_wireRegisters[0x18][0x14] == 0x14, "out-of-range mic gain rejected");

    // Recording shutdown.
    g_takeResult = pdFALSE;
    CHECK(!A::endRecording() && A::recording(), "endRecording lock failure leaves state");
    g_takeResult = pdTRUE;
    CHECK(A::endRecording() && !A::recording(), "endRecording clears recording");
    CHECK(g_wireRegisters[0x18][0x14] == 0x00, "mic input off after endRecording");
    CHECK(g_wireRegisters[0x18][0x17] == 0x00, "ADC volume minimum after endRecording");
    CHECK(!A::setMicGain(4), "mic gain rejected when not recording");
    CHECK(!A::beginRecording(0xC8, A::MIC_PGA_MAX + 1) && !A::recording(), "out-of-range gain rejected at setup");
    CHECK(A::begin() && g_wireRegisters[0x18][0x04] == 0x10 && g_wireRegisters[0x18][0x32] == 0xC0,
          "playback configuration unchanged after a recording session");
    CHECK(A::end(), "final shutdown");

    return g_failures ? 1 : 0;
}
