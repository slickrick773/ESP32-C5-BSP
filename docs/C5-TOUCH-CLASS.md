# The ESP32-C5 touch-display class (Pancake ↔ Waveshare)

Ports slot across these boards cleanly because they share a core. This page
names that core, so the BSP reads as the reference for the **whole C5
touch-display class** — not one board — and spells out exactly what differs so
a driver written for one board lands on the other without guesswork.

> **Proof scope.** Everything the BSP *claims* is hardware-verified on the
> **Waveshare ESP32-C5-Touch-LCD-3.5** (see [BOARD-WS-C5-35.md](BOARD-WS-C5-35.md)
> and the ledger in [HARDWARE-FINDINGS.md](HARDWARE-FINDINGS.md)). The **Marauder
> Pancake C5** is the community twin: same silicon, panel and touch, cross-referenced
> here from the scene's shipping configs — **not run on hardware by this BSP.**
> Treat the Pancake column as "what to expect," not "what we verified."

## The shared core (why ports are drop-ins)

| | Shared across the class |
|---|---|
| MCU | **ESP32-C5** — RISC-V, **single-core**, dual-band 2.4/5 GHz Wi-Fi + BLE |
| Display | **ST7796, 320×480, SPI** |
| Touch | **FT6336 capacitive, I²C, addr `0x38`** |
| RAM | external **PSRAM** |
| Console | native **USB-Serial-JTAG** (no USB-UART bridge); GPIO13/14 are the USB D-/D+ lines |

A driver that talks ST7796-over-SPI and FT6336-at-`0x38`, and that respects the
single-core pacing (no blocking I/O in an ISR/critical section), is already
most of the way to running on any board in the class. That is the whole reason
the fleet's ports (Marauder, GhostESP, AWOKxDAG, Picoware) carry both boards
side by side.

## What differs

The deltas are peripheral and wiring, not the display/touch core.

| Area | Waveshare C5-Touch-LCD-3.5 | Marauder Pancake C5 |
|---|---|---|
| LCD reset / touch reset / backlight | behind a **CH32V006 I/O expander** (`0x24` on the touch I²C bus) — no `TFT_RST`, no `TFT_BL`, no LEDC | **direct GPIO** |
| Power / battery | **AXP2101 PMU** (raw I²C, or XPowersLib) | **MAX17048** fuel gauge |
| GPS | **LC76G over I²C** on the shared touch bus (`0x50`/`0x54`/`0x58`); the header UART is not bonded — see HARDWARE-FINDINGS | none |
| Storage | microSD (CS 9) · **32 MB** flash | — |
| RGB LED | **unusable** — the "NeoPixel" on **GPIO27 is the I²C SDA line**; driving it is the black-panel-on-reset trap (disable it; Marauder #1665) | usable RGB |
| Touch INT gate | **GPIO3** — gating reads on INT kills phantom/offset taps | board-dependent |
| Onboard extras | ES8311 audio codec (`0x18`), RTC (`0x51`), IMU (`0x6B`) all on the shared bus | — |

### Writing for the Waveshare when you have Pancake code

1. **Don't drive reset/backlight on GPIO.** Route LCD reset, touch reset and
   backlight through the `0x24` expander; set `TFT_RST -1`, no `TFT_BL`. Bring
   the rail up and release both resets together — resetting the touch chip while
   its rail is off leaves the FT6336 dead.
2. **Don't touch GPIO27 as an LED.** It is SDA. Any NeoPixel/RGB init there
   corrupts the bus and blanks the panel on reset.
3. **Battery is AXP2101, not MAX17048** — different address, different registers.
   Raw I²C works; no XPowersLib required.
4. **One shared I²C bus** carries touch, codec, expander, PMU **and** GPS. Use
   the bus discipline in HARDWARE-FINDINGS (per-read reopen + retry for touch,
   STOP-based codec reads, touch-gated GPS cycle) — it is not optional once the
   GPS is on the bus.

## Where the class shows up

Projects that already ship a Pancake target are the natural home for Waveshare
support; several carry both today.

| Project | Pancake | Waveshare C5-3.5 |
|---|---|---|
| **Marauder** (justcallmekoko) | "Pancake" target | `WS_C5_35` (H4W9's TFT_eSPI fork builds the class) |
| **GhostESP** (Revival) | `sdkconfig.Pancake` | PR #433 |
| **AWOKxDAG** (dagnazty) | `PANCAKE_C5` | `AWOK_WAVESHARE_C5_35` (PR #11) |
| **Picoware** (jblanked) | Pancake board (MicroPython) | in progress |
| Marauder forks | inherit the board list | — |

The display layer is converging: **H4W9's TFT_eSPI fork** is the common C5
display code across Marauder, and H4W9 maintains the Pancake while this BSP is
the Waveshare reference. Same family, split ownership.

## See also

- [BOARD-WS-C5-35.md](BOARD-WS-C5-35.md) — the proven board, full pin map and address scan
- [HARDWARE-FINDINGS.md](HARDWARE-FINDINGS.md) — the shared-bus rules, the GPIO27/GPS/USB traps, the verification ledger
