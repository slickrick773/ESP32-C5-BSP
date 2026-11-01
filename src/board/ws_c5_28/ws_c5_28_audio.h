// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>

namespace WsC5_28 {
namespace Audio {
constexpr uint8_t CODEC_ADDR = 0x18;
constexpr uint8_t PA_CTRL = 3;  // EXIO3, NOT a GPIO
constexpr int BCLK = 24;
constexpr int LRCK = 25;
// MCU perspective: DOUT=26, DIN=27 (the initial bring-up list reversed them).
constexpr int DOUT = 26;       // MCU -> codec DSDIN
constexpr int DIN = 27;       // codec ADC -> MCU; recording not verified
constexpr int MCLK = -1;       // not wired; codec derives clock from BCLK
constexpr uint32_t SAMPLE_RATE = 16000;

// Playback hardware-verified: the owner heard audio_28 beeps, 2026-10-31,
// with volume 0xE0. Recording is not implemented. Call from one control task.
// Start the consumer's I2S master FIRST: standard I2S, 16 kHz, signed
// 16-bit stereo. No I2S library is owned by this driver. The audio_28 example
// uses Arduino ESP_I2S, matching the proven porkchop-ws-c5 path.
// Initialise before concurrent bus users: Expander::begin() is not locked.
// Requests PA off before configuring the codec; enables it only after the ID
// and configuration succeed. I2C failure cannot guarantee physical PA state.
// DAC volume is the raw register (0..255).
bool begin(uint8_t volume = 0xC0);
bool ready();
bool setVolume(uint8_t volume);
// Disable the amp before stopping the consumer's I2S clocks. Returns false
// if the lock/write fails; caller must not assume the amp is off then.
// Amp-only: does not mute or power down the codec. Success means the expander
// acknowledged the disable write, not independent electrical readback.
bool end();
}  // namespace Audio
}  // namespace WsC5_28
