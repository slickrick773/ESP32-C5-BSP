# Board: Waveshare ESP32-C5-Touch-LCD-2.8

A 2.8" capacitive C5 board: the same ESP32-C5 (RISC-V, dual-band 2.4/5 GHz
Wi-Fi + BLE) as the 3.5", with an **ST7789 240×320** display and a **CST3530**
touch controller, behind the same **CH32V006 I/O expander** on a shared I²C
bus — but wired differently (different display/touch controllers, a different
I²C bus, a different SPI pinout).

> **Status: ported, pending BSP hardware-verification.** Unlike the 3.5", the
> BSP's own 2.8" drivers have **not yet been run on hardware from this library.**
> The pin map, reset timing, ST7789 init and CST3530 read path are **mirrored
> from Rick's working umbravox 2.8" port**, which *is* hardware-accepted on the
> physical 2.8" board — so this is a faithful port of proven code, not a guess.
> It is promoted to "supported", phase by phase, only after `hello_board_28` is
> flashed from this BSP and confirmed on the real 2.8" board. See the governing
> law in [HARDWARE-FINDINGS.md](HARDWARE-FINDINGS.md).

## Build / flash

Console and flashing are over **native USB (USB-Serial-JTAG) at 115200** — there
is no separate USB-UART bridge. If the board won't auto-enter download mode:
**hold BOOT, tap RST, release BOOT**, then upload.

The example (`examples/hello_board_28`) uses the `pioarduino` platform fork and
drives the ST7789 with **Arduino_GFX** (`moononournation/GFX Library for Arduino
@ 1.5.5`, the version the umbravox 2.8" port validates this panel with). Build
with Python 3.10–3.13 (the platform refuses 3.14+).

## Pin map (mirrored from the proven umbravox 2.8" port — pending BSP hw-check)

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
  re-open each read; the CST3530's repeated-START read is proven to work *without*
  it on the umbravox 2.8" board, so this path omits it. (To be re-confirmed on BSP
  hardware.)
- **No sensitivity register.** The CST3530 has none documented, so there is no
  threshold to set (the FT6336 driver sets one).

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

**None yet.** First milestone: `hello_board_28` (ST7789 display bring-up +
CST3530 touch) flashed from this BSP and confirmed on the real 2.8" board. Until
then every row above is "ported, pending BSP hardware-verification."
