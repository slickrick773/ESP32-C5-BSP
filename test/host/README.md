# Host tests

Native (no-hardware, no-PlatformIO) regression tests for the BSP's reliability
guarantees. The **real** driver sources are compiled against small mocks of the
Arduino / Wire / FreeRTOS / XPowersLib surface (in [`mock/`](mock)), so the tests
exercise the actual functions, not reimplementations.

```
bash test/host/run.sh      # builds + runs; exits non-zero on any failure
```

## What is covered

| Test | Real code under test | Asserts |
|------|----------------------|---------|
| `test_bus` | `esp32c5_i2c_bus.cpp` — `recover()`, `lock()`/`Guard` | `recover()` returns `false` before `begin()` (uninitialised) and when SDA stays held low, `true` when SDA is already free or the clock pulses free it; a `Guard` whose lock fails runs no bus op; a successful `Guard` releases the lock exactly once on scope exit and a failed one releases nothing. |
| `test_clients` | `ws_c5_35_battery.cpp` `refresh()`, `ws_c5_35_expander.cpp` `backlight()` | On a failed lock, `refresh()` reads no PMU registers and does not reopen the bus, and `backlight()` writes nothing; both act when the lock is held. |
| `test_touch` | `ws_c5_35_touch.cpp` `ftRead()`/`parsePoint()`/`read()` | A short (truncated) I²C packet yields **no** coordinate (`read()` returns `false`, outputs untouched); a complete packet parses the correct point. |

These catch the exact pre-fix defects: before this change `refresh()`/`backlight()`
touched the bus with no lock, `ftRead()` zero-filled a short read and returned a
phantom point near `(0,0)`, and `recover()` returned `true` even when SDA stayed
stuck. Each test fails against that prior behaviour.

## What still requires hardware validation

The host tests cover **decision logic only**. They do not — and cannot — prove:

- **Real electrical bus recovery.** `recover()` is tested against a mocked SDA
  line; whether 9 SCL pulses actually free a wedged slave, and whether the final
  `digitalRead(SDA)` reflects the true bus state, must be confirmed on hardware.
- **Live multitask interleaving.** The lock prevents a battery/backlight bus op
  from landing inside a GPS transaction *by construction*; these tests prove the
  "no bus op on failed lock" property, not a real two-task race on silicon.
- **Real FT6336 short-read conditions.** The truncated packet is synthetic; how
  often and under what conditions the C5 bus actually drops touch bytes is a
  hardware question.
- **Actual AXP2101 register reads** (XPowersLib is stubbed) and the GPS/touch
  happy-path timing (unchanged by this increment, but not re-run on hardware here).

## 2.8-inch audio coverage

`test_audio_28` checks codec ID/short-read/configuration failures, expander
output preservation, BCLK selection, the vendor 16 kHz DAC oversampling value,
volume writes, and amp shutdown. `test_audio_shutdown` compiles the example's
actual shutdown helper against the real audio/expander drivers and a fake I2S
transport. It verifies retry/recovery for lock and I2C failures, zero-only
fallback writes, no I2S shutdown while amp-disable is unconfirmed, and reported
I2S shutdown failure. Persistent-fault observation is bounded by the host fake;
the firmware deliberately continues retrying with a scheduler yield.

These are software failure-path checks. They do not prove electrical amp state,
I2S pin wiring, actual clocks, analog audio quality, or concurrent hardware use.

## 3.5-inch audio coverage

`test_audio_35` checks codec ID/short-read/configuration/lock failures, BCLK
clock selection, the hardware-tested DAC oversampling value, volume writes and
shutdown for playback. For capture it checks that recording is refused while
playback is configured (and the reverse), setup write and lock failures, the
analog-mic and ADC-volume registers, the DAC staying muted, shutdown, and that
the playback configuration is unchanged after a recording session.

These are register-level checks against mocked I²C. They do not prove I2S pin
wiring, clocks, microphone signal, or audio quality.

`test_rtc_35` checks clock detection, BCD decoding, the oscillator-stop flag,
rejection of invalid BCD, out-of-range and short reads, range checks before a
set, the written register values, and that the clock is restarted after a
failed write. `test_imu_35` checks identity rejection, the configuration
registers, scaling to g and degrees per second, short reads and shutdown. Both
check that a lock failure performs no bus read. They run against a mocked
register map and say nothing about the real parts' behaviour.

The `test_battery_28` suite checks 2.8-inch expander ADC decoding and nominal
voltage conversion, initialization/lock failures, NACK/short/invalid reads,
caller-data preservation, and read-only sampling. It does not validate physical
voltage accuracy, charging, or battery presence.

Startup regression coverage checks both board variants with zero and default
brightness, initialization failure/retry, deferred reveal, and repeat-safe SPI
preparation. The separate bounded bootloader bus tests are documented in
[the experimental probe](../../tools/startup_probe/README.md).
