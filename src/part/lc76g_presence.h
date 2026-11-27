// SPDX-License-Identifier: MIT
// Whether an LC76G is on the bus at all, and what the driver should do about
// it on each service step. Pure logic, no Arduino and no I2C, so it is tested
// on the host (test/host/test_gps_presence.cpp).
//
// Why it exists: the receiver plugs into a header and many boards run without
// one. The read cycle opens with one-byte writes, and the Arduino core logs
// every write that is not acknowledged as an error, so a board with no
// receiver filled the console. An address-only probe (start, address, stop,
// no data byte) is not logged by the core when nothing answers. So until a
// receiver has answered, the driver makes only those probes, and makes them
// rarely.
//
// The rules:
//   - Before the first check the state is Unknown and a probe is due at once.
//   - A probe nobody answers makes it Absent. The next probe is due
//     RECHECK_MS later, so a receiver plugged in after boot is picked up
//     without a reset.
//   - A probe that is answered makes it Present, and it stays Present until
//     the next boot. From then on the driver runs its normal read cycles and
//     its normal recovery, including for a receiver that stops answering.
#pragma once

#include <stdint.h>

namespace C5Parts {
namespace Gps {

enum class Presence : uint8_t {
    Unknown,   // not checked yet
    Absent,    // nothing answered the last check; checked again every RECHECK_MS
    Present,   // a receiver has answered since boot
};

class PresenceGate {
public:
    static constexpr uint32_t RECHECK_MS = 10000;   // gap between probes while absent

    enum class Step : uint8_t {
        Idle,    // absent and the next probe is not due: stay off the bus
        Probe,   // make one address-only probe, then call probed()
        Poll,    // a receiver has answered: run the normal read cycle
    };

    Presence state() const { return state_; }

    // What the service step at `now` (milliseconds, may wrap) should do.
    Step due(uint32_t now) const {
        if (state_ == Presence::Present) return Step::Poll;
        if (state_ == Presence::Unknown) return Step::Probe;
        return (uint32_t)(now - lastProbeMs_) >= RECHECK_MS ? Step::Probe : Step::Idle;
    }

    // Record the result of the probe made at `now`. Returns true when a
    // receiver has answered, so the caller can run a read cycle straight away.
    bool probed(uint32_t now, bool answered) {
        if (state_ == Presence::Present) return true;   // never withdrawn
        lastProbeMs_ = now;
        state_ = answered ? Presence::Present : Presence::Absent;
        return answered;
    }

private:
    Presence state_ = Presence::Unknown;
    uint32_t lastProbeMs_ = 0;
};

}  // namespace Gps
}  // namespace C5Parts
