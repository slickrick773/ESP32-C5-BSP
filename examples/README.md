# Examples

One runnable sketch per phase, each a standalone PlatformIO project pinned to
the `pioarduino` platform the C5 needs. An example is committed only once its
board capability has been observed working on a real board.

Build and flash any example from its own directory:

```
pio run -e <env> -t upload
```

Console is USB-Serial-JTAG at 115200. If the board won't auto-enter download
mode: **hold BOOT, tap RST, release BOOT**, then upload.

## Available

- **`hello_board/`** — Phase 1. Brings the board up through the BSP (expander
  powers the panel + releases the ST7796 reset), paints the display, prints the
  board name, and draws/reports each FT6336 touch. Carries its own board
  manifest, variant, partition table, TFT_eSPI user setup and the ESP32-C5
  processor patch, so it builds without any other project's toolchain.

## Planned

- `gps_i2c/` — LC76G over I²C, print fix + satellite count (Phase 3)
- `battery/` — AXP2101 readout via the expander (Phase 4)
- `reliability_reference/` — Wi-Fi + GPS + touch at once (capstone)
