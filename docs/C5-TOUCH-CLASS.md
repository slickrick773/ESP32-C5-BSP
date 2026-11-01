# The ESP32-C5 touch-display class (Pancake ↔ Waveshare 3.5 ↔ Waveshare 2.8)

Ports slot across these boards cleanly because they share a core. This page
names that core, so the BSP reads as the reference for the **whole C5
touch-display class** — not one board — and spells out exactly what differs so
a driver written for one board lands on the other without guesswork.

> **Proof scope.** Everything the BSP *claims* is hardware-verified on the
> **Waveshare ESP32-C5-Touch-LCD-3.5** (see [BOARD-WS-C5-35.md](BOARD-WS-C5-35.md)
> and the ledger in [HARDWARE-FINDINGS.md](HARDWARE-FINDINGS.md)). The **Waveshare
> ESP32-C5-Touch-LCD-2.8** (see [BOARD-WS-C5-28.md](BOARD-WS-C5-28.md)) is
> *ported, pending BSP hardware-verification* — its board layer is mirrored from
> a proven port but **not yet flashed from this BSP.** The **Marauder Pancake C5**
> is the community twin, cross-referenced from the scene's shipping configs —
> **not run on hardware by this BSP.** Treat the 3.5" column as "what we verified,"
> and the 2.8" and Pancake columns as "what to expect."

## The shared core (why ports are drop-ins)

| | Shared across the class |
|---|---|
| MCU | **ESP32-C5** — RISC-V, **single-core**, dual-band 2.4/5 GHz Wi-Fi + BLE |
| RAM | external **PSRAM** |
| Console | native **USB-Serial-JTAG** (no USB-UART bridge); GPIO13/14 are the USB D-/D+ lines |
| Display | an **SPI panel**, native-portrait, landscaped at rotation 1 (ST7796 on the 3.5" & Pancake; **ST7789** on the 2.8") |
| Touch | an **I²C capacitive controller** on a shared bus (FT6336 `0x38` on the 3.5" & Pancake; **CST3530 `0x58`** on the 2.8") |
| Bring-up | panel/touch resets (and, where present, power/backlight) behind peripherals, not bare GPIO; a shared I²C bus with the C5's per-read quirks |

The display and touch *controllers* are not identical across the whole class —
the Waveshare 2.8" swaps an ST7789 + CST3530 for the 3.5"/Pancake's ST7796 +
FT6336 — but the **shape** is: single-core pacing (no blocking I/O in an
ISR/critical section), an SPI panel, and an I²C capacitive touch on a shared
bus. A driver written to that shape, with the controller specifics behind a thin
board layer, is most of the way to running on any board in the class. That is
the whole reason the fleet's ports (Marauder, GhostESP, AWOKxDAG, Picoware)
carry these boards side by side.

## What differs

Within the Waveshare family the deltas are the display/touch controller plus
wiring; across to the Pancake they are peripheral and wiring.

| Area | Waveshare C5-Touch-LCD-3.5 | Waveshare C5-Touch-LCD-2.8 | Marauder Pancake C5 |
|---|---|---|---|
| Display | **ST7796** 320×480, inversion on | **ST7789** 240×320, inversion on, **MSB-first RAMCTRL** (`0xB0 00 E0`) byte-order fix | ST7796 320×480 |
| Touch | **FT6336** `0x38`, single-byte STOP read, per-read bus re-open | **CST3530** `0x58`, 4-byte-reg repeated-START 9-byte packet + ack, **no** per-read re-open | FT6336 `0x38` |
| Display SPI pins | MISO 2, CS 8, DC 5 | **MISO 8, CS 10, DC 9** | board-dependent |
| Touch I²C bus | SDA 27 / SCL 26 | **SDA 0 / SCL 1** | board-dependent |
| LCD / touch reset / backlight | behind a **CH32V006 I/O expander** (`0x24`) — no `TFT_RST`, no `TFT_BL`, no LEDC | same expander `0x24`, but **no EXIO5 panel rail** (resets only) | **direct GPIO** |
| Power / battery | **AXP2101 PMU** (raw I²C, or XPowersLib) | **not yet confirmed** (schematic check pending; may be simpler) | **MAX17048** fuel gauge |
| GPS | **LC76G over I²C** on the shared touch bus (`0x50`/`0x54`/`0x58`) | LC76G planned on SDA 0 / SCL 1 — **`0x58` clashes with CST3530 touch**, resolve first | none |
| Storage | microSD (CS 9) · **32 MB** flash | microSD (**CS 23**) | — |
| RGB LED | **unusable** — "NeoPixel" on **GPIO27 is SDA** (black-panel-on-reset trap; Marauder #1665) | GPIO27 is not the bus here; nothing defined on it | usable RGB |
| Touch INT gate | **GPIO3** — gating reads on INT kills phantom/offset taps | **GPIO5** (set as input; CST3530 INT mode not yet characterised) | board-dependent |
| Onboard extras | ES8311 codec (`0x18`), RTC (`0x51`), IMU (`0x6B`) on the shared bus | **not yet confirmed** (schematic check pending) | — |

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

### Writing for the Waveshare 2.8" when you have 3.5" code

1. **Swap the controllers, keep the shape.** The panel is ST7789 (240×320, not
   ST7796 320×480) and touch is CST3530 at `0x58` (not FT6336 at `0x38`). The
   board layer (`src/board/ws_c5_28`) already encapsulates both; a consumer calls
   the same `begin()` / `Touch::read()` / `Touch::poll()` surface.
2. **Get the ST7789 byte order right.** RAMCTRL `0xB0` must be `0x00 0xE0`
   (MSB-first) for a library that pushes MSB-first 16-bit pixels (Arduino_GFX);
   vendor LSB buffers use `0x00 0xE8`. Wrong byte = garbled colour.
3. **Different bus and SPI pins.** Touch/expander I²C is SDA 0 / SCL 1 (not
   27/26); display MISO/CS/DC are 8/10/9 (not 2/8/5); SD CS is 23 (not 9).
4. **The CST3530 read is not the FT6336 read.** Four-byte register `0xD0070000`
   with a repeated START, a 9-byte packet, a STOP ack to `0xD00002AB`, and **no**
   per-read bus re-open (the 3.5"'s FT6336 workaround is not needed here).
5. **Mind the GPS clash.** On the 2.8" `0x58` is touch — the LC76G's `0x58`
   address collides; resolve before adding GPS.

## See also

- [BOARD-WS-C5-35.md](BOARD-WS-C5-35.md) — the proven board, full pin map and address scan
- [BOARD-WS-C5-28.md](BOARD-WS-C5-28.md) — the 2.8" twin (ported, pending BSP hw-check): ST7789 + CST3530 deltas
- [HARDWARE-FINDINGS.md](HARDWARE-FINDINGS.md) — the shared-bus rules, the GPIO27/GPS/USB traps, the verification ledger
