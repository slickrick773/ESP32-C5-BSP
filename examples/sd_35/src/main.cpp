// SPDX-License-Identifier: MIT
// Waveshare 3.5-inch microSD test. The card shares the display's SPI bus
// (SCLK 6, MOSI 7, MISO 2) with its own chip select on GPIO 9.
//
// Mounts the card, reports its type and size, then writes a small file, reads
// it back, compares it, and deletes it. No display is rendered (backlight
// off). The test file is /bsp_sd_test.txt; nothing else on the card is touched.
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <string.h>
#include <chip/esp32c5_i2c_bus.h>
#include <board/ws_c5_35/ws_c5_35_board.h>

static const char* kPath = "/bsp_sd_test.txt";
static const char kPayload[] = "ESP32-C5-BSP sd_35 write/read check 0123456789\n";
// 4 MHz mounted and passed on hardware; faster clocks are not characterised here.
static constexpr uint32_t kSpiHz = 4000000;

static const char* typeName(uint8_t t) {
    switch (t) {
        case CARD_MMC:  return "MMC";
        case CARD_SD:   return "SDSC";
        case CARD_SDHC: return "SDHC/SDXC";
        default:        return "unknown";
    }
}

static bool roundTrip() {
    File f = SD.open(kPath, FILE_WRITE);
    if (!f) { Serial.println("[sd_35] open for write FAILED"); return false; }
    const size_t wrote = f.print(kPayload);
    f.close();
    if (wrote != strlen(kPayload)) { Serial.println("[sd_35] short write"); return false; }

    f = SD.open(kPath, FILE_READ);
    if (!f) { Serial.println("[sd_35] open for read FAILED"); return false; }
    char back[sizeof(kPayload)] = {};
    const size_t got = f.readBytes(back, sizeof(back) - 1);
    f.close();
    if (got != strlen(kPayload) || memcmp(back, kPayload, got) != 0) {
        Serial.println("[sd_35] read-back MISMATCH");
        return false;
    }
    Serial.printf("[sd_35] wrote and read back %u bytes: match\n", (unsigned)got);
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Esp32C5::I2cBus::begin(WsC5_35::I2C_SDA, WsC5_35::I2C_SCL, WsC5_35::I2C_HZ);
    // Board::begin() resets SPI2 and drives the display and SD chip selects
    // inactive before either device is used, and keeps the backlight off.
    if (!WsC5_35::begin(0)) {
        Serial.println("[sd_35] board startup FAILED");
        return;
    }
    SPI.begin(WsC5_35::LCD_SCLK, WsC5_35::LCD_MISO, WsC5_35::LCD_MOSI);
    if (!SD.begin(WsC5_35::SD_CS, SPI, kSpiHz)) {
        Serial.println("[sd_35] mount FAILED (no card, unformatted card, or wiring)");
        return;
    }
    Serial.printf("[sd_35] mounted: %s, %llu MB\n", typeName(SD.cardType()),
                  SD.cardSize() / (1024ULL * 1024ULL));
    const bool ok = roundTrip();
    const bool removed = SD.remove(kPath);
    if (!removed) Serial.println("[sd_35] test file removal FAILED");
    SD.end();
    Serial.printf("[sd_35] result: %s\n", ok && removed ? "PASS" : "FAIL");
}

void loop() { delay(1000); }
