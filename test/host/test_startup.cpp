// SPDX-License-Identifier: MIT
#include <Arduino.h>
#include <Wire.h>
#include "testutil.h"
#ifdef BOARD_28
#include "../../src/board/ws_c5_28/ws_c5_28_board.h"
namespace Board = WsC5_28;
#else
#include "../../src/board/ws_c5_35/ws_c5_35_board.h"
namespace Board = WsC5_35;
#endif
static int resets;
namespace Esp32C5 { namespace Spi2 { void resetPeripheral() { ++resets; } } }
#ifdef BOARD_28
namespace WsC5_28 { namespace Touch { bool begin() { return true; } } }
#else
namespace WsC5_35 { namespace Touch { bool begin() { return true; } } }
#endif
int main(int argc, char**) {
    const uint8_t brightness = argc > 1 ? 255 : 0;
    g_wireRegisterMode = true;
    g_wireEndTransStatus = 2;
    CHECK(!Board::begin(brightness), "NACK cannot mark expander ready");
    CHECK(!Board::Expander::present(), "failed init remains retryable");
    g_wireEndTransStatus = 0;
    g_wireWritesLen = 0;
    CHECK(Board::begin(brightness), "retry succeeds");
    CHECK(resets == 1, "failed initialization retry does not reset live SPI");
    CHECK(g_wireRegisters[0x24][0x05] == brightness, "requested PWM is final state");
    bool sawZero = false;
    for (unsigned i = 0; i < g_wireWritesLen; ++i) {
        const auto w = g_wireWrites[i];
        if (w.reg == 5 && w.value == 0) sawZero = true;
        if (w.reg == 5 && w.value != 0)
            CHECK(i == g_wireWritesLen - 1, "no illumination before reset sequence completes");
    }
    CHECK(sawZero, "initialization suppresses PWM");
#ifndef BOARD_28
    CHECK(g_wireWrites[0].reg == 5 && g_wireWrites[0].value == 0,
          "3.5-inch suppresses PWM before mode/power changes");
#endif
    CHECK(g_pinLevel[Board::LCD_CS] == HIGH && g_pinLevel[Board::SD_CS] == HIGH,
          "startup deselects display and SD");
    const unsigned csWrites = g_pinWriteCount[Board::LCD_CS] + g_pinWriteCount[Board::SD_CS];
    const unsigned writes = g_wireWritesLen;
    Board::prepareSharedSpi();
    CHECK(Board::begin(255), "repeat begin succeeds");
    CHECK(resets == 1, "repeat preparation never resets SPI2");
    CHECK(g_pinWriteCount[Board::LCD_CS] + g_pinWriteCount[Board::SD_CS] == csWrites,
          "repeat preparation does not disturb active chip selects");
    CHECK(g_wireWritesLen == writes, "repeat begin never pulses panel reset or changes PWM");
    Board::Expander::backlight(120);
    CHECK(g_wireRegisters[0x24][5] == 120, "explicit reveal still works");
    return g_failures;
}
