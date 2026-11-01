// SPDX-License-Identifier: MIT
#include <Wire.h>
#include <freertos/semphr.h>
#include "testutil.h"
#include "../../src/board/ws_c5_28/ws_c5_28_expander.h"
#include "../../examples/audio_28/src/audio_shutdown.h"

struct StopObservation {};
struct FakeI2s {
    int writes = 0, ends = 0;
    int recoverAfter = 0; // zero: leave failure persistent
    bool failWrites = false, failEnd = false, allZero = true, stoppedWithAmpOn = false;
    size_t write(const uint8_t* data, size_t n) {
        for (size_t i = 0; i < n; ++i) allZero &= data[i] == 0;
        ++writes;
        if (recoverAfter && writes == recoverAfter) {
            g_takeResult = pdTRUE;
            g_wireEndTransStatus = 0;
        }
        // Bound the host observation of an intentionally indefinite retry loop.
        if (!recoverAfter && writes == 4) throw StopObservation{};
        return failWrites ? 0 : n;
    }
    bool end() {
        ++ends;
        stoppedWithAmpOn |= (g_wireRegisters[0x24][3] & 8) != 0;
        return !failEnd;
    }
};
int main() {
    namespace E = WsC5_28::Expander;
    g_wireRegisterMode = true;
    CHECK(E::begin(), "initialise real expander driver");
    CHECK(E::setOutput(3, true), "enable amp for shutdown test");
    FakeI2s immediate;
    CHECK(Audio28Example::shutdown(immediate), "successful shutdown returns true");
    CHECK(immediate.ends == 1 && !immediate.stoppedWithAmpOn && immediate.writes == 0,
          "stop clocks only after acknowledged amp disable");

    E::setOutput(3, true);
    g_takeResult = pdFALSE;
    FakeI2s locked; locked.recoverAfter = 2;
    CHECK(Audio28Example::shutdown(locked), "shutdown recovers after lock failures");
    CHECK(locked.writes == 2 && locked.allZero && locked.ends == 1 && !locked.stoppedWithAmpOn,
          "feed only silence while retrying; disable before ending I2S");

    E::setOutput(3, true);
    g_wireEndTransStatus = 2;
    FakeI2s nack; nack.recoverAfter = 3; nack.failWrites = true;
    CHECK(Audio28Example::shutdown(nack), "retry amp NACK even when silence writes fail");
    CHECK(nack.writes == 3 && nack.ends == 1 && !nack.stoppedWithAmpOn,
          "failed silence writes do not authorize clock shutdown");

    E::setOutput(3, true);
    g_wireEndTransStatus = 2;
    FakeI2s persistent; persistent.failWrites = true;
    bool observed = false;
    try { Audio28Example::shutdown(persistent); } catch (const StopObservation&) { observed = true; }
    CHECK(observed && persistent.ends == 0 && persistent.allZero,
          "persistent failures never call I2S end or resume tone");
    g_wireEndTransStatus = 0;
    FakeI2s stopFailure; stopFailure.failEnd = true;
    CHECK(!Audio28Example::shutdown(stopFailure), "report I2S end failure");
    CHECK(!stopFailure.stoppedWithAmpOn && g_wireRegisters[0x24][3] == 3,
          "amp disabled and reset bits preserved even if I2S end fails");
    return g_failures ? 1 : 0;
}
