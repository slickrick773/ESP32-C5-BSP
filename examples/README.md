# Examples

Standalone capability examples and diagnostic utilities. Most use PlatformIO;
`ble_heap_probe` uses Arduino CLI. Each example documents its verification
limits; inclusion alone is not a hardware-support claim.

Build and flash any example from its own directory:

```
pio run -e <env> -t upload
```

Console is USB-Serial-JTAG at 115200. If the board won't auto-enter download
mode: **hold BOOT, tap RST, release BOOT**, then upload. The platform needs
**Python 3.10–3.13** — see [../docs/BUILD.md](../docs/BUILD.md) if your `pio` is
on 3.14+.

## Available

- **[Touch and scrolling sample](touch_redraw/README.md)** — Waveshare C5
  **3.5-inch**. Scroll the list, tap a row to select it, switch Toggle, and tap
  Count to increment the counter. The backlight turns on after the first
  complete frame. Cold boot and physical reset passed the user check for the
  documented test image. Open the sample for build instructions and test details.
- **`hello_board/`** — Phase 1 (Waveshare 3.5"). Brings the board up through the
  BSP (expander powers the panel + releases the ST7796 reset), paints the
  display, prints the board name, and draws a stroke that follows FT6336 touch.
  Carries its own board manifest, variant, partition table, TFT_eSPI user setup
  and the ESP32-C5 processor patch, so it builds without any other project's
  toolchain.
- **`hello_board_28/`** — Phase 1 for the Waveshare **2.8"** (ST7789 240×320,
  CST3530 touch). Same proof as `hello_board`, but drives the panel with
  **Arduino_GFX** (`moononournation/GFX Library for Arduino @ 1.6.9`),
  reproducing the proven downstream 2.8" path incl. the MSB-first RAMCTRL
  byte-order fix. No TFT_eSPI user setup and no C5 processor patch (Arduino_GFX
  drives through the Arduino SPI class). **Hardware-verified on the real 2.8"
  board (2026-10-31):** flashed from this BSP, panel renders correctly and the
  CST3530 tracks taps + a smooth drag at the 8 ms `poll()` cadence. See
  [docs/BOARD-WS-C5-28.md](../docs/BOARD-WS-C5-28.md).
- **[`audio_28/`](audio_28/README.md)** — Waveshare **2.8-inch speaker playback**,
  hardware-verified by the owner hearing repeating 440 Hz beeps (2026-10-31). Uses
  Arduino ESP_I2S with no MCLK, MCU DOUT=26 / DIN=27, and EXIO3 amp enable.
  Codec control stays driver-agnostic in the BSP; recording is not verified.
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
- **[`battery_28/`](battery_28/README.md)** — 2.8-inch raw battery ADC and
  estimated voltage over the expander, with a screen timer and serial output.
  Owner-confirmed continued operation after USB removal; voltage accuracy,
  percentage and charging state are not verified.
- **`battery/`** — Phase 4. AXP2101 readout via XPowersLib: percentage, cell
  voltage, charge/USB state. Boot I²C scan. Serial only. Needs a LiPo on the
  MX1.25 `BAT` header for percentage/voltage.
- **`reliability_reference/`** — the capstone. Wi-Fi + GPS + touch + battery all
  live on the shared bus at once, with a status screen and a touch scribble area
  that stays smooth under load. Needs the display, the LC76G on I²C, and
  (optionally) a battery.

- **[audio_35](audio_35/README.md)** — 3.5-inch ES8311 speaker test. Three nominal
  440 Hz tones, then automatic stop; clean playback confirmed by the owner on
  November 10, 2026. No display rendering; recording and pitch accuracy unverified.
- **[mic_35](mic_35/README.md)** — 3.5-inch ES8311 microphone level test. Three
  1-second captures with level statistics over serial, then automatic stop. On
  November 10, 2026 the level stayed low in silent runs and rose when sound
  was made. No captured audio has been listened to; recording quality is
  unverified and loud nearby sound clipped at the default gain.
- **[screen_flush](screen_flush/)** — 3.5-inch panel colour-cycle utility.
- **[ble_heap_probe](ble_heap_probe/README.md)** — Arduino CLI diagnostic for BLE
  advertising under memory pressure, not a supported application feature.
