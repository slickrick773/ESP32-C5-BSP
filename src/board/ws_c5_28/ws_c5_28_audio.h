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
constexpr int DIN = 27;       // codec ADC -> MCU; see beginRecording()
constexpr int MCLK = -1;       // not wired; codec derives clock from BCLK
constexpr uint32_t SAMPLE_RATE = 16000;
constexpr uint8_t MIC_PGA_MAX = 10;   // ES8311 reg 0x14 PGA field maximum

// Playback hardware-verified: the owner heard audio_28 beeps, 2026-10-31,
// with volume 0xE0. Call from one control task.
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

// Microphone capture: onboard mic -> ES8311 ADC -> I2S DIN (GPIO 27). Same
// method and settings as the 3.5-inch driver, where it is hardware-tested.
// NOT yet hardware-verified on this board. The amplifier is held off for the
// whole capture. Start an I2S master with BCLK running (16 kHz, 16-bit, RX on
// DIN) first. One mode at a time: returns false while playback is configured.
bool beginRecording(uint8_t adcVolume = 0xC8, uint8_t pgaStep = MIC_PGA_MAX);
bool recording();
// Analog preamp gain, 0..MIC_PGA_MAX. Only valid while recording.
bool setMicGain(uint8_t pgaStep);
// Analog mic input off and ADC volume to minimum. The caller stops I2S.
bool endRecording();
}  // namespace Audio
}  // namespace WsC5_28
