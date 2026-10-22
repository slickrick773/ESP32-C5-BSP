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

| Layer | What | Proof |
|---|---|---|
| Chip core (any C5) | single-core pinning, band-switch ordering, USB-JTAG console, 5 GHz channel enumeration | findings hardware-verified across the fleet; code migration in progress |
| Board (3.5) | ST7796, FT6336, AXP2101, CH32V00x, LC76G GPS-over-I²C, shared-bus coordination | findings hardware-verified; drivers landing phase by phase, each re-verified on hardware here |

This repository is being built up one phase at a time, and **a capability is only
written down as "supported" once it has been run on a real board.** See
[docs/HARDWARE-FINDINGS.md](docs/HARDWARE-FINDINGS.md) for the three levels of
proof (silicon ≠ IDF-exposed ≠ works-on-hardware) and every claim's tag.

## Supported boards

- **Waveshare ESP32-C5-Touch-LCD-3.5** — the one fully-proven board. See
  [docs/BOARD-WS-C5-35.md](docs/BOARD-WS-C5-35.md).

Other C5 boards can be added against the chip core + board-template structure —
but none is claimed supported until someone runs it on that hardware.

## Architecture

```
src/
  chip/     C5 chip-level, reusable across any C5 board
            (single-core helper, band-switch, USB-JTAG console, 5 GHz quirks)
  board/    board-specific drivers; one proven board today (the 3.5)
            ws_c5_35/  ST7796 · FT6336 · AXP2101 · CH32V00x · LC76G-I²C · bus lock
examples/   runnable sketches, one per phase
docs/       hardware findings and per-board notes
```

Built as an Arduino library first (the proven code is Arduino/C++). The hardware
core — I²C discipline, GPS protocol, pin maps, findings — is kept structured so
an ESP-IDF component can wrap it later. Only one framework is built now.

## Toolchain

Built with PlatformIO using the `pioarduino` platform fork (the C5 needs it).
See each example's `platformio.ini`. Console is USB-Serial-JTAG at 115200;
download mode is **hold BOOT, tap RST, release BOOT**.

## License

MIT. See [LICENSE](LICENSE).
