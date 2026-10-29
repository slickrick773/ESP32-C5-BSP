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
