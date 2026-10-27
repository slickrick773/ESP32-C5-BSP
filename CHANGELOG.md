# Changelog

All notable changes to this board-support package. A capability is only listed
as hardware-verified once it has been run on a real board; dates are observation
dates. See [docs/HARDWARE-FINDINGS.md](docs/HARDWARE-FINDINGS.md) for the full
evidence and claim tags.

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
