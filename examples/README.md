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
- **`touch_int/`** — optional INT-gated touch (`Touch::beginInt(GPIO3)`): reads
  are skipped when the FT6336 INT line says no finger is down, killing stale /
  phantom points. Serial only — prints that it reports no touch when untouched
  (no phantom) and tracks a finger when down.
- **`battery/`** — Phase 4. AXP2101 readout via XPowersLib: percentage, cell
  voltage, charge/USB state. Boot I²C scan. Serial only. Needs a LiPo on the
  MX1.25 `BAT` header for percentage/voltage.
- **`reliability_reference/`** — the capstone. Wi-Fi + GPS + touch + battery all
  live on the shared bus at once, with a status screen and a touch scribble area
  that stays smooth under load. Needs the display, the LC76G on I²C, and
  (optionally) a battery.
