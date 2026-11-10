// SPDX-License-Identifier: Apache-2.0
// ES8311 sequence adapted from porkchop-ws-c5/PorkChop_CYD/es8311.h
// (Apache-2.0), itself ported from Espressif esp_codec_dev es8311.c, and
// cross-checked against squachwatch-ws-c5/src/sound.cpp's WS_C5_35 codecInit
// (same registers: clock from BCLK, 16 kHz, 16-bit I2S, DAC on). The register
// writes are codec-level and board-independent; the 3.5"-specific facts live
// in ws_c5_35_audio.h (I2S pins, and the amp having no software enable).
// Fixed 16 kHz / 16-bit / DAC / slave / BCLK clock source, no MCLK.
#include "ws_c5_35_audio.h"
#include "../../chip/esp32c5_i2c_bus.h"
#include <Wire.h>

namespace WsC5_35 {
namespace Audio {
static bool s_ready = false;
static bool writeReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(CODEC_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}
static bool readReg(uint8_t reg, uint8_t& value) {
    Wire.beginTransmission(CODEC_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)CODEC_ADDR, 1) != 1 || Wire.available() < 1)
        return false;
    value = (uint8_t)Wire.read();
    return true;
}
static bool configure(uint8_t volume) {
    Esp32C5::I2cBus::Guard guard(100);
    if (!guard) return false;
    Esp32C5::I2cBus::reopen();  // C5 read quirk; once per locked transaction
    bool ok = true;
    auto wr = [&](uint8_t reg, uint8_t value) {
        if (ok) ok = writeReg(reg, value);
    };
    auto rd = [&](uint8_t reg) -> uint8_t {
        uint8_t value = 0;
        if (ok) ok = readReg(reg, value);
        return value;
    };
    // Reject missing/wrong codecs before changing their configuration.
    const uint8_t id1 = rd(0xFD), id2 = rd(0xFE);
    if (!ok || id1 != 0x83 || id2 != 0x11) return false;
    uint8_t v;
    // ── open ───────────────────────────────────────────────────────────────
    if (rd(0x0D) != 0xFA) wr(0x0D, 0xFA);
    wr(0x44, 0x08); wr(0x44, 0x08);   // I2C noise immunity (x2 per vendor)
    wr(0x01, 0x30); wr(0x02, 0x00); wr(0x03, 0x10);
    wr(0x16, 0x24); wr(0x04, 0x10); wr(0x05, 0x00);
    wr(0x0B, 0x00); wr(0x0C, 0x00); wr(0x10, 0x1F);
    wr(0x11, 0x7F); wr(0x00, 0x80);
    v = rd(0x00); v &= 0xBF; wr(0x00, v);     // slave mode
    wr(0x01, 0xBF);                                  // 0x3F|0x80: clock from SCLK, no invert
    v = rd(0x06); v &= ~0x20; wr(0x06, v);    // SCLK not inverted
    wr(0x13, 0x10); wr(0x1B, 0x0A); wr(0x1C, 0x6A);
    wr(0x44, 0x58);                                  // internal reference (ADCL+DACR)

    // ── config_sample: 16 kHz coeff {4096000,16000}, use_mclk=false → datmp=3 ──
    v = rd(0x02); v &= 0x07; v |= (0 << 5); v |= (3 << 3); wr(0x02, v);
    wr(0x05, 0x00);                                  // adc_div/dac_div = 1
    v = rd(0x03); v &= 0x80; v |= 0x10; wr(0x03, v);   // fs_mode 0, adc_osr 0x10
    v = rd(0x04); v &= 0x80; v |= 0x10; wr(0x04, v);   // Matches the working 3.5-inch reference and tested BSP configuration.
    v = rd(0x07); v &= 0xC0; v |= 0x00; wr(0x07, v);   // lrck_h
    wr(0x08, 0xFF);                                            // lrck_l
    v = rd(0x06); v &= 0xE0; v |= (4 - 1); wr(0x06, v); // bclk_div 4

    // ── 16-bit + I2S normal format ─────────────────────────────────────────
    v = rd(0x09); v |= 0x0C; v &= 0xFC; wr(0x09, v);
    v = rd(0x0A); v |= 0x0C; v &= 0xFC; wr(0x0A, v);

    // ── start: power up DAC ────────────────────────────────────────────────
    wr(0x00, 0x80); wr(0x01, 0xBF);
    v = rd(0x09); v &= ~0x40; wr(0x09, v);    // enable DAC serial-in path
    wr(0x17, 0xBF);
    wr(0x0E, 0x02);
    wr(0x12, 0x00);                                  // power up DAC
    wr(0x14, 0x1A);
    v = rd(0x14); v &= ~0x40; wr(0x14, v);    // no digital mic
    wr(0x0D, 0x01);
    wr(0x15, 0x40);
    wr(0x37, 0x08);
    wr(0x45, 0x00);

    // ── volume + unmute ────────────────────────────────────────────────────
    wr(0x32, volume);                                  // DAC volume (0x00..0xFF, higher=louder)
    v = rd(0x31); v &= 0x9F; wr(0x31, v);     // unmute DAC

    return ok;
}
bool ready() { return s_ready; }
bool begin(uint8_t volume) {
    if (s_ready) return true;
    // The caller must initialize board power/reset and the shared I2C bus.
    // The I2S master must already be running (codec clocks off BCLK).
    s_ready = configure(volume);
    return s_ready;
}
bool setVolume(uint8_t volume) {
    if (!s_ready) return false;
    Esp32C5::I2cBus::Guard guard(100);
    return guard && writeReg(0x32, volume);
}
bool end() {
    // Quiet the codec DAC. The I2S owner separately stops its transport.
    Esp32C5::I2cBus::Guard guard(100);
    if (!guard) return false;
    Esp32C5::I2cBus::reopen();
    uint8_t v = 0;
    bool ok = readReg(0x31, v);
    if (ok) { v |= 0x60; ok = writeReg(0x31, v); }  // mute DAC (L+R)
    if (ok) ok = writeReg(0x32, 0x00);              // DAC volume to zero
    s_ready = false;
    return ok;
}
}  // namespace Audio
}  // namespace WsC5_35
