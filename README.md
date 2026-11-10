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

Built one phase at a time, and **a capability is only called "supported" once it
has run on a real board.** See [docs/HARDWARE-FINDINGS.md](docs/HARDWARE-FINDINGS.md)
for the three levels of proof (silicon ≠ IDF-exposed ≠ works-on-hardware) and
every claim's tag.

| Phase | Scope | Status |
|---|---|---|
| 0 | Scaffold + hardware-findings doc | ✅ done |
| 1 | Board definition, pin map, ST7796 display, FT6336 touch | ✅ hardware-verified |
| 2 | Shared-I²C core (reopen / lock / timeout / recovery) + touch hardening | ✅ hardware-verified |
| 3 | LC76G GPS over I²C | ✅ hardware-verified (29 sats, HDOP ~0.6) |
| 4 | AXP2101 battery readout | ✅ hardware-verified (89% / 4.15 V / charging) |
| 5 | Single-core task-pinning helper + chip utilities | ✅ hardware-verified |
| 6 | Wi-Fi + GPS + touch capstone (`reliability_reference`) | ✅ hardware-verified |
| Sample | [Touch and scrolling sample](examples/touch_redraw/README.md) — scrolling list, row selection, Toggle and Count (3.5-inch) | ✅ hardware-tested; cold boot and physical reset confirmed for the documented images |
| Sample | [Speaker test](examples/audio_35/README.md) — three 440 Hz tones through the ES8311 (3.5-inch) | ✅ hardware-tested; clean playback confirmed by the owner, 2026-11-10 |
| Sample | [Microphone level test](examples/mic_35/README.md) — ES8311 capture level over serial (3.5-inch) | ✅ hardware-tested 2026-11-10: level follows sound, and the owner judged one played-back 3-second capture clear speech; noise and frequency response not measured |
| Sample | [microSD test](examples/sd_35/README.md) — mount, write, read back, delete (3.5-inch) | ✅ hardware-tested with one 29.8 GB card at 4 MHz, 2026-11-10; a bench build also read and wrote it without error while the display redrew |
| Sample | [Onboard devices readout](examples/onboard_35/README.md) — clock, IMU, temperature/humidity, BOOT button (3.5-inch) | ✅ hardware-tested 2026-11-10 on two boards: clock counting, IMU at rest and in three poses, SHTC3 readings, BOOT presses. The clock has no backup cell: it kept time through a USB unplug with a LiPo fitted and lost it without one |
| Sample | [Touch and audio](examples/touch_audio/README.md) — list, TONE and MIC controls (3.5-inch) | ✅ hardware-tested 2026-11-10: owner heard a clean tone while scrolling the list, the list kept moving, and MIC followed a clap; long-run use not tested |
| Sample | [Audio self-test](examples/audio_selftest_35/README.md) — board plays a tone and hears it on its own microphone (3.5-inch) | ✅ hardware-tested 2026-11-10: passed three of three runs, and reported FAIL when the speaker was silenced |

Phases 1–6 describe the **3.5-inch** board. Board-specific support is listed below.

The latest Git tag is `v0.4.0`. Main contains additional changes; no GitHub
Releases have been published.

Adopted across the C5 fleet (Marauder, PorkChop, Bruce,
Picoware, Launcher) — each port self-contained, no BSP dependency upstream. Three
more are in review upstream: GhostESP, AWOKxDAG, SquachWatch.

Chip-level findings that are **hardware-verified on the fleet and documented
here, but not yet wrapped as BSP code**: the dual-band `set_band_mode()`-after-
`start()` ordering and 5 GHz channel enumeration. They
live in the findings doc until a verified helper lands.

Board-level traps are documented there too: the NeoPixel pin that is really the
I²C SDA line (GPIO27), the backlight that runs off the expander (no `TFT_BL`
pin), the AXP2101 fuel gauge read over raw I²C (no XPowersLib needed), the
GPIO13/14 = native-USB trap, and the USB link stalling on large transfers.

## Supported boards

- **Waveshare ESP32-C5-Touch-LCD-3.5** — phases 1–6 above, plus ES8311 speaker
  playback. The owner confirmed clean playback on November 10, 2026, using
  the standalone [audio_35 example](examples/audio_35/README.md). Microphone
  capture is implemented: on the same date the
  [mic_35 example](examples/mic_35/README.md) showed the level staying low in
  a silent run and rising when sound was made, and the owner judged one
  played-back 3-second capture clear speech. Pitch accuracy is unverified.
  With the display running, the [touch_audio sample](examples/touch_audio/README.md)
  played a tone the owner judged clean. The microSD card, real-time clock
  and IMU each have a driver or example with a first hardware result; see the
  status table above for exactly what was and was not tested. See
  [docs/BOARD-WS-C5-35.md](docs/BOARD-WS-C5-35.md).
- **Waveshare ESP32-C5-Touch-LCD-2.8** — the 2.8" twin (ST7789 240×320, CST3530
  touch). **Display, touch, speaker playback, battery-voltage readout, and
  continued battery-only operation are hardware-verified** in separate BSP
  examples. Voltage accuracy, charging state, runtime capacity, recording, and
  GPS remain unverified. See
  [docs/BOARD-WS-C5-28.md](docs/BOARD-WS-C5-28.md).

Other C5 boards can be added against the chip core + board-template structure —
but none is claimed supported until someone runs it on that hardware.

The Waveshare boards are two members of a wider **C5 touch-display class**
(ESP32-C5 + an SPI panel + an I²C capacitive touch) that the fleet's ports
already carry side by side. What the shared core is, how the 2.8" and 3.5"
differ, and exactly how the twin **Marauder Pancake C5** differs, is in
[docs/C5-TOUCH-CLASS.md](docs/C5-TOUCH-CLASS.md) — so a driver written for one
board lands on the other.

GPS on this board runs over I²C — the LC76G on the shared bus — which needs no
extra GPIO and leaves the BOOT button free. The I²C and UART options, and the
trade-offs between them, are in [docs/GPS.md](docs/GPS.md).

## Touch and scrolling sample

**[Open the touch and scrolling sample](examples/touch_redraw/README.md)** —
Waveshare C5 3.5-inch example with a scrolling list, tap-to-select rows, Toggle,
and Count controls. The backlight turns on after the first complete frame.
Cold boot and physical reset passed the user check for the documented test image.

See [all examples](examples/README.md), including the
[3.5-inch speaker test](examples/audio_35/README.md),
[3.5-inch microphone level test](examples/mic_35/README.md),
[3.5-inch audio self-test](examples/audio_selftest_35/README.md),
[3.5-inch microSD test](examples/sd_35/README.md),
[3.5-inch onboard devices readout](examples/onboard_35/README.md),
[3.5-inch touch and audio sample](examples/touch_audio/README.md),
[2.8-inch battery test](examples/battery_28/README.md),
[panel colour-cycle utility](examples/screen_flush/), and
[BLE heap diagnostic](examples/ble_heap_probe/README.md).

## Architecture

```
src/
  chip/     C5 chip-level, reusable across any C5 board
            esp32c5_i2c_bus   shared-bus discipline: reopen, non-blocking lock,
                              timeout guard, 9-clock recovery
            esp32c5_tasks     single-core-aware task pinning + chip info
            esp32c5_spi       deterministic SPI2 peripheral reset
  board/    board-specific drivers; two hardware-proven boards
            ws_c5_35/  pins · expander (CH32V006) · touch (FT6336) ·
                       gps (LC76G over I²C) · battery (AXP2101) ·
                       audio (ES8311 playback, mic capture) · rtc · imu · env (SHTC3) ·
                       board begin()/name()
            ws_c5_28/  pins · expander (CH32V006) · touch (CST3530) ·
                       audio (ES8311 playback) · battery voltage ·
                       board begin()/name() (recording/gps later)
examples/   standalone samples (PlatformIO unless noted)
            touch_redraw           touch + scrolling list, row selection, Toggle, Count (3.5")
            hello_board            display + touch (3.5", TFT_eSPI)
            hello_board_28         display + touch (2.8", ST7789 Arduino_GFX)
            audio_35               ES8311 three-tone test (3.5", ESP_I2S)
            mic_35                 ES8311 microphone level test (3.5", ESP_I2S)
            sd_35                  microSD mount + write/read-back (3.5")
            onboard_35             clock, IMU, SHTC3 and BOOT button readout (3.5")
            audio_selftest_35      speaker-to-microphone loopback check, PASS/FAIL (3.5")
            touch_audio            list + TONE and MIC controls (3.5")
            audio_28               ES8311 440 Hz beep (2.8", ESP_I2S)
            gps_i2c                LC76G GPS over I²C (fix + sat count)
            battery_28             battery ADC/estimated voltage + battery-only screen test
            ble_heap_probe         BLE advertiser memory-pressure diagnostic (Arduino CLI)
            battery                AXP2101 readout (%, voltage, charge state)
            touch_int              INT-gated touch (skips reads when no finger)
            single_core            single-core task pinning
            reliability_reference  capstone: Wi-Fi + GPS + touch + battery
            screen_flush           clears ST7796 image retention (colour cycle)
docs/       hardware findings, per-board notes, GPS wiring, build guide
tools/      c5flash   ROM-loader flash read/write that chunks around the
                      C5 USB large-transfer stall
```

Built as an Arduino library first (the proven code is Arduino/C++). The hardware
core — I²C discipline, GPS protocol, pin maps, findings — is kept free of any
graphics library (drivers use only `Wire`) so an ESP-IDF component can wrap it
later. Only one framework is built now.

## Toolchain

Built with PlatformIO using the `pioarduino` platform fork (the C5 needs it).
See each example's `platformio.ini` and [docs/BUILD.md](docs/BUILD.md) — note
the **Python 3.10–3.13** requirement (the platform refuses 3.14+). To depend on
the BSP from your own project (exact pin, the scaffolding to copy, and the
validated non-display build), see [docs/CONSUMER-SETUP.md](docs/CONSUMER-SETUP.md). Console is
USB-Serial-JTAG at 115200; download mode is **hold BOOT, tap RST, release
BOOT**.

## License

MIT. See [LICENSE](LICENSE).
