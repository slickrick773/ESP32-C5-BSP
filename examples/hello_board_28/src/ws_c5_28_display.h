// SPDX-License-Identifier: MIT
// ST7789 init sequence for the Waveshare ESP32-C5-Touch-LCD-2.8, in Arduino_GFX
// opcode form. This is the DRIVER-CODED form of the register table documented
// (driver-neutrally) in src/board/ws_c5_28/ws_c5_28_pins.h — it lives here, in
// the example, because the opcodes (BEGIN_WRITE / WRITE_COMMAND_8 / …) are
// Arduino_GFX's, and the BSP's src/ stays free of any one graphics library.
//
// Copied verbatim from Rick's working umbravox 2.8" port
// (device/ws_c5_28_display.h), which is hardware-accepted on the physical 2.8"
// board. Upstream attribution: Waveshare ESP32-C5-Touch-LCD-2.8 v1.2.0
// examples/arduino/06_lvgl_demo/Display_ST7789.cpp (77a7dce). Arduino_GFX
// handles rotation/address windows; this retains the panel-specific
// power/gamma and — critically — the MSB-first RAMCTRL byte-order fix.
#pragma once

#include <Arduino_GFX_Library.h>

static const uint8_t ws28Init[] = {
    BEGIN_WRITE,
    END_WRITE, DELAY, 120, BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29,
    END_WRITE, DELAY, 120, BEGIN_WRITE,
    WRITE_COMMAND_8, 0x11,
    END_WRITE, DELAY, 120, BEGIN_WRITE,
    WRITE_COMMAND_8, 0x36,
    WRITE_DATA_8, 0x00,
    WRITE_COMMAND_8, 0x3A,
    WRITE_DATA_8, 0x05,
    WRITE_COMMAND_8, 0xB0,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0xE0, // RAMCTRL: MSB first for Arduino_GFX (vendor raw buffers use E8/LSB)
    WRITE_COMMAND_8, 0xB2,
    WRITE_DATA_8, 0x0C,
    WRITE_DATA_8, 0x0C,
    WRITE_DATA_8, 0x00,
    WRITE_DATA_8, 0x33,
    WRITE_DATA_8, 0x33,
    WRITE_COMMAND_8, 0xB7,
    WRITE_DATA_8, 0x75,
    WRITE_COMMAND_8, 0xBB,
    WRITE_DATA_8, 0x1A,
    WRITE_COMMAND_8, 0xC0,
    WRITE_DATA_8, 0x2C,
    WRITE_COMMAND_8, 0xC2,
    WRITE_DATA_8, 0x01,
    WRITE_DATA_8, 0xFF,
    WRITE_COMMAND_8, 0xC3,
    WRITE_DATA_8, 0x13,
    WRITE_COMMAND_8, 0xC4,
    WRITE_DATA_8, 0x20,
    WRITE_COMMAND_8, 0xC6,
    WRITE_DATA_8, 0x0F,
    WRITE_COMMAND_8, 0xD0,
    WRITE_DATA_8, 0xA4,
    WRITE_DATA_8, 0xA1,
    WRITE_COMMAND_8, 0xD6,
    WRITE_DATA_8, 0xA1,
    WRITE_COMMAND_8, 0xE0,
    WRITE_DATA_8, 0xD0,
    WRITE_DATA_8, 0x0D,
    WRITE_DATA_8, 0x14,
    WRITE_DATA_8, 0x0D,
    WRITE_DATA_8, 0x0D,
    WRITE_DATA_8, 0x09,
    WRITE_DATA_8, 0x38,
    WRITE_DATA_8, 0x44,
    WRITE_DATA_8, 0x4E,
    WRITE_DATA_8, 0x3A,
    WRITE_DATA_8, 0x17,
    WRITE_DATA_8, 0x18,
    WRITE_DATA_8, 0x2F,
    WRITE_DATA_8, 0x30,
    WRITE_COMMAND_8, 0xE1,
    WRITE_DATA_8, 0xD0,
    WRITE_DATA_8, 0x09,
    WRITE_DATA_8, 0x0F,
    WRITE_DATA_8, 0x08,
    WRITE_DATA_8, 0x07,
    WRITE_DATA_8, 0x14,
    WRITE_DATA_8, 0x37,
    WRITE_DATA_8, 0x44,
    WRITE_DATA_8, 0x4D,
    WRITE_DATA_8, 0x38,
    WRITE_DATA_8, 0x15,
    WRITE_DATA_8, 0x16,
    WRITE_DATA_8, 0x2C,
    WRITE_DATA_8, 0x2E,
    WRITE_COMMAND_8, 0x21,   // inversion ON
    WRITE_COMMAND_8, 0x29,   // display ON
    WRITE_COMMAND_8, 0x2C,   // memory write
    END_WRITE
};
