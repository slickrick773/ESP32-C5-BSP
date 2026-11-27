// SPDX-License-Identifier: MIT
// Host tests for the LC76G presence decision (src/part/lc76g_presence.h) and
// for the REAL driver (src/part/lc76g_gps.cpp) obeying it against a mocked
// bus: no data byte is written while no receiver has answered.
#include <cstdio>
#include <Arduino.h>
#include <Wire.h>
#include "testutil.h"
#include "../../src/chip/esp32c5_i2c_bus.h"
#include "../../src/part/lc76g_gps.h"

using C5Parts::Gps::Presence;
using C5Parts::Gps::PresenceGate;
namespace Gps = C5Parts::Gps;
typedef PresenceGate::Step Step;

static const uint32_t RECHECK = PresenceGate::RECHECK_MS;

static void testGate() {
    std::printf("gate: the first check decides:\n");
    {
        PresenceGate g;
        CHECK(g.state() == Presence::Unknown, "starts Unknown");
        CHECK(g.due(0) == Step::Probe, "a probe is due at once, at time 0");
        CHECK(g.due(123456) == Step::Probe, "and at any later time while Unknown");
        CHECK(g.probed(40, false) == false, "an unanswered probe reports no receiver");
        CHECK(g.state() == Presence::Absent, "one unanswered probe makes it Absent");
    }
    {
        PresenceGate g;
        CHECK(g.probed(40, true) == true, "an answered first probe reports a receiver");
        CHECK(g.state() == Presence::Present, "one answered probe makes it Present");
        CHECK(g.due(40) == Step::Poll, "Present polls in the same step");
    }

    std::printf("gate: absent stays off the bus between slow probes:\n");
    {
        PresenceGate g;
        g.probed(1000, false);
        CHECK(g.due(1000) == Step::Idle, "idle straight after an unanswered probe");
        CHECK(g.due(1000 + RECHECK - 1) == Step::Idle, "idle one millisecond before the re-check");
        CHECK(g.due(1000 + RECHECK) == Step::Probe, "probe due at the re-check interval");
        g.probed(1000 + RECHECK, false);
        CHECK(g.state() == Presence::Absent, "still Absent after a second unanswered probe");
        CHECK(g.due(1000 + 2 * RECHECK - 1) == Step::Idle, "the interval restarts from the last probe");
        CHECK(g.due(1000 + 2 * RECHECK) == Step::Probe, "and the next probe comes due again");

        int probes = 0;
        for (uint32_t t = 1000 + 2 * RECHECK; t < 1000 + 2 * RECHECK + 60000; t += 10)
            if (g.due(t) == Step::Probe) { probes++; g.probed(t, false); }
        CHECK(probes == 6, "six probes in a minute of 10 ms steps, no more");
    }

    std::printf("gate: a receiver plugged in later is picked up:\n");
    {
        PresenceGate g;
        g.probed(0, false);
        CHECK(g.probed(RECHECK, true) == true, "an answered re-check reports a receiver");
        CHECK(g.state() == Presence::Present, "Absent becomes Present");
        CHECK(g.due(RECHECK) == Step::Poll, "and polls at once");
    }

    std::printf("gate: Present is kept:\n");
    {
        PresenceGate g;
        g.probed(0, true);
        CHECK(g.probed(5000, false) == true, "a later unanswered result does not withdraw it");
        CHECK(g.state() == Presence::Present, "state stays Present");
        CHECK(g.due(5000 + 10 * RECHECK) == Step::Poll, "never falls back to probing");
    }

    std::printf("gate: the millisecond clock may wrap:\n");
    {
        PresenceGate g;
        const uint32_t nearWrap = 0xFFFFFFFFu - 2999;   // 3 s before the wrap
        g.probed(nearWrap, false);
        CHECK(g.due(nearWrap + 5000) == Step::Idle, "idle across the wrap before the interval");
        CHECK(g.due(nearWrap + RECHECK) == Step::Probe, "probe due after the wrap");
    }
}

// Run the driver for `ms` of mocked time in 10 ms loop passes.
static void run(uint32_t ms) {
    for (uint32_t end = g_millis + ms; g_millis < end; g_millis += 10) Gps::tick(g_millis);
}

static void testDriver() {
    Esp32C5::I2cBus::begin(27, 26, 100000);
    Gps::begin();

    std::printf("driver: before the first tick:\n");
    CHECK(Gps::presence() == Presence::Unknown, "presence() is Unknown");
    CHECK(Gps::probes() == 0, "no probe made yet");

    std::printf("driver: no receiver on the bus:\n");
    g_wireEndTransStatus = 2;   // every address NACKs
    g_wireWriteCount = 0; g_wireRequestCount = 0;
    const int reopens = g_wireEndCount;
    g_millis = 1;
    Gps::tick(g_millis);
    CHECK(Gps::presence() == Presence::Absent, "Absent after the first tick");
    CHECK(Gps::probes() == 1, "decided by one probe");
    g_millis += 10;
    run(60000);
    CHECK(Gps::presence() == Presence::Absent, "still Absent a minute later");
    CHECK(Gps::probes() == 7, "one probe at boot and six more in the minute");
    CHECK(g_wireWriteCount == 0, "no data byte written (nothing the core would log)");
    CHECK(g_wireRequestCount == 0, "no read attempted");
    CHECK(g_wireEndCount == reopens, "the bus is not re-opened for a probe");
    CHECK(Gps::cycles() == 0 && Gps::fails() == 0, "cycles() and fails() stay 0 while Absent");

    std::printf("driver: a receiver is plugged in:\n");
    g_wireEndTransStatus = 0;   // addresses ACK from now on
    run(RECHECK);
    CHECK(Gps::presence() == Presence::Present, "Present within one re-check interval");
    CHECK(Gps::probes() == 8, "found by the next probe");
    CHECK(g_wireWriteCount > 0, "the read cycle's writes start");
    CHECK(Gps::cycles() > 0, "read cycles are counted");

    std::printf("driver: once Present, polling is as before:\n");
    const uint32_t c0 = Gps::cycles(), f0 = Gps::fails(), p0 = Gps::probes();
    run(5000);
    CHECK(Gps::cycles() - c0 == 10, "one read cycle every 500 ms");
    CHECK(Gps::probes() == p0, "no more probes");

    // The receiver stops answering: the driver keeps its old behaviour (full
    // cycles with recovery writes, counted as fails), it does not go quiet.
    g_wireEndTransStatus = 2;
    const int w0 = g_wireWriteCount;
    run(5000);
    CHECK(Gps::presence() == Presence::Present, "a receiver that stops answering stays Present");
    CHECK(Gps::cycles() - c0 == 20, "read cycles carry on at 500 ms");
    CHECK(Gps::fails() - f0 >= 10, "and are counted as fails");
    CHECK(g_wireWriteCount - w0 == 30, "three one-byte writes per failed cycle, as before");
    CHECK(Gps::probes() == p0, "still no probes");
}

int main() {
    g_failures = 0;
    testGate();
    testDriver();
    std::printf("test_gps_presence: %d failure(s)\n", g_failures);
    return g_failures;
}
