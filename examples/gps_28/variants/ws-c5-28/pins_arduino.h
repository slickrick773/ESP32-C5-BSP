#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include "soc/soc_caps.h"
#include <stdint.h>

// Waveshare ESP32-C5-Touch-LCD-2.8. The 2.8" twin of the 3.5" board: same C5
// core, but a different wiring (see src/board/ws_c5_28/ws_c5_28_pins.h):
//
//  * Shared I2C bus is SDA 0 / SCL 1 (the 3.5" is 27/26). No RGB_LED on GPIO27
//    on this board either; nothing is defined there.
//  * Display SPI is SCLK 6 / MOSI 7 / MISO 8 (the 3.5" MISO is 2), the panel's
//    CS is GPIO10 and DC is GPIO9. The display driver (Arduino_GFX) is
//    configured in the sketch, not here.
//  * microSD CS is GPIO23 (the 3.5" is 9), on the shared display bus.
//  * GPIO16/17 are the C5's internal MSPI flash/PSRAM pins and are never used.

static const uint8_t TX = 11;
static const uint8_t RX = 12;

static const uint8_t USB_DM = 13;
static const uint8_t USB_DP = 14;

// Default I2C: the shared sensor/touch/expander bus (SDA 0, SCL 1). Code that
// needs a non-default clock calls Wire.begin(0, 1, 100000) explicitly; these
// keep Arduino's bare Wire.begin() sane.
static const uint8_t SDA = 0;
static const uint8_t SCL = 1;

// SPI: the display + microSD shared bus. SS below is the SD card's CS (23).
static const uint8_t SS = 23;
static const uint8_t MOSI = 7;
static const uint8_t MISO = 8;
static const uint8_t SCK = 6;

// LP I2C / LP UART pins are fixed on the ESP32-C5.
static const uint8_t LP_SDA = 4;
static const uint8_t LP_SCL = 5;
#define WIRE1_PIN_DEFINED
#define SDA1 LP_SDA
#define SCL1 LP_SCL
static const uint8_t LP_RX = 12;
static const uint8_t LP_TX = 11;

#define SERIAL_TX 11
#define SERIAL_RX 12

#define SPI_SCK_PIN 6
#define SPI_MOSI_PIN 7
#define SPI_MISO_PIN 8
#define SPI_SS_PIN 23

// microSD, on the display's SPI bus. CS 23.
#define SDCARD_CS 23
#define SDCARD_SCK SPI_SCK_PIN
#define SDCARD_MISO SPI_MISO_PIN
#define SDCARD_MOSI SPI_MOSI_PIN

#endif /* Pins_Arduino_h */
