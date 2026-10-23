# Examples

One runnable sketch per phase, each a standalone PlatformIO project pinned to
the `pioarduino` platform the C5 needs. An example is committed only once its
board capability has been observed working on a real board.

Build and flash any example from its own directory:

```
pio run -e <env> -t upload
```

Console is USB-Serial-JTAG at 115200. If the board won't auto-enter download
mode: **hold BOOT, tap RST, release BOOT**, then upload. The platform needs
**Python 3.10–3.13** — see [../docs/BUILD.md](../docs/BUILD.md) if your `pio` is
on 3.14+.

## Available

- **`hello_board/`** — Phase 1. Brings the board up through the BSP (expander
  powers the panel + releases the ST7796 reset), paints the display, prints the
  board name, and draws a stroke that follows FT6336 touch. Carries its own
  board manifest, variant, partition table, TFT_eSPI user setup and the ESP32-C5
  processor patch, so it builds without any other project's toolchain.
- **`gps_i2c/`** — Phase 3. LC76G GNSS over I²C on the shared bus: a one-shot bus
  scan (confirms `0x50` is present), then link health, then fix + satellite
  count. Display-less (no TFT_eSPI). Wire the LC76G to SDA 27 / SCL 26 / 3V3 /
  GND — see [docs/BOARD-WS-C5-35.md](../docs/BOARD-WS-C5-35.md).
- **`single_core/`** — Phase 5. Shows the C5 is unicore and that the BSP's
  `createPinnedTask()` lands a task on the only core. Serial only.

## Planned

- `battery/` — AXP2101 readout (Phase 4, deferred until a LiPo is on the board)
- `reliability_reference/` — Wi-Fi + GPS + touch at once (capstone)
