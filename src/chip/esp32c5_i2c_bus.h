// SPDX-License-Identifier: MIT
// ESP32-C5 shared-I2C discipline — chip-level, reusable across any C5 board.
//
// The C5 puts touch, an audio codec, a PMU, an I/O expander and (over I2C) a
// GPS on one bus, and that bus has two sharp edges this core exists to tame:
//
//   1. The C5's I2C controller, once a sketch has called Wire.begin() early,
//      leaves subsequent READS unable to complete (writes ACK, reads come back
//      zero) until the bus is re-opened. The cure that works on hardware is to
//      Wire.end()/Wire.begin() around a read transaction. reopen() is that, in
//      ONE place, so every driver does it the same way.
//
//   2. A multi-frame transaction (the LC76G GPS) must not be interleaved with
//      any other slave, and it needs a long bus timeout that must NOT leak into
//      the short touch reads that follow. lock()/unlock() serialise bus owners
//      (non-blocking tryLock() lets a UI skip a frame instead of stalling), and
//      TimeoutGuard saves/restores Wire's timeout around a transaction.
//
// Single-threaded sketches get the lock uncontended (no behaviour change); a
// multi-task app (Wi-Fi + GPS + touch at once) gets real coordination for free.
// See docs/HARDWARE-FINDINGS.md ("shared I2C bus discipline").
#pragma once

#include <stdint.h>

namespace Esp32C5 {
namespace I2cBus {

// Remember the bus pins/clock, open the bus once, and create the lock. Call
// once in setup() before any driver touches the bus. Idempotent.
void begin(int sda, int scl, uint32_t hz);

// Re-open the bus (Wire.end + Wire.begin on the remembered pins). Call once at
// the start of a read transaction on the C5; see sharp edge (1) above. No-op if
// begin() has not run.
void reopen();

// 9-clock recovery for a bus wedged by a slave holding SDA low (a transaction
// torn down mid-flight can leave the controller stuck, taking every device with
// it). Pulses SCL up to 9 times to let the slave finish, then reopens. Reads
// the lines with digitalRead only, so it issues no transaction of its own.
// Returns true if the bus looked wedged and was recovered.
bool recover();

// Serialise bus owners. lock() blocks up to timeoutMs (0 = non-blocking try,
// UINT32_MAX = wait forever) and returns true if the lock was taken; pair a
// true with unlock(). Safe no-ops (return true) if begin() has not run.
bool lock(uint32_t timeoutMs);
void unlock();

// RAII bus lock. `if (Esp32C5::I2cBus::Guard g{0}) { ...bus work... }` takes the
// lock (non-blocking here), runs the body only if it was free, and releases on
// scope exit. Default waits forever.
class Guard {
public:
    explicit Guard(uint32_t timeoutMs = UINT32_MAX) : held_(lock(timeoutMs)) {}
    ~Guard() { if (held_) unlock(); }
    explicit operator bool() const { return held_; }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
private:
    bool held_;
};

// RAII Wire-timeout override: set a new timeout for a transaction and restore
// the previous one on scope exit, so (e.g.) a long GPS timeout never lingers on
// the short touch reads that follow.
class TimeoutGuard {
public:
    explicit TimeoutGuard(uint16_t timeoutMs);
    ~TimeoutGuard();
    TimeoutGuard(const TimeoutGuard&) = delete;
    TimeoutGuard& operator=(const TimeoutGuard&) = delete;
private:
    uint16_t saved_;
};

}  // namespace I2cBus
}  // namespace Esp32C5
