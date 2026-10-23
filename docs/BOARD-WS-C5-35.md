# Board: Waveshare ESP32-C5-Touch-LCD-3.5

A 3.5" capacitive C5 board: ESP32-C5 (RISC-V, dual-band 2.4/5 GHz Wi-Fi + BLE),
ST7796 320×480 display, FT6336 touch, ES8311 audio, AXP2101 battery, all behind
a CH32V00x I/O expander on a shared I²C bus.

**This is the one fully-proven board in the BSP.** Every capability below is
hardware-verified; the cross-cutting findings (why the GPS is on I²C, the
shared-bus rules) are in [HARDWARE-FINDINGS.md](HARDWARE-FINDINGS.md).

## Build / flash

Console and flashing are over **native USB (USB-Serial-JTAG) at 115200** — there
is no separate USB-UART bridge. If the board won't auto-enter download mode:
**hold BOOT, tap RST, release BOOT**, then upload.

The build uses the `pioarduino` platform fork (the stock `espressif32` platform
does not carry the C5). Each example's `platformio.ini` pins it.

## Pin map (hardware-verified)

| Function | Detail |
|---|---|
| Display (ST7796) | SPI MOSI 7, SCLK 6, MISO 2, CS 8, DC 5; RST on the expander (`TFT_RST -1`) |
| Touch (FT6336) | I²C SDA 27 / SCL 26, addr `0x38`, 100 kHz |
| I/O expander (CH32V00x) | `0x24` on the touch bus — LCD reset, touch reset, backlight PWM |
| Audio (ES8311) | codec `0x18` on the touch bus → I²S BCK 23, WS 10, DOUT 25 (no MCLK) → NS4150B amp |
| Battery (AXP2101) | PMU on the touch bus; MX1.25 2-pin 3.7 V LiPo header (cell optional) |
| SD | CS 9 |
| GPS (LC76G) | over I²C on the **same** touch bus (SDA 27 / SCL 26) — command `0x50`, read `0x54`, wake `0x58`. The J3 header UART is not bonded to the chip; see HARDWARE-FINDINGS. |

## Board-specific notes

- **One shared I²C bus** (SDA 27 / SCL 26) carries the FT6336, ES8311, CH32V00x,
  AXP2101 **and** the GPS. Coexistence depends on the bus discipline documented in
  HARDWARE-FINDINGS: per-read bus re-open + retry for touch, STOP-based reads for
  the codec, and a short-settle, touch-gated, timeout-restoring GPS cycle.
- **No backlight GPIO / no LEDC.** Backlight PWM, LCD reset and touch reset all
  live on the `0x24` expander, brought up with the rail and both resets released
  together (resetting the touch chip while its rail is off leaves the FT6336
  unresponsive).
- **Display push clock** defaults to 80 MHz (a clean /2 of the 160 MHz source).
  The panel is rated 60 MHz, so this is above spec but holds on the units tested;
  the next clean step down is ~53 MHz (/3), since 60 is not a clean divide.
- **Battery**: read via XPowersLib. With no cell attached the AXP2101 still
  answers and reports USB/charge state; capacity reads as `NONE`.

## Onboard I²C address map (verified by bus scan)

All on the shared bus (SDA 27 / SCL 26):

| Addr | Device |
|---|---|
| `0x18` | ES8311 audio codec |
| `0x24` | CH32V00x I/O expander |
| `0x34` | AXP2101 PMU |
| `0x38` | FT6336 touch |
| `0x51` | RTC |
| `0x6B` | IMU |
| `0x70` | (onboard sensor) |
| `0x50` / `0x54` / `0x58` | LC76G GPS — **only when an I²C GPS is wired** |

A boot scan that shows the onboard set but no `0x50` means the GPS is the
problem (SDA/SCL swapped, wrong pins, not in I²C mode, or unpowered), not the
bus. Note `0x51` is the RTC, not the GPS.

## Verified in this BSP (on hardware)

Display (ST7796) · touch (FT6336) · I/O expander (CH32V00x) · GPS (LC76G over
I²C) · single-core task pinning · USB-JTAG console · battery (AXP2101, read via
XPowersLib with a cell on the MX1.25 header). Wi-Fi (2.4 + 5 GHz), BLE, SD and audio
are proven on the fleet and documented in HARDWARE-FINDINGS.md, but are not yet
wrapped as BSP code here. See the verification ledger in HARDWARE-FINDINGS.md.
