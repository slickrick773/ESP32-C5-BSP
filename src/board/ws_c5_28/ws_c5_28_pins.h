// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-2.8 — pin map and panel facts.
//
// The second board in the C5 touch-display class. One place for its hardware
// constants, so nothing downstream has to rediscover them. These were mirrored
// from a working downstream 2.8" port and the Waveshare ESP32-C5-Touch-LCD-2.8
// v1.2.0 schematic, then HARDWARE-VERIFIED from this BSP: display bring-up and
// CST3530 touch run on the real 2.8" board via hello_board_28 (2026-10-31). See
// docs/BOARD-WS-C5-28.md. (Battery voltage readout and battery-only continuation
// are verified; voltage accuracy, charging/runtime and GPS remain unverified.
// Verified audio pins are in ws_c5_28_audio.h.)
//
// Deltas vs the 3.5" (ws_c5_35_pins.h): a different display controller
// (ST7789, not ST7796), a different touch controller (CST3530 at 0x58, not
// FT6336 at 0x38), a different I2C bus (SDA 0 / SCL 1, not 27/26), and a
// different SPI pinout (MISO 8 / CS 10 / DC 9 / SD_CS 23). The CH32V006 I/O
// expander is the same chip at the same 0x24, but with a 2.8"-specific
// bring-up (no EXIO5 panel rail).
//
// The display SPI pins are repeated here (not just in the example's driver
// config) so board code that is NOT the graphics library — SD on the same bus,
// diagnostics — has one honest source. They are named LCD_* for the same
// reason as the 3.5": a graphics library may #define bare TFT_MOSI/TFT_DC
// macros that would clobber a same-named constexpr.
#pragma once

#include <stdint.h>

namespace WsC5_28 {

// ---- Shared I2C bus (touch / expander / GPS all live here) ------------------
// NOTE: different pins from the 3.5" (27/26). On the 2.8" the bus is SDA 0 /
// SCL 1. Touch, the 0x24 expander and (over I2C) an LC76G GPS share it.
constexpr int      I2C_SDA      = 0;
constexpr int      I2C_SCL      = 1;
constexpr uint32_t I2C_HZ       = 100000;   // 100 kHz: robust over the panel flex

// ---- Display: ST7789 240x320 over SPI (shared with the microSD card) --------
constexpr int      LCD_SCLK     = 6;
constexpr int      LCD_MOSI     = 7;
constexpr int      LCD_MISO     = 8;        // 3.5" uses 2 here
constexpr int      LCD_CS       = 10;       // 3.5" uses 8
constexpr int      LCD_DC       = 9;        // 3.5" uses 5
// Display reset is NOT a GPIO: it is EXIO1 on the 0x24 expander (see below).
// On this panel there is no hardware reset LINE the sketch can pulse; the
// proven init opens with a 120 ms settle and sends NO 0x01 soft-reset.
constexpr int      LCD_RST      = -1;
constexpr int      LCD_BL       = -1;       // backlight is the expander's PWM, not a GPIO
constexpr int      PANEL_W      = 240;      // native portrait glass
constexpr int      PANEL_H      = 320;
// ST7789 on this board wants colour inversion ON (the proven init sends 0x21).
constexpr bool     PANEL_NEEDS_INVERSION = true;
// Landscape rotation. Native panel is 240x320 portrait; rotation 1 lands it
// 320x240 landscape, which is how the board is used and how the init + the
// CST3530 touch mapping are laid out.
constexpr uint8_t  ROTATION     = 1;

// Byte order: MSB-first. The ST7789 RAMCTRL (0xB0) must be written 0x00 0xE0
// for a graphics library that pushes MSB-first 16-bit pixels (Arduino_GFX).
// Waveshare's own vendor code uses raw LSB-first buffers and writes 0xB0 0x00
// 0xE8 instead — getting this byte wrong is the classic swapped-colour symptom
// on this panel. See the register table below and docs/BOARD-WS-C5-28.md.
constexpr uint8_t  LCD_RAMCTRL_REG   = 0xB0;
constexpr uint8_t  LCD_RAMCTRL_B0    = 0x00;
constexpr uint8_t  LCD_RAMCTRL_MSB   = 0xE0;   // MSB-first (Arduino_GFX, 16-bit)
constexpr uint8_t  LCD_RAMCTRL_LSB   = 0xE8;   // LSB-first (vendor raw buffers)

// Driver-neutral record of the proven ST7789 init, so the panel spec lives in
// src/ even though the driver-coded form (Arduino_GFX opcodes) stays in the
// example. Mirrored from a downstream 2.8" port, which attributes it to
// Waveshare ESP32-C5-Touch-LCD-2.8 v1.2.0 examples/arduino/06_lvgl_demo/
// Display_ST7789.cpp (77a7dce). Sequence (command : data...):
//
//   delay 120 ms
//   0x29                              display ON   (sent early, then again at end)
//   delay 120 ms
//   0x11                              sleep OUT
//   delay 120 ms
//   0x36 : 0x00                       MADCTL: no flips (rotation is the driver's)
//   0x3A : 0x05                       COLMOD: 16-bit RGB565
//   0xB0 : 0x00 0xE0                  RAMCTRL: MSB-first (the byte-order fix)
//   0xB2 : 0x0C 0x0C 0x00 0x33 0x33   porch control
//   0xB7 : 0x75                       gate control
//   0xBB : 0x1A                       VCOM
//   0xC0 : 0x2C                       LCM control
//   0xC2 : 0x01 0xFF                  VDV/VRH enable
//   0xC3 : 0x13                       VRH
//   0xC4 : 0x20                       VDV
//   0xC6 : 0x0F                       FRCTRL2 (frame rate)
//   0xD0 : 0xA4 0xA1                  power control 1
//   0xD6 : 0xA1                       (undocumented vendor power reg)
//   0xE0 : D0 0D 14 0D 0D 09 38 44 4E 3A 17 18 2F 30   positive gamma
//   0xE1 : D0 09 0F 08 07 14 37 44 4D 38 15 16 2C 2E   negative gamma
//   0x21                              inversion ON
//   0x29                              display ON
//   0x2C                              memory write (start)

// ---- Touch: CST3530 capacitive, on the shared I2C bus -----------------------
// A DIFFERENT controller from the 3.5"'s FT6336. Addressed at 0x58 with a
// four-byte register and a repeated START (not the FT6336's single-byte,
// STOP-based read). INT line is a real GPIO here (GPIO5).
constexpr uint8_t  CST3530_ADDR     = 0x58;
constexpr int      CST3530_INT_GPIO = 5;
// Registers used by the read path (see ws_c5_28_touch.cpp).
constexpr uint32_t CST3530_REG_READ = 0xD0070000;   // first-contact packet (9 bytes)
constexpr uint32_t CST3530_REG_ACK  = 0xD00002AB;    // acknowledge a complete packet

// ---- I/O expander: CH32V006 at 0x24 on the shared I2C bus -------------------
// Same chip and address as the 3.5", but a 2.8"-specific bring-up: this board
// has NO EXIO5 panel-power rail, so only the two resets are driven.
constexpr uint8_t  EXIO_ADDR     = 0x24;
constexpr uint8_t  EXIO_MODE_REG = 0x02;    // direction/mode
constexpr uint8_t  EXIO_OUT_REG  = 0x03;    // output latch
constexpr uint8_t  EXIO_PWM_REG  = 0x05;    // backlight PWM
constexpr uint8_t  EXIO_TP_RST   = 0;       // EXIO0 -> CST3530 reset
constexpr uint8_t  EXIO_LCD_RST  = 1;       // EXIO1 -> ST7789 reset
// (No EXIO_POWER: the 2.8" has no EXIO5 panel rail.)

// ---- microSD: on the display SPI bus ----------------------------------------
constexpr int      SD_CS         = 23;      // 3.5" uses 9

}  // namespace WsC5_28
