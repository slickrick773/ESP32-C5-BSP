// SPDX-License-Identifier: MIT
#include <cassert>
#include <cstdio>
#include <vector>
#include "../bootloader_components/dark_start/early_i2c.h"

static bool sdaReleased, sclReleased, active;
static unsigned edges, elapsed, stretchReads, nackByte, loseAt, stuckAfter;
static bool stuckSda, stuckScl;
static std::vector<unsigned> bytes;
static unsigned byteValue;
static void sda(bool v) {
    if (sclReleased && !v && sdaReleased) {
        active = true; edges = 0; byteValue = 0;
    }
    if (sclReleased && v && !sdaReleased) active = false;
    sdaReleased = v;
}
static void scl(bool v) {
    if (v && !sclReleased && active) {
        ++edges;
        if (edges % 9) byteValue = (byteValue << 1) | sdaReleased;
        else { bytes.push_back(byteValue); byteValue = 0; }
    }
    sclReleased = v;
}
static bool readSda() {
    if (stuckSda) return false;
    if (loseAt && edges == loseAt) return false;
    if (active && edges && edges % 9 == 0) return edges / 9 == nackByte;
    return sdaReleased;
}
static bool readScl() {
    if (stuckScl || (stuckAfter && edges >= stuckAfter)) return false;
    if (stretchReads) { --stretchReads; return false; }
    return sclReleased;
}
static void delayUs(uint32_t us) { elapsed += us; }
static const early_i2c_io io = {sda, scl, readSda, readScl, delayUs};
static void reset() {
    sdaReleased = sclReleased = true; active = false;
    edges = elapsed = stretchReads = nackByte = loseAt = byteValue = stuckAfter = 0;
    stuckSda = stuckScl = false; bytes.clear();
}
static void bounded(enum early_i2c_result expected) {
    assert(early_pwm_off(&io) == expected);
    assert(elapsed <= 2500);
    assert(sdaReleased && sclReleased);
}
int main() {
    reset(); bounded(EARLY_OK);
    assert((bytes == std::vector<unsigned>{0x48, 0x05, 0x00}));
    assert(!active);
    for (unsigned n = 1; n <= 3; ++n) {
        reset(); nackByte = n; bounded(EARLY_NACK);
        assert(bytes.size() == n); assert(!active);
    }
    reset(); stuckSda = true; bounded(EARLY_BUSY); assert(bytes.empty());
    reset(); stuckScl = true; bounded(EARLY_TIMEOUT); assert(bytes.empty());
    for (unsigned edge = 1; edge <= 27; ++edge) {
        reset(); stuckAfter = edge; bounded(EARLY_TIMEOUT);
    }
    reset(); stretchReads = 399; bounded(EARLY_OK);
    reset(); stretchReads = 401; bounded(EARLY_TIMEOUT);
    reset(); loseAt = 2; bounded(EARLY_BUSY); assert(bytes.empty());
    puts("early I2C: bytes, ACK/NACK at each byte, busy, stretch, timeout, arbitration pass");
}
