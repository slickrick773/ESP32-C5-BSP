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
constexpr int DIN = 24;        // codec ADC -> MCU; see beginRecording()
constexpr int MCLK = -1;       // no MCLK net; codec derives its clock from BCLK
constexpr uint32_t SAMPLE_RATE = 16000;
constexpr uint8_t MIC_PGA_MAX = 10;   // ES8311 reg 0x14 PGA field maximum

// Codec control only: the caller owns I2S and board startup. Initialize the
// board expander/shared bus before starting I2S, then call begin(). The example
// uses Expander::begin(0), keeping the display backlight off. Omitting board
// startup produced silence in testing; the electrical cause is not isolated.
// No dedicated amplifier-enable control is implemented here.
//
// Clean speaker playback confirmed by the owner on 2026-11-10 using audio_35.
// Pitch accuracy is unverified. Use one control task.
// Start I2S first: 16 kHz, signed 16-bit stereo, clock source BCLK, no MCLK.
// The DAC volume is a raw register value (0..255; higher is louder).
// Measured on hardware: 0xC0 is about 6x quieter than 0xE0, and PCM peak 12000
// at 0xE0 clips. Keep peak x gain under full scale for an undistorted tone.
// The ADC stays enabled in this mode: with I2S running in both directions the
// microphone arrives on the left slot and the codec's digital copy of the DAC
// input on the right slot (see the audio_selftest_35 example).
bool begin(uint8_t volume = 0xC0);
bool ready();
bool setVolume(uint8_t volume);
// Mute and zero DAC volume. On lock failure, returns false without changing
// ready(); after an attempted transaction, clears ready() even on I2C failure.
// The caller must separately stop I2S and handle a failed shutdown result.
bool end();

// Microphone capture: onboard mic -> ES8311 ADC -> I2S DIN (GPIO 24).
// Hardware-tested 2026-11-10 with mic_35 for level response only (low when
// silent, rising with sound, clipping on loud nearby sound; data on the left
// I2S slot), and one 3-second speech capture judged clear by the owner.
// Noise and frequency response are unmeasured. The ADC settings follow the
// 3.5-inch Bruce port (analog mic input 0x14 = 0x1A, ADC volume 0xC8, DAC muted).
// Same startup contract as begin(): board expander and shared bus first, then
// an I2S master with BCLK running (16 kHz, 16-bit, RX on DIN), then this call.
// One mode at a time: returns false while playback is configured.
bool beginRecording(uint8_t adcVolume = 0xC8, uint8_t pgaStep = MIC_PGA_MAX);
bool recording();
// Analog microphone preamp gain, 0..MIC_PGA_MAX (nominally 3 dB per step per
// the ES8311 datasheet). Only valid while recording. Out-of-range is rejected.
bool setMicGain(uint8_t pgaStep);
// Analog mic input off and ADC volume to minimum. Same result contract as
// end(). The caller separately stops I2S.
bool endRecording();
}  // namespace Audio
}  // namespace WsC5_35
