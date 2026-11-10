# Changelog

All notable changes to this board-support package. A capability is only listed
as hardware-verified once it has been run on a real board; dates are observation
dates. See [docs/HARDWARE-FINDINGS.md](docs/HARDWARE-FINDINGS.md) for the full
evidence and claim tags.

## Unreleased

### Added
- 3.5-inch ES8311 speaker playback: `Audio::begin()` / `setVolume()` / `end()`
  and the `audio_35` three-tone example. Clean playback confirmed by the owner
  on 2026-11-10. Pitch accuracy is unverified.
- 3.5-inch microphone capture: `Audio::beginRecording()` / `endRecording()` and
  the `mic_35` level-test example. Hardware-tested on 2026-11-10: the level is low
  when silent, rises with sound and clips on loud nearby sound, and the owner
  judged one 3-second speech capture clear. Playback configuration is
  unchanged; playback and capture are one at a time.
- Microphone preamp gain: `Audio::setMicGain()` and a gain argument to
  `beginRecording()`. A hardware sweep on 2026-11-10 rose at every step.
- 3.5-inch real-time clock driver (`WsC5_35::Rtc`) and IMU driver
  (`WsC5_35::Imu`), with host tests and the `onboard_35` readout example.
  Hardware-tested on 2026-11-10 for the clock counting and the IMU at rest
  and turned over. `BOOT_BUTTON` pin constant added; presses confirmed.
- `sd_35` microSD example: mount, write, read back, delete. Passed on
  2026-11-10 with one card at 4 MHz while the display was idle.
- `touch_audio` combined display, touch, speaker and microphone sample. On
  2026-11-10 the owner heard a clean tone with the display running and both
  controls responded. The tone is rendered at startup: generating it while
  playing was slower than real time on this chip and made it pulse.
- `audio_35`, `mic_35`, `sd_35`, `onboard_35` and `touch_audio` added to the CI
  example build matrix.
- Optional dark startup via `Board::begin(0)` / `Expander::begin(0)`, preserving
  the full-brightness default and existing reset timing.
- Reusable ESP32-C5 SPI2 reset plus one-time per-board `prepareSharedSpi()`,
  with display and SD deselected before either device uses the shared bus.
  Repeated board initialization does not reset an already prepared SPI bus.
- Additive classified touch sampling (`Contact`, `Released`, `Unavailable`,
  `Error`) while preserving `read()` and `poll()`.
- CI coverage for both 2.8-inch examples, native driver tests, flasher tests,
  and isolated consumer builds.

### Fixed
- `c5flash write` now exits unsuccessfully whenever verification reports a
  textual mismatch, even if the transport subprocess itself returned zero.
- Overview documentation now matches the verified 2.8-inch battery evidence.

Current local validation passed 13 flasher tests, the expanded host driver suite,
bounded bootloader-I2C fault tests, all ten example builds, and two clean
file-based consumer builds of the uncommitted library snapshot. Arduino builds
used Python 3.13 and an isolated PlatformIO core. The git-based consumer CI job
has not been run against this candidate.

Full dark-boot acceptance **FAILED**: cold boot still briefly shows uninitialized
panel memory, and reset still shows the retained previous frame. The behaviour was
characterised: the backlight is on at power-up and the expander's PWM output
controls it — a PWM=0 write to expander register `0x05` is observed to darken the
panel, a physical RST restores the glow and re-defaults the PWM response, and
esp_restart does not. The glow appears in ROM download mode with neither the
second-stage bootloader nor the application running, so it does not require any
user-flashed ESP firmware. The tested ESP-side suppressions (`begin(0)`, an
`initVariant()` hook, the after-init bootloader PWM=0 write) did not eliminate the
observed flash; whether an earlier ROM-level action could pre-empt it is unresolved,
as the first-illumination timing against ROM startup was not measured. Candidate
approaches to remove it entirely — a change to the CH32's power-up state (firmware
not published by the vendor) or a hardware gate — are plausible, not established
requirements. Not established: whether
the expander drives the line high at power-up or it defaults high undriven, and the
flash's optical duration (not instrumented). The retained frame is seen on reset and
did not persist to the final image on a true cold boot. An experimental IDF 5.5.2
after-init bootloader PWM=0 probe was built, host-tested, explicitly authorized,
flashed and verified (`[dark-start] result=0 attempts=1`, normal display and touch),
and eliminated **neither** symptom. See
[its limits and recovery plan](tools/startup_probe/README.md).

## [0.4.0] — 2026-10-29

A reliability + tooling release. **No function-signature or pin changes**; there
is a **notable return-value compatibility change** — `recover()`'s semantics (see
below). The
changes make the package's packaged behaviour match its reliability promise.
Evidence levels are kept distinct: *host-tested* = logic proven by native mocked
tests, *compile-tested* = builds in a clean consumer, *hardware-verified* = run on
a real board (see [docs/HARDWARE-FINDINGS.md](docs/HARDWARE-FINDINGS.md)).

### Fixed (reliability — host-tested)
- **One shared-bus locking policy on every runtime client.** `Battery::refresh()`
  and `Expander::backlight()` now take the shared-bus `Guard` before any bus op and
  do nothing on a failed acquisition (no reopen, no read/write), matching
  `Gps::service()`. Previously both touched the bus with no lock, so a battery
  refresh could tear down the bus inside a GPS transaction in another task.
- **Touch rejects short reads.** `ftRead()` demands all requested bytes and fails
  on a partial packet instead of zero-filling it, so a truncated read can no longer
  surface as a phantom point near `(0,0)`.
- **`I2cBus::recover()` — return-value semantics changed.** It now returns exactly
  its observable postcondition: SDA sampled released (high) before the controller
  restart. Previously an already-released SDA returned `false`, and an unsuccessful
  recovery could return `true`; both are corrected. The header also now documents
  that the result is not a usability guarantee and that callers hold the lock. No
  in-tree caller depended on the old return value.

### Fixed (tooling — host-tested)
- **`c5flash`**: the read fallback clamps the final partial chunk (a non-`0x1000`
  read no longer over-reads), auto port-detection requires exactly one candidate
  (explicit `--port`, empty rejected), and the port path is resolved once per
  invocation and reused across chunks/retries.

### Added
- **`test/host/`** — native (no-hardware) mocked regression tests compiling the real
  drivers (20 assertions: locking, short-read, recovery, lock lifetime).
- **`tools/c5flash/test/`** — host-only mocked tests (10) for the read/port fixes.
- **`docs/CONSUMER-SETUP.md`** — the validated clean-consumer install (exact pin,
  delivered example scaffolding, resolved toolchain), separating the *compile-tested*
  non-display path from the still-*unvalidated* display path.

### Changed
- Version metadata aligned to `0.4.0` across `library.json`, `library.properties`,
  and `ESP32C5_BSP.h` (the header previously reported a stale `0.3.0`). Historical
  tags are retained.

## [0.3.2] — 2026-10-27

### Added
- **`examples/screen_flush`** — a standalone example that cycles the panel
  through full-screen white/black + R/G/B to clear **ST7796 image retention**
  (the faint ghost a long static, high-contrast screen can leave). Builds clean.

### Fixed
- **`tools/c5flash` now writes with explicit `--flash-mode dio --flash-freq 80m`.**
  Writing the bootloader segment with `--flash-mode keep` left a bad flash-mode
  byte in the image header; the ROM rejected it (`invalid header` →
  `ets_flash_boot` assert → watchdog reboot loop), so every build "booted" to a
  dark screen. Read the ROM/console log first when a flash won't boot.

### Changed (hardware findings)
- **Black panel after a USB reset — cause corrected.** It's the **NeoPixel on
  GPIO27** (the I²C SDA line) corrupting the shared bus on reset, cleared by
  disabling the NeoPixel (Marauder #1665) — *not* the power-rail dip the 0.3.1
  note guessed ("not a GPIO/signal conflict" was wrong for the reset case). The
  separate **USB→battery** panel-black stays its own symptom, still UNVERIFIED.
- README: the fleet-adoption list now includes **AWOKxDAG**, and the
  `screen_flush` example is listed.

## [0.3.1] — 2026-10-26

### Added (hardware findings)
- **NeoPixel is on GPIO27 — which is the I²C SDA line.** A Pancake-style LED on
  GPIO27 corrupts the shared bus (touch/expander/PMU/GPS); there is no usable
  NeoPixel on this board.
- **Backlight dimming runs off the CH32V006 expander PWM**, not a `TFT_BL` pin
  (`TFT_BL = -1`); a config that doesn't route through the expander gets no
  dimming.
- **AXP2101 fuel gauge readable over raw I²C** (chip ID `0x03`=`0x4A`, enable
  `0x18` bit 3, present `0x00` bit 3, percent `0xA4`) — no XPowersLib required,
  for toolchains that can't pull it in.
- **The USB link stalls on some data patterns during large transfers** — chunk
  the transfer (512 KB write segments; 64 KB → 4 KB reads with a shifted retry)
  to work around it. Now has a tool: **`tools/c5flash`**, an esptool-5.x wrapper
  that does the chunking + retry + verify on the ROM loader (`--no-stub`).
- **Panel can go black on a USB→battery switch or after a USB reset** (symptom
  verified; cause flagged UNVERIFIED — a power-rail dip is suspected). A `RST`
  clears it.
- **GPIO13/14 are the native USB D-/D+ lines** — never assign a UART there
  (documented in 0.3.x; a downstream port lost USB by inheriting a UART GPS on
  14/13).

### Changed
- README: corrected the Phase 6 status (published; fleet adoption underway), and
  the architecture listing now includes the `battery` board driver and the
  `battery` / `touch_int` / `reliability_reference` examples.

## [0.3.0]

- Published. GPS byte-sink / transport-only mode for the LC76G over I²C.
- Capstone (`reliability_reference`): Wi-Fi + GPS + touch + battery on the shared
  bus at once, hardware-verified.

## [0.1.0 – 0.2.0]

- Phases 0–5: scaffold + hardware-findings doc; board definition, pin map,
  ST7796 display, FT6336 touch; shared-I²C core (reopen / lock / timeout /
  recovery) + touch hardening; LC76G GPS over I²C (29 sats, HDOP ~0.6); AXP2101
  battery readout (89% / 4.15 V / charging); single-core task-pinning helper +
  chip utilities. All hardware-verified on the Waveshare ESP32-C5-Touch-LCD-3.5.
