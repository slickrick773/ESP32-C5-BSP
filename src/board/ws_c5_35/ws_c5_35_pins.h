// SPDX-License-Identifier: MIT
// Waveshare ESP32-C5-Touch-LCD-3.5 — pin map and panel facts.
//
// One place for the board's hardware constants, so nothing downstream has to
// rediscover them. Verified on hardware; see docs/BOARD-WS-C5-35.md and
// docs/HARDWARE-FINDINGS.md.
//
// The display SPI pins are duplicated by the example's TFT_eSPI user-setup
// header (TFT_eSPI only reads its own TFT_* macros). They are repeated here so
// board code that is NOT TFT_eSPI (SD on the same bus, diagnostics) has one
// honest source, and so the two can be checked against each other. They are
// named LCD_* on purpose: TFT_eSPI #defines TFT_MOSI/TFT_DC/etc as macros, and
// a constexpr of the same name in a translation unit that also pulls the
// user-setup would be clobbered by the macro.
#pragma once

#include <stdint.h>

namespace WsC5_35 {

// ---- Shared I2C bus (touch / expander / codec / PMU / GPS all live here) ----
constexpr int      I2C_SDA      = 27;
constexpr int      I2C_SCL      = 26;
constexpr uint32_t I2C_HZ       = 100000;   // 100 kHz: robust over the panel flex

// ---- Display: ST7796 320x480 over SPI (shared with the microSD card) --------
constexpr int      LCD_MOSI     = 7;
constexpr int      LCD_SCLK     = 6;
constexpr int      LCD_MISO     = 2;
constexpr int      LCD_CS       = 8;
constexpr int      LCD_DC       = 5;
// Display reset is NOT a GPIO: it is EXIO1 on the 0x24 expander (see below).
constexpr int      PANEL_W      = 320;      // native portrait glass
constexpr int      PANEL_H      = 480;
// ST7796 on this board wants colour inversion ON (Waveshare's init sends 0x21),
// validated on hardware.
constexpr bool     PANEL_NEEDS_INVERSION = true;

// ---- Touch: FT6336 capacitive, on the shared I2C bus ------------------------
constexpr uint8_t  FT6336_ADDR  = 0x38;
constexpr uint8_t  FT6336_ID    = 0x64;     // FT6336U reports this at reg 0xA3

// ---- I/O expander: CH32V006 at 0x24 on the shared I2C bus --------------------
// Owns panel power/backlight and both resets; there is no backlight GPIO.
constexpr uint8_t  EXIO_ADDR     = 0x24;
constexpr uint8_t  EXIO_MODE_REG = 0x02;    // direction/mode
constexpr uint8_t  EXIO_OUT_REG  = 0x03;    // output latch
constexpr uint8_t  EXIO_PWM_REG  = 0x05;    // backlight PWM
constexpr uint8_t  EXIO_TP_RST   = 0;       // EXIO0 -> FT6336 reset
constexpr uint8_t  EXIO_LCD_RST  = 1;       // EXIO1 -> ST7796 reset
constexpr uint8_t  EXIO_POWER    = 5;       // EXIO5 -> panel power / backlight

// ---- Other devices on the shared bus (addresses, for reference) -------------
constexpr uint8_t  ES8311_ADDR   = 0x18;    // audio codec (Phase later)
// AXP2101 PMU is also on this bus (Phase 4, via XPowersLib).

// ---- microSD: on the display SPI bus ---------------------------------------
constexpr int      SD_CS         = 9;

}  // namespace WsC5_35
