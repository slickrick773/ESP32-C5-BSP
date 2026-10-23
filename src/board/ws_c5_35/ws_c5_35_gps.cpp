// SPDX-License-Identifier: MIT
// See ws_c5_35_gps.h. Protocol is Waveshare's LC76G_RPI_I2C demo (i2c_read.c /
// i2c_write.c), ported onto the shared-bus core. Verified on hardware.
//
// Transaction (10 ms in the demo; a short settle works here because the module
// clock-stretches, which keeps the main-loop hitch small):
//   0. 1-byte dummy write to 0x50 (wake / presence)         [Recovery on NACK]
//   1. write {reg 0xAA510008, len 4} (LE) to 0x50   -> "how much?"
//   2. read 4 bytes from 0x54                       -> length (LE)
//   3. write {reg 0xAA512000, len N} (LE) to 0x50   -> "send N"
//   4. read N bytes from 0x54                       -> NMEA text
#include "ws_c5_35_gps.h"
#include "ws_c5_35_pins.h"
#include "../../chip/esp32c5_i2c_bus.h"
#include <Arduino.h>
#include <Wire.h>

namespace WsC5_35 {
namespace Gps {

namespace Bus = Esp32C5::I2cBus;

static const uint8_t  CR_ADDR  = 0x50;   // command/register write address
static const uint8_t  RD_ADDR  = 0x54;   // data read address
static const uint8_t  WR_ADDR  = 0x58;   // data write address (wake)
static const uint32_t CHUNK     = 1024;  // bytes per cycle; bounds the hitch
static const uint32_t PERIOD_MS = 500;   // min gap between cycles (~2 Hz)
static const uint32_t STEP_MS   = 3;     // inter-step settle (demo uses 10)

static uint32_t s_cycles = 0, s_bytes = 0, s_fails = 0, s_recov = 0, s_wakes = 0;

// ---- low-level I2C (unchanged protocol) ------------------------------------
static inline void put32le(uint8_t* b, uint32_t v) {
    b[0] = v; b[1] = v >> 8; b[2] = v >> 16; b[3] = v >> 24;
}
static uint8_t writeCr(uint32_t reg, uint32_t len) {   // 0 = ok
    uint8_t b[8]; put32le(b, reg); put32le(b + 4, len);
    Wire.beginTransmission(CR_ADDR); Wire.write(b, 8); return Wire.endTransmission();
}
static size_t readBytes(uint8_t* buf, size_t n) {
    size_t got = Wire.requestFrom((uint8_t)RD_ADDR, n);
    for (size_t i = 0; i < got; i++) buf[i] = Wire.read();
    return got;
}
static uint8_t dummy(uint8_t addr) {   // 1-byte write; 0 = ACKed
    Wire.beginTransmission(addr); Wire.write((uint8_t)0); return Wire.endTransmission();
}

// ---- minimal NMEA parsing (GGA for fix/sats/pos/hdop, GSV for in-view) ------
static bool    s_fix = false;
static uint8_t s_sats = 0, s_inView = 0;
static double  s_lat = 0, s_lon = 0;
static float   s_hdop = 0;

static double parseLatLon(const char* f, char hemi) {
    // ddmm.mmmm / dddmm.mmmm -> decimal degrees
    const double v = atof(f);
    const int deg = (int)(v / 100);
    const double min = v - deg * 100.0;
    double dd = deg + min / 60.0;
    if (hemi == 'S' || hemi == 'W') dd = -dd;
    return dd;
}

// Split a comma-separated NMEA body into field pointers (in place).
static int splitFields(char* s, char* fields[], int maxF) {
    int n = 0;
    fields[n++] = s;
    for (; *s && n < maxF; s++) {
        if (*s == ',') { *s = 0; fields[n++] = s + 1; }
        else if (*s == '*') { *s = 0; break; }
    }
    return n;
}

static void parseLine(char* line) {
    if (line[0] != '$') return;
    // sentence id is chars 3..5 (skip the 2-char talker), e.g. "GGA","GSV","RMC"
    const char* id = line + 3;
    char* f[20];
    const int n = splitFields(line, f, 20);
    if (!strncmp(id, "GGA", 3) && n >= 9) {
        const int q = atoi(f[6]);
        s_fix  = (q > 0);
        s_sats = (uint8_t)atoi(f[7]);
        s_hdop = (float)atof(f[8]);
        if (s_fix && f[2][0] && f[4][0]) {
            s_lat = parseLatLon(f[2], f[3][0]);
            s_lon = parseLatLon(f[4], f[5][0]);
        }
    } else if (!strncmp(id, "GSV", 3) && n >= 4) {
        s_inView = (uint8_t)atoi(f[3]);
    } else if (!strncmp(id, "RMC", 3) && n >= 3) {
        if (f[2][0] == 'V') s_fix = false;   // 'A' valid, 'V' void
    }
}

// The BSP's own parser sink. Its signature matches ByteSink (below) so a read
// cycle can route bytes either here or to a sketch-supplied sink; `now` is
// unused here because the parser timestamps nothing.
static void parserSink(char c, uint32_t /*now*/) {
    static char line[120];
    static size_t len = 0;
    if (c == '\n' || c == '\r') {
        if (len) { line[len] = 0; parseLine(line); len = 0; }
    } else if (len < sizeof(line) - 1) {
        line[len++] = (char)c;
    } else {
        len = 0;   // overlong: drop and resync
    }
}

// Where a cycle's NMEA bytes go: parserSink (parser tick) or a sketch's own
// parser (byte-sink tick). Set per cycle in service(), consumed by poll().
typedef void (*ByteSink)(char, uint32_t);
static ByteSink s_sink = parserSink;

// ---- one atomic read cycle --------------------------------------------------
// Returns bytes read, 0 if nothing, or -1 on a bus fail. Caller holds the lock.
// `now` is forwarded to the active sink so a sketch parser can timestamp bytes.
static int poll(uint32_t now) {
    static uint8_t buf[CHUNK];
    int out = -1;
    Bus::TimeoutGuard to(300);   // long timeout for the clock-stretch; auto-restored
    Bus::reopen();               // C5 read quirk: re-open ONCE, never between steps
    do {
        delay(STEP_MS);
        if (dummy(CR_ADDR) != 0) {   // demo Recovery_I2C: nudge 0x54 then 0x58
            if (dummy(RD_ADDR) == 0 || dummy(WR_ADDR) == 0) s_recov++;
            break;
        }
        delay(STEP_MS);
        if (writeCr(0xAA510008UL, 4) != 0) break;
        delay(STEP_MS);
        uint8_t lb[4];
        if (readBytes(lb, 4) != 4) break;
        uint32_t len = lb[0] | (lb[1] << 8) | ((uint32_t)lb[2] << 16) | ((uint32_t)lb[3] << 24);
        if (len == 0) { out = 0; break; }
        if (len >= 35 * 1024) break;      // demo treats this as illegal
        if (len > CHUNK) len = CHUNK;      // drain the rest on later cycles
        delay(STEP_MS);
        if (writeCr(0xAA512000UL, len) != 0) break;
        delay(STEP_MS);
        size_t got = readBytes(buf, len);
        if (got != len) break;
        for (size_t i = 0; i < got; i++) s_sink((char)buf[i], now);
        out = (int)got;
    } while (0);
    return out;
}

// Demo Ql_Wake_I2C: nudge the module's I2C transmitter after it idles. Caller
// holds the lock.
static bool wake() {
    bool ok = false;
    Bus::TimeoutGuard to(300);
    Bus::reopen();
    do {
        delay(STEP_MS); if (dummy(CR_ADDR) != 0) break;
        delay(STEP_MS); if (writeCr(0xAA510004UL, 4) != 0) break;
        delay(STEP_MS);
        uint8_t fb[4];
        if (readBytes(fb, 4) != 4) break;
        uint32_t freeLen = fb[0] | (fb[1] << 8) | ((uint32_t)fb[2] << 16) | ((uint32_t)fb[3] << 24);
        if (freeLen < 1) break;
        delay(STEP_MS); if (writeCr(0xAA531000UL, 1) != 0) break;
        delay(STEP_MS); ok = (dummy(WR_ADDR) == 0);
    } while (0);
    return ok;
}

void begin() {
    Wire.setBufferSize(CHUNK + 16);   // persists across Wire.end()/begin()
}

// One service step, feeding the cycle's bytes to `sink`. Both tick() overloads
// are this with a different sink; the throttle/lock/reopen/wake/counters are
// identical, so the parser tick and the byte-sink tick behave the same but for
// where the NMEA lands.
static void service(ByteSink sink, uint32_t now, uint32_t busyUntilMs) {
    static uint32_t s_lastAt = 0, s_empty = 0;
    if (s_lastAt && now - s_lastAt < PERIOD_MS) return;
    if (busyUntilMs && (int32_t)(busyUntilMs - now) > 0) return;   // defer while touched

    // GPS owns the bus for the whole multi-frame cycle; wait briefly for it.
    // Touch trylocks non-blocking and skips a frame while we hold it.
    Bus::Guard g(100);
    if (!g) return;

    s_sink = sink;
    s_lastAt = now ? now : 1;
    int r = poll(now);
    s_cycles++;
    if (r < 0)       s_fails++;
    else if (r == 0) { if (++s_empty >= 8) { s_empty = 0; if (wake()) s_wakes++; } }
    else             { s_empty = 0; s_bytes += r; }
}

void tick(uint32_t now, uint32_t busyUntilMs) { service(parserSink, now, busyUntilMs); }

void tick(void (*sink)(char, uint32_t), uint32_t now, uint32_t busyUntilMs) {
    service(sink ? sink : parserSink, now, busyUntilMs);
}

bool     hasFix()           { return s_fix; }
uint8_t  satellites()       { return s_sats; }
uint8_t  satellitesInView() { return s_inView; }
double   latitude()         { return s_lat; }
double   longitude()        { return s_lon; }
float    hdop()             { return s_hdop; }
uint32_t bytesRead()        { return s_bytes; }
uint32_t cycles()           { return s_cycles; }
uint32_t fails()            { return s_fails; }

}  // namespace Gps
}  // namespace WsC5_35
