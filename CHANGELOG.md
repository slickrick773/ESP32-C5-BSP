# Changelog

All notable changes to this board-support package. A capability is only listed
as hardware-verified once it has been run on a real board; dates are observation
dates. See [docs/HARDWARE-FINDINGS.md](docs/HARDWARE-FINDINGS.md) for the full
evidence and claim tags.

## Unreleased

### Repository

- **Registry publishing from a clean checkout.** A workflow publishes each
  GitHub release to the PlatformIO registry. It packs a clean export of the
  release tag, checks the package file-for-file against the tag, scans it, and
  only then uploads, behind an approval step.
  `tools/release/check_package.sh` runs the same checks locally.

## [0.5.1] — 2026-11-10

Documentation and repository housekeeping. No code changes.

### Documentation

- **Clean startup: wait before lighting.** The 3.5-inch recipe now waits 150 ms
  between the last draw of the first frame and turning the backlight on. One
  firmware port to this board still showed static or the old picture for a few
  frames without it; with it, slow-motion video of a reset and a cold boot was
  clean there and in five further ports. See
  [Clean startup](docs/BOARD-WS-C5-35.md#clean-startup). No code changes.
- **USB-reset start-up crash documented.** On some firmware images a reset
  sent over USB crashes PSRAM start-up once and the board then boots normally;
  the RST button and power-on were not affected in testing. Position-dependent,
  mechanism unknown. See
  [HARDWARE-FINDINGS](docs/HARDWARE-FINDINGS.md#a-reset-sent-over-usb-can-crash-psram-start-up-on-some-images).

### Repository

- **Contributing guide and issue forms.** [CONTRIBUTING](CONTRIBUTING.md) sets
  out the evidence rule for hardware claims; issue forms for bug reports and
  hardware results ask for the board, the reset type and a boot log.
- **PlatformIO package manifest** now lists exactly what ships (`export` in
  `library.json`), so the library can be published to the PlatformIO registry.

## [0.5.0] — 2026-11-10

The 2.8-inch board reaches parity with the 3.5-inch, both boards gain audio in
both directions, a clock, an IMU, a temperature/humidity sensor and a
whole-board check, and every earlier hardware result was re-run before release.

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
- `touch_audio` combined display, touch, speaker and microphone sample. The
  tone plays from a background task. On 2026-11-10 the owner heard it clean
  while scrolling the list. It is rendered at startup: generating it while
  playing was slower than real time on this chip and made it pulse.
- `audio_selftest_35`: the board plays a tone and checks it on its own
  microphone using I2S in both directions, then prints PASS or FAIL. On
  2026-11-10 it passed three of three hardware runs and reported FAIL with the
  speaker silenced.
- 3.5-inch temperature/humidity driver (`WsC5_35::Env`, SHTC3 at `0x70`) with
  host tests, read out by `onboard_35`. Hardware readings on 2026-11-10 were
  plausible with valid checksums; accuracy is untested.
- `Battery::setBackupCharge()` / `backupCharge()` for the PMU's backup-pin
  charger, for boards modified with a rechargeable clock cell.
- `audio_35`, `mic_35`, `audio_selftest_35`, `sd_35`, `onboard_35` and
  `touch_audio` added to the CI example build matrix.
- Optional dark startup via `Board::begin(0)` / `Expander::begin(0)`, preserving
  the full-brightness default and existing reset timing.
- Reusable ESP32-C5 SPI2 reset plus one-time per-board `prepareSharedSpi()`,
  with display and SD deselected before either device uses the shared bus.
  Repeated board initialization does not reset an already prepared SPI bus.
- Additive classified touch sampling (`Contact`, `Released`, `Unavailable`,
  `Error`) while preserving `read()` and `poll()`.
- CI coverage for both 2.8-inch examples, native driver tests, flasher tests,
  and isolated consumer builds.

### Added (2.8-inch, 2026-11-10)
- Microphone capture for the 2.8-inch board (`Audio::beginRecording()` and
  friends) with host tests, and the `mic_28` and `audio_selftest_28` examples.
- GPS on the 2.8-inch board: `gps_28`, using the shared LC76G driver with its
  new option to never address `0x58`, which the touch controller owns there.
  Hardware-verified with a fix and with touch working alongside.
- Clock, IMU and temperature/humidity sensor on the 2.8-inch board through the
  shared drivers, with the `onboard_28` example.
- `sd_28` and `touch_audio_28` examples.
- `board_check_35` and `board_check_28`: every onboard device in one run,
  printed as a PASS/FAIL table.
- `tools/bench/serial_log.py`: reset a board and print its serial output with
  timestamps.
- All new examples added to the CI build matrix.

### Changed
- The PCF85063, QMI8658, SHTC3 and LC76G drivers moved to `src/part/` and are
  shared by both boards. The `WsC5_35::Rtc`, `Imu`, `Env` and `Gps` names are
  unchanged; `WsC5_28::` equivalents are new. `Gps::begin()` gained an optional
  argument and keeps its previous behaviour by default.
- The 3.5-inch speaker test and touch demo now play 1 kHz at PCM peak 4500
  instead of 440 Hz at peak 12000. The old level was clipped; the new one is
  undistorted and at a pitch the small speaker reproduces well.
- README status table: audio, onboard devices and microSD are now phases 7, 8
  and 9; "Sample" is kept for the two demonstrations that add no driver.
- README status: the 2.8-inch board now has its own table.

### Validation (2026-11-10)
- Earlier results were re-run rather than carried forward: battery readout,
  the single-core helper, interrupt-gated touch, and Wi-Fi, GPS and touch
  running together all held on hardware.
- Startup behaviour was measured from video. Static on cold boot and the
  previous frame reappearing after RST are both the panel's memory showing
  before the first frame is written, and starting dark removes both. About one
  second of grey glow before any application code runs remains and cannot be
  removed in firmware. The board notes gain a "Clean startup" section.

- The PSRAM error line printed at every boot was traced to its source: a
  pre-v1.2 chip workaround in the pinned core that runs before PSRAM is set up.
  Expected to be harmless on the v1.2 boards tested; not something the BSP can
  change.
- The startup probes are now tracked as labelled diagnostics, with their host
  test in the suite and `exio_probe` in the CI build matrix.

### Findings (2026-11-10, 3.5-inch)
- The real-time clock has no backup cell. It lost its time with USB removed
  and no LiPo fitted, and kept it through a 10-second unplug on a second
  board with a LiPo.
- The original `audio_35` listening level (440 Hz, PCM peak 12000, codec
  volume `0xE0`) was clipped, and the small speaker barely reproduces 440 Hz.
  The default was changed to a clean 1 kHz tone; see Changed above.
- The microSD card worked while the display redrew on the shared SPI bus in a
  bench build.
- The devices at I²C `0x70` and `0x6E` are identified as the SHTC3 sensor and
  the BF3901 camera's control port.

### Fixed
- `c5flash write` now exits unsuccessfully whenever verification reports a
  textual mismatch, even if the transport subprocess itself returned zero.
- Overview documentation now matches the verified 2.8-inch battery evidence.

Release validation: the host driver suite (including the bootloader-I2C fault
tests), 13 flasher tests, CI builds of all 26 examples and the two clean
consumer builds. Hardware results are listed entry by entry above and in
[docs/HARDWARE-FINDINGS.md](docs/HARDWARE-FINDINGS.md); what was not exercised
is listed under Known limitations in the README.

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
[its limits and recovery plan](tools/startup_probe/README.md). Re-measured from
video on 2026-11-10: see Validation above and the board notes' Clean startup
section for what starting dark does remove.

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
