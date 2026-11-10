// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>

namespace WsC5_35 {
namespace Audio {
// ES8311 codec at I2C 0x18 on the shared touch bus (SDA 27 / SCL 26), driving
// the onboard NS4150B mono amplifier and speaker on the Waveshare
// ESP32-C5-Touch-LCD-3.5.
constexpr uint8_t CODEC_ADDR = 0x18;
// I2S pins (MCU perspective), from the Waveshare 3.5" hardware reference and
// cross-checked against squachwatch-ws-c5 (docs/WAVESHARE-C5-35.md:
// "I2S BCK 23, WS 10, DOUT 25 (no MCLK)"). These are NOT the 2.8" board's pins
// (that board uses BCLK 24 / LRCK 25 / DOUT 26) — do not share pin constants.
constexpr int BCLK = 23;
constexpr int LRCK = 10;
constexpr int DOUT = 25;       // MCU -> codec DSDIN
constexpr int DIN = 24;        // codec ADC -> MCU; recording not implemented
constexpr int MCLK = -1;       // no MCLK net; codec derives its clock from BCLK
constexpr uint32_t SAMPLE_RATE = 16000;

// Codec control only: the caller owns I2S and board startup. Initialize the
// board expander/shared bus before starting I2S, then call begin(). The example
// uses Expander::begin(0), keeping the display backlight off. Omitting board
// startup produced silence in testing; the electrical cause is not isolated.
// No dedicated amplifier-enable control is implemented here.
//
// Clean speaker playback confirmed by the owner on 2026-11-10 using audio_35.
// Recording and pitch accuracy are unverified. Use one control task.
// Start I2S first: 16 kHz, signed 16-bit stereo, clock source BCLK, no MCLK.
// The DAC volume is a raw register value (0..255; higher is louder).
bool begin(uint8_t volume = 0xC0);
bool ready();
bool setVolume(uint8_t volume);
// Mute and zero DAC volume. On lock failure, returns false without changing
// ready(); after an attempted transaction, clears ready() even on I2C failure.
// The caller must separately stop I2S and handle a failed shutdown result.
bool end();
}  // namespace Audio
}  // namespace WsC5_35
