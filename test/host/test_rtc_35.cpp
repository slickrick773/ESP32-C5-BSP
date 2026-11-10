// SPDX-License-Identifier: MIT
// Host test for the 3.5-inch RTC driver against a mocked register map.
#include <Wire.h>
#include <freertos/semphr.h>
#include "testutil.h"
#include "../../src/board/ws_c5_35/ws_c5_35_rtc.h"
#include "../../src/chip/esp32c5_i2c_bus.h"
namespace R = WsC5_35::Rtc;
static uint8_t* reg = g_wireRegisters[0x51];
int main() {
    Esp32C5::I2cBus::begin(27, 26, 100000);
    g_wireRegisterMode = true;

    g_wireEndTransStatus = 2;
    CHECK(!R::begin() && !R::present(), "absent clock is not present");
    g_wireEndTransStatus = 0;
    g_takeResult = pdFALSE;
    CHECK(!R::begin(), "begin reports lock failure");
    g_takeResult = pdTRUE;
    CHECK(R::begin() && R::present(), "clock detected");

    // 2026-11-10 Tuesday 01:23:45, oscillator-stop flag set.
    reg[0x04] = 0x80 | 0x45; reg[0x05] = 0x23; reg[0x06] = 0x01; reg[0x07] = 0x10;
    reg[0x08] = 0x02; reg[0x09] = 0x11; reg[0x0A] = 0x26;
    R::DateTime t{}; bool lost = false;
    CHECK(R::read(t, lost), "time read succeeds");
    CHECK(t.year == 2026 && t.month == 11 && t.day == 10 && t.weekday == 2 &&
          t.hour == 1 && t.minute == 23 && t.second == 45, "BCD fields decoded");
    CHECK(lost, "oscillator-stop flag reported");
    reg[0x04] = 0x45;
    CHECK(R::read(t, lost) && !lost, "cleared flag reported");

    reg[0x05] = 0x7A;  // invalid BCD minutes
    R::DateTime keep{1, 2, 3, 4, 5, 6, 7};
    CHECK(!R::read(keep, lost) && keep.year == 1 && keep.second == 7, "invalid BCD rejected, caller value preserved");
    reg[0x05] = 0x23;
    reg[0x09] = 0x13;  // month 13
    CHECK(!R::read(keep, lost), "out-of-range month rejected");
    reg[0x09] = 0x11;
    g_wireShortRead = 1;
    CHECK(!R::read(keep, lost), "short read rejected");
    g_wireShortRead = 0;
    g_takeResult = pdFALSE;
    const unsigned before = g_wireRequestCount;
    CHECK(!R::read(keep, lost) && g_wireRequestCount == before, "lock failure performs no read");
    g_takeResult = pdTRUE;

    const unsigned writes = g_wireWritesLen;
    CHECK(!R::set({2026, 13, 1, 0, 0, 0, 0}) && g_wireWritesLen == writes, "invalid month rejected without writing");
    CHECK(!R::set({1999, 1, 1, 0, 0, 0, 0}) && g_wireWritesLen == writes, "year before 2000 rejected without writing");
    CHECK(R::set({2027, 12, 31, 5, 23, 59, 58}), "set succeeds");
    CHECK(reg[0x04] == 0x58 && reg[0x05] == 0x59 && reg[0x06] == 0x23 && reg[0x07] == 0x31 &&
          reg[0x08] == 0x05 && reg[0x09] == 0x12 && reg[0x0A] == 0x27, "fields written as BCD, stop flag clear");
    CHECK(reg[0x00] == 0x00, "clock restarted in 24-hour mode");
    CHECK(R::read(t, lost) && t.year == 2027 && t.second == 58 && !lost, "set value reads back");
    g_wireFailReg = 0x07;
    CHECK(!R::set({2027, 1, 1, 0, 0, 0, 0}), "failed field write reported");
    CHECK(reg[0x00] == 0x00, "clock restarted after a failed field write");
    g_wireFailReg = -1;
    return g_failures ? 1 : 0;
}
