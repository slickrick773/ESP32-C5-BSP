# Board: Waveshare ESP32-C5-Touch-LCD-2.8

A 2.8" capacitive C5 board: the same ESP32-C5 (RISC-V, dual-band 2.4/5 GHz
Wi-Fi + BLE) as the 3.5", with an **ST7789 240×320** display and a **CST3530**
touch controller, behind the same **CH32V006 I/O expander** on a shared I²C
bus — but wired differently (different display/touch controllers, a different
I²C bus, a different SPI pinout).

> **Status: hardware-verified (display + touch), 2026-10-31.** `hello_board_28`
> was flashed from this BSP onto a physical Waveshare ESP32-C5-Touch-LCD-2.8 and
> confirmed on the real board: the panel lights and renders correctly (readable,
> landscape, right colors — no inversion/mirror/garble), and CST3530 touch
> registers taps with sane coordinates and tracks a continuous drag smoothly.
> The pin map, reset timing, ST7789 init and CST3530 read path were mirrored from
> Rick's working umbravox 2.8" port and now **run from this library**, so the
> display + touch phase is legitimately "supported" under the BSP's own law.
> **Battery, audio and GPS remain later, unverified phases** (see the notes
> below). See the governing law in [HARDWARE-FINDINGS.md](HARDWARE-FINDINGS.md).

## Build / flash

Console and flashing are over **native USB (USB-Serial-JTAG) at 115200** — there
is no separate USB-UART bridge. If the board won't auto-enter download mode:
**hold BOOT, tap RST, release BOOT**, then upload.

The example (`examples/hello_board_28`) uses the `pioarduino` platform fork and
drives the ST7789 with **Arduino_GFX** (`moononournation/GFX Library for Arduino
@ 1.6.9` — the version umbravox's `^1.5.5` resolves to and builds with; 1.5.5
does not compile against this C5 Arduino core). Build with Python 3.10–3.13 (the
platform refuses 3.14+). Flashed hands-free here with `tools/c5flash`.

## Pin map (hardware-verified on the real 2.8" board, 2026-10-31)

| Function | Detail | Delta vs 3.5" |
|---|---|---|
| Display (ST7789) | SPI SCLK 6, MOSI 7, **MISO 8**, **CS 10**, **DC 9**; RST on the expander (−1) | MISO 2→8, CS 8→10, DC 5→9; ST7796→ST7789 |
| Touch (CST3530) | I²C **SDA 0 / SCL 1**, addr **`0x58`**, INT **GPIO5**, 100 kHz | FT6336 `0x38`→CST3530 `0x58`; bus 27/26→0/1 |
| I/O expander (CH32V006) | `0x24` on the touch bus — LCD reset (EXIO1), touch reset (EXIO0), backlight PWM | **no EXIO5 panel rail** on this board |
| SD | CS **23** | 9→23 |
| GPS (LC76G) | planned over I²C on the SDA 0 / SCL 1 bus — **address clash to resolve, see below** | same module as 3.5" |

## Display: the ST7789 byte-order fix

The panel is native 240×320 portrait; `ROTATION 1` lands it 320×240 landscape.
Two panel facts matter and are both in the proven init (`ws28Init[]`, carried in
`examples/hello_board_28/src/ws_c5_28_display.h`; the register table is documented
driver-neutrally in `src/board/ws_c5_28/ws_c5_28_pins.h`):

- **Inversion ON** (`0x21`) — the panel wants it (Arduino_GFX `IPS=true` matches).
- **RAMCTRL (`0xB0`) = `0x00 0xE0`** — **MSB-first**, the byte-order fix for a
  graphics library that pushes MSB-first 16-bit pixels (Arduino_GFX). Vendor raw
  buffers use `0x00 0xE8` (LSB-first). Getting this byte wrong is the classic
  swapped/garbled-colour symptom on this panel.

There is **no hardware reset line** to pulse (RST −1); the init opens with a
120 ms settle and sends **no `0x01` soft-reset** — the expander's EXIO1 pulse is
the panel reset. The driver choice (Arduino_GFX) is confined to the example; the
BSP board layer (`src/board/ws_c5_28`) stays free of any graphics library, so a
TFT_eSPI or ESP-IDF consumer can reuse it. A TFT_eSPI variant is a **separate,
later-validated** example.

## Touch: CST3530, not FT6336

The CST3530 is read very differently from the 3.5"'s FT6336:

- **Four-byte register + repeated START.** A read addresses register
  `0xD0070000` with a repeated START (`endTransmission(false)`), pulls a 9-byte
  packet, then **acknowledges** the packet with a STOP write to `0xD00002AB`.
- **Packet decode** (register `0xD0070000`): count = `p[3]&0x0F` (0–5),
  finger-down = `p[8]&0xF0`, `nx = ((p[7]&0x0F)<<8)|p[4]`,
  `ny = ((p[7]&0xF0)<<4)|p[5]`, bounds 240×320.
- **Landscape map** (rotation 1): `sx = ny`, `sy = 239 − nx` (`Touch::toLandscape`).
- **No per-read bus re-open.** The 3.5"'s FT6336 needs the C5 `Wire.end()/begin()`
  re-open each read; the CST3530's repeated-START read works *without* it —
  confirmed on the real 2.8" board from this BSP (reads land every ~8 ms with
  effectively zero dropouts).
- **No sensitivity register.** The CST3530 has none documented, so there is no
  threshold to set (the FT6336 driver sets one).
- **Tighter `poll()` cadence than the 3.5".** On-board timing showed the CST3530
  takes a fresh read every ~8 ms with ~0 dropouts and reports new coordinates
  ~every 15 ms, so `poll()` defaults to `minReadIntervalMs=8` / `graceMs=60`
  (vs the FT6336's 90/12). A looser cadence makes a fast drag read as angular
  "connect-the-dots"; at 8 ms the stroke is smooth (verified with Rick on hw).

The host test `test/host/test_touch_28.cpp` exercises this decode (short-read
rejection, a valid contact, finger-up, out-of-range, and the landscape map)
against the real driver.

## Onboard I²C: the GPS address clash to resolve

On the 3.5", the LC76G GPS answers at `0x50` / `0x54` / `0x58` on the shared bus.
On the 2.8", **`0x58` is the CST3530 touch controller** — so the GPS's `0x58`
("wake") address **clashes with touch.** Before the GPS phase, confirm against
hardware whether the LC76G on this board uses a different address set, or whether
the clash forces the GPS onto a different arrangement. This is flagged here so
the GPS phase starts from the known conflict, not a surprise.

Power/audio ICs are **not yet confirmed** against the v1.2.0 schematic: the
umbravox 2.8" config wires only display + touch + SD. Rick has the 2.8" with its
included battery + speaker and an LC76G; battery, audio and GPS are **later
phases**, each to start by confirming the actual charger/amp ICs on the
Waveshare ESP32-C5-Touch-LCD-2.8 v1.2.0 schematic (they may be simpler than the
3.5"'s AXP2101/ES8311, or the same).

## Verified in this BSP (on hardware)

**Display + touch — hardware-verified 2026-10-31.** `hello_board_28` was built
from this BSP, flashed hands-free with `tools/c5flash` (verify OK), and run on a
physical Waveshare ESP32-C5-Touch-LCD-2.8:

- **CH32V006 expander** `0x24` brings the panel up on the SDA 0 / SCL 1 bus
  (serial: `I/O expander 0x24 ready`).
- **ST7789 display** inits and renders at 320×240 landscape — lit, readable,
  correct colors (the `0xB0 00 E0` MSB-first RAMCTRL byte order is right: no
  inversion/mirror/garble), confirmed on glass.
- **CST3530 touch** `0x58` ACKs (serial: `CST3530 on I2C 0x58 (OK)`), registers
  taps with sane in-range coordinates, and tracks a continuous drag smoothly at
  the 8 ms `poll()` cadence.

**Not yet verified (later phases):** battery, audio and GPS — their ICs are not
yet confirmed against the v1.2.0 schematic, and the LC76G `0x58`/touch clash
(above) is unresolved. Those remain "ported/planned, pending hardware."
