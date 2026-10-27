# ESP32-C5-BSP

**The thing that makes the ESP32-C5 stable.**

Not a radio toolkit, not a flashy demo. This is a board-support package whose
entire value is *reproducibility*: the hardware problems that a fleet of C5
firmware ports already solved — shared-I²C discipline, GPS over I²C when the
header UART turns out to be dead, display/touch/battery/audio bring-up, the
dual-band band-switch ordering trap — packaged once so the next C5 author
doesn't re-solve them.

5 GHz scanning and sniffing is a crowded space. **This package does not compete
there.** It competes on reliability.

## Status

Built one phase at a time, and **a capability is only called "supported" once it
has run on a real board.** See [docs/HARDWARE-FINDINGS.md](docs/HARDWARE-FINDINGS.md)
for the three levels of proof (silicon ≠ IDF-exposed ≠ works-on-hardware) and
every claim's tag.

| Phase | Scope | Status |
|---|---|---|
| 0 | Scaffold + hardware-findings doc | ✅ done |
| 1 | Board definition, pin map, ST7796 display, FT6336 touch | ✅ hardware-verified |
| 2 | Shared-I²C core (reopen / lock / timeout / recovery) + touch hardening | ✅ hardware-verified |
| 3 | LC76G GPS over I²C | ✅ hardware-verified (29 sats, HDOP ~0.6) |
| 4 | AXP2101 battery readout | ✅ hardware-verified (89% / 4.15 V / charging) |
| 5 | Single-core task-pinning helper + chip utilities | ✅ hardware-verified |
| 6 | Wi-Fi + GPS + touch capstone (`reliability_reference`) | ✅ hardware-verified |

Published (`v0.3.x`) and adopted across the C5 fleet (Marauder, PorkChop, Bruce,
SquachWatch, GhostESP, AWOKxDAG, Picoware) — each port self-contained, no BSP dependency upstream.

Chip-level findings that are **hardware-verified on the fleet and documented
here, but not yet wrapped as BSP code**: the dual-band `set_band_mode()`-after-
`start()` ordering, 5 GHz channel enumeration, and ES8311 audio bring-up. They
live in the findings doc until a verified helper lands.

Board-level traps are documented there too: the NeoPixel pin that is really the
I²C SDA line (GPIO27), the backlight that runs off the expander (no `TFT_BL`
pin), the AXP2101 fuel gauge read over raw I²C (no XPowersLib needed), the
GPIO13/14 = native-USB trap, and the USB link stalling on large transfers.

## Supported boards

- **Waveshare ESP32-C5-Touch-LCD-3.5** — the one fully-proven board. See
  [docs/BOARD-WS-C5-35.md](docs/BOARD-WS-C5-35.md).

Other C5 boards can be added against the chip core + board-template structure —
but none is claimed supported until someone runs it on that hardware.

The Waveshare is one member of a wider **C5 touch-display class** (ESP32-C5 +
ST7796 320×480 + FT6336) that the fleet's ports already carry side by side. What
that core is, and exactly how the twin **Marauder Pancake C5** differs, is in
[docs/C5-TOUCH-CLASS.md](docs/C5-TOUCH-CLASS.md) — so a driver written for one
board lands on the other.

## Architecture

```
src/
  chip/     C5 chip-level, reusable across any C5 board
            esp32c5_i2c_bus   shared-bus discipline: reopen, non-blocking lock,
                              timeout guard, 9-clock recovery
            esp32c5_tasks     single-core-aware task pinning + chip info
  board/    board-specific drivers; one proven board today (the 3.5)
            ws_c5_35/  pins · expander (CH32V006) · touch (FT6336) ·
                       gps (LC76G over I²C) · battery (AXP2101) ·
                       board begin()/name()
examples/   one standalone PlatformIO project per capability
            hello_board            display + touch
            gps_i2c                LC76G GPS over I²C (fix + sat count)
            battery                AXP2101 readout (%, voltage, charge state)
            touch_int              INT-gated touch (skips reads when no finger)
            single_core            single-core task pinning
            reliability_reference  capstone: Wi-Fi + GPS + touch + battery
            screen_flush           clears ST7796 image retention (colour cycle)
docs/       hardware findings, per-board notes, build guide
tools/      c5flash   ROM-loader flash read/write that chunks around the
                      C5 USB large-transfer stall
```

Built as an Arduino library first (the proven code is Arduino/C++). The hardware
core — I²C discipline, GPS protocol, pin maps, findings — is kept free of any
graphics library (drivers use only `Wire`) so an ESP-IDF component can wrap it
later. Only one framework is built now.

## Toolchain

Built with PlatformIO using the `pioarduino` platform fork (the C5 needs it).
See each example's `platformio.ini` and [docs/BUILD.md](docs/BUILD.md) — note
the **Python 3.10–3.13** requirement (the platform refuses 3.14+). Console is
USB-Serial-JTAG at 115200; download mode is **hold BOOT, tap RST, release
BOOT**.

## License

MIT. See [LICENSE](LICENSE).
