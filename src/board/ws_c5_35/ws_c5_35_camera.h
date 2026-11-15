// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-3.5 — camera control (BF3901 at I2C 0x6E).
// Only boards sold with the camera have it; on the others begin() returns
// false.
//
// Control (identity, registers, start-up settings) is over I2C. Pictures
// arrive on two data lines with a clock and a frame signal, received by the
// ESP32-C5 parallel-input peripheral: 240x320 pixels, RGB565. The camera's
// clock is an oscillator on the board and its reset is a resistor and
// capacitor, so there is nothing to switch on before talking to it.
//
// Initialize the shared bus (Esp32C5::I2cBus::begin) first. Every call takes
// the bus lock and returns false without touching the bus if it is not free.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace WsC5_35 {
namespace Camera {

constexpr uint8_t  REG_ID_HIGH = 0xFC;
constexpr uint8_t  REG_ID_LOW  = 0xFD;
constexpr uint16_t CHIP_ID     = 0x3901;

struct RegValue { uint8_t reg, value; };

// Read the identity registers. Returns true if they read 0x3901.
bool begin();
bool present();

// Single register access. Both need begin() to have succeeded.
bool readReg(uint8_t reg, uint8_t& value);
bool writeReg(uint8_t reg, uint8_t value);
// Write a table in order under one bus lock; stops at the first failure. An
// entry whose register is REG_DELAY pauses for `value` milliseconds instead.
constexpr uint8_t REG_DELAY = 0xFE;
bool writeRegs(const RegValue* table, size_t count);

// ---- Pictures ---------------------------------------------------------------
constexpr int    WIDTH  = 240;
constexpr int    HEIGHT = 320;
// What the camera sends per picture: a 4-byte picture header, then for each
// line a 6-byte line header and the line's pixels.
constexpr size_t FRAME_HEADER_BYTES = 4;
constexpr size_t LINE_HEADER_BYTES  = 6;
constexpr size_t RAW_FRAME_BYTES = FRAME_HEADER_BYTES + (size_t)HEIGHT * (LINE_HEADER_BYTES + WIDTH * 2);
constexpr size_t FRAME_BYTES     = (size_t)WIDTH * HEIGHT * 2;

// Load the start-up settings (two data lines, 240x320 RGB565). The camera is
// left idle; call stream(true) to start it sending.
bool configure();
bool stream(bool on);
// Picture orientation, after configure(). The start-up settings have both
// on; turning both off turns the picture by 180 degrees.
bool setMirrorFlip(bool mirror, bool flip);

// Check the headers of a received picture and pack its pixels into `out`
// (FRAME_BYTES, two bytes per pixel, low byte first). `out` may be the same
// buffer as `raw`. Returns false, with `out` possibly part-written, if the
// length or any header is wrong.
bool decodeFrame(const uint8_t* raw, size_t rawBytes, uint8_t* out);

// Receiving (ws_c5_35_camera_capture.cpp). captureBegin() claims the
// parallel-input peripheral, GPIO 0, 1, 11 and 12, and a receive buffer of
// RAW_FRAME_BYTES. captureFrame() waits for one whole picture and decodes it
// into `out` (FRAME_BYTES); it returns false on a timeout or when no valid
// picture arrived. captureEnd() releases everything.
bool captureBegin();
bool captureFrame(uint8_t* out, uint32_t timeoutMs);
void captureEnd();
// Bytes received and pictures rejected so far, for diagnosis.
struct CaptureStats { uint32_t received, badLength, badHeader, lastBytes; uint8_t lastHead[8]; };
CaptureStats captureStats();

}  // namespace Camera
}  // namespace WsC5_35
