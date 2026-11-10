// SPDX-License-Identifier: MIT
// See pcf85063_rtc.h.
#include "pcf85063_rtc.h"
#include "../chip/esp32c5_i2c_bus.h"
#include <Wire.h>

namespace C5Parts {
namespace Rtc {
namespace Bus = Esp32C5::I2cBus;

static bool s_present = false;

static bool writeReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}
// STOP-terminated pointer write, then the read (the C5 read path that works
// for the other devices on this bus).
static bool readRegs(uint8_t reg, uint8_t* out, int n) {
    Wire.beginTransmission(ADDR);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return false;
    if (Wire.requestFrom((int)ADDR, n) != n || Wire.available() < n) return false;
    for (int i = 0; i < n; ++i) out[i] = (uint8_t)Wire.read();
    return true;
}
static bool fromBcd(uint8_t bcd, uint8_t max, uint8_t& out) {
    if ((bcd & 0x0F) > 9 || (bcd >> 4) > 9) return false;
    out = (uint8_t)((bcd >> 4) * 10 + (bcd & 0x0F));
    return out <= max;
}
static uint8_t toBcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

bool present() { return s_present; }

bool begin() {
    Bus::Guard guard(100);
    if (!guard) return false;
    Bus::reopen();
    uint8_t ctrl = 0;
    s_present = readRegs(0x00, &ctrl, 1);
    return s_present;
}

bool read(DateTime& out, bool& integrityLost) {
    Bus::Guard guard(100);
    if (!guard) return false;
    Bus::reopen();
    // The seven time registers are read in one transaction so the chip's
    // read latch keeps them coherent; the second pass covers a torn first read.
    for (int attempt = 0; attempt < 2; ++attempt) {
        uint8_t r[7];
        if (!readRegs(0x04, r, 7)) return false;
        DateTime t{};
        uint8_t year = 0;
        const bool ok = fromBcd(r[0] & 0x7F, 59, t.second) &&
                        fromBcd(r[1] & 0x7F, 59, t.minute) &&
                        fromBcd(r[2] & 0x3F, 23, t.hour) &&
                        fromBcd(r[3] & 0x3F, 31, t.day) &&
                        fromBcd(r[5] & 0x1F, 12, t.month) &&
                        fromBcd(r[6], 99, year) && t.day >= 1 && t.month >= 1;
        if (!ok) continue;
        t.weekday = r[4] & 0x07;
        t.year = (uint16_t)(2000 + year);
        integrityLost = (r[0] & 0x80) != 0;
        out = t;
        return true;
    }
    return false;
}

bool set(const DateTime& v) {
    if (v.year < 2000 || v.year > 2099 || v.month < 1 || v.month > 12 ||
        v.day < 1 || v.day > 31 || v.weekday > 6 || v.hour > 23 ||
        v.minute > 59 || v.second > 59)
        return false;
    Bus::Guard guard(100);
    if (!guard) return false;
    Bus::reopen();
    // Stop the clock while the fields are written, then restart it in 24-hour
    // mode. Writing seconds with bit 7 clear also clears the stop flag.
    bool ok = writeReg(0x00, 0x20);
    if (ok) ok = writeReg(0x04, toBcd(v.second));
    if (ok) ok = writeReg(0x05, toBcd(v.minute));
    if (ok) ok = writeReg(0x06, toBcd(v.hour));
    if (ok) ok = writeReg(0x07, toBcd(v.day));
    if (ok) ok = writeReg(0x08, v.weekday);
    if (ok) ok = writeReg(0x09, toBcd(v.month));
    if (ok) ok = writeReg(0x0A, toBcd((uint8_t)(v.year - 2000)));
    // Always try to restart the clock, even after a failed field write.
    const bool restarted = writeReg(0x00, 0x00);
    return ok && restarted;
}
}  // namespace Rtc
}  // namespace C5Parts
