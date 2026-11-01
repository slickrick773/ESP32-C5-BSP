// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include <board/ws_c5_28/ws_c5_28_audio.h>

namespace Audio28Example {
// Failure path only. The caller remains the sole I2S owner until this returns.
// Retry indefinitely if PA cannot be confirmed off; never resume tone playback.
// Templated only on the transport so host tests can exercise the actual helper.
template <class I2s>
bool shutdown(I2s& i2s) {
    static const int16_t silence[256 * 2] = {};
    bool reportedAmp = false;
    bool reportedWrite = false;
    while (!WsC5_28::Audio::end()) {
        if (!reportedAmp) {
            Serial.printf("[audio_28] amp-disable unconfirmed; retaining I2S and retrying with silence\n");
            reportedAmp = true;
        }
        if (i2s.write((const uint8_t*)silence, sizeof(silence)) != sizeof(silence) && !reportedWrite) {
            Serial.printf("[audio_28] silence write failed; amp/clock state cannot be guaranteed\n");
            reportedWrite = true;
        }
        // Yield even when both I2C and I2S fail immediately.
        delay(1);
    }
    // An acknowledged amp-disable write is required before stopping clocks.
    if (!i2s.end()) {
        Serial.printf("[audio_28] I2S shutdown failed (amp-disable acknowledged)\n");
        return false;
    }
    return true;
}
}  // namespace Audio28Example
