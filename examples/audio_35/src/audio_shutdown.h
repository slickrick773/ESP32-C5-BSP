// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include <board/ws_c5_35/ws_c5_35_audio.h>

namespace Audio35Example {
// Used after normal completion and failures. The caller owns I2S until this
// returns. Attempt codec muting and transport shutdown independently; neither
// software result alone establishes the physical amplifier state.
template <class I2s>
bool shutdown(I2s& i2s) {
    // Attempt I2S shutdown even if codec muting fails.
    if (!WsC5_35::Audio::end())
        Serial.println("[audio_35] codec mute unconfirmed; stopping I2S clocks to silence the amp");
    if (!i2s.end()) {
        Serial.println("[audio_35] I2S shutdown failed; amp/clock state cannot be guaranteed");
        return false;
    }
    return true;
}
}  // namespace Audio35Example
