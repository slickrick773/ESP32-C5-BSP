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

### Waveshare ESP32-C5-Touch-LCD-3.5

| Phase | Scope | Status |
|---|---|---|
| 0 | Scaffold + hardware-findings doc | ✅ done |
| 1 | Board definition, pin map, ST7796 display, FT6336 touch | ✅ hardware-verified |
| 2 | Shared-I²C core (reopen / lock / timeout / recovery) + touch hardening | ✅ hardware-verified |
| 3 | LC76G GPS over I²C | ✅ hardware-verified (29 sats, HDOP ~0.6); re-run 2026-11-10 after the driver moved to shared code: 13–15 sats, HDOP 0.84–1.01 |
| 4 | AXP2101 battery readout | ✅ hardware-verified (89% / 4.15 V / charging) |
| 5 | Single-core task-pinning helper + chip utilities | ✅ hardware-verified |
| 6 | Wi-Fi + GPS + touch capstone (`reliability_reference`) | ✅ hardware-verified |
| 7 | ES8311 audio: speaker playback, microphone capture and gain, playing and recording at once — [audio_35](examples/audio_35/README.md), [mic_35](examples/mic_35/README.md), [audio_selftest_35](examples/audio_selftest_35/README.md) | ✅ hardware-verified 2026-11-10: owner judged the 1 kHz speaker test clean and loud enough (about 6% harmonics, pitch 995–999 Hz by laptop microphone); microphone level follows sound and one played-back capture was judged clear speech; the self-test passed three of three runs and reported FAIL with the speaker silenced. Microphone noise and frequency response not measured |
| 8 | Onboard devices: PCF85063 clock, QMI8658 IMU, SHTC3 temperature/humidity, BOOT button — [onboard_35](examples/onboard_35/README.md) | ✅ hardware-verified 2026-11-10 on two boards: clock counting, IMU at rest and in three poses, SHTC3 readings, BOOT presses. The clock has no backup cell: it kept time through a USB unplug with a LiPo fitted and lost it without one. Sensor accuracy not tested |
| 9 | microSD on the shared SPI bus (tested example; no BSP SD driver) — [sd_35](examples/sd_35/README.md) | ✅ hardware-verified 2026-11-10 with one 29.8 GB card at 4 MHz; a bench build also read and wrote it without error while the display redrew |
| Sample | [Touch and scrolling sample](examples/touch_redraw/README.md) — scrolling list, row selection, Toggle and Count (3.5-inch) | ✅ hardware-tested; cold boot and physical reset confirmed for the documented images |
| Sample | [Touch and audio](examples/touch_audio/README.md) — list, TONE and MIC controls (3.5-inch) | ✅ hardware-tested 2026-11-10: owner heard a clean tone while scrolling the list, the list kept moving, and MIC followed a clap; long-run use not tested |
| Sample | [Whole-board check](examples/board_check_35/README.md) — every onboard device in one PASS/FAIL table (3.5-inch) | ✅ ten of ten checks passed, two consecutive runs, 2026-11-10 |
| Sample | [Speaker switch, PWR button, header lines, charge current](examples/extras_35/README.md) — self-checking test (3.5-inch) | ✅ hardware-tested 2026-11-10, one board: charge-current setting read back, header lines E8 and E9 passed with a jumper wire, speaker silenced and restored (laptop-microphone recording), PWR press read. On a second board with a LiPo, battery voltage stepped with the charge setting, six rounds of six. Charge current not measured in mA; header voltage and E10–E15 not measured |

Sample rows are demonstrations built on those phases and add no driver of
their own. Phases 2 and 5 are chip-level and serve both boards.

### Waveshare ESP32-C5-Touch-LCD-2.8

The 2.8-inch board was brought up by capability rather than in numbered
phases. It uses a different display, touch controller, pinout and battery
circuit from the 3.5-inch, so each row has its own driver and evidence.

| Capability | Evidence | Status |
|---|---|---|
| Board definition, pin map, CH32V006 expander, ST7789 display, CST3530 touch | [hello_board_28](examples/hello_board_28/) | ✅ hardware-verified 2026-10-31: panel renders correctly; taps and a continuous drag tracked |
| ES8311 speaker playback | [audio_28](examples/audio_28/README.md) | ✅ hardware-verified 2026-10-31 by the owner's ear: repeating beeps without crackle; output frequency not measured |
| Battery voltage through the expander, and running on battery alone | [battery_28](examples/battery_28/README.md) | ✅ hardware-verified: about 4.1 V estimated with USB and battery connected, and the screen kept updating after USB was unplugged. Voltage accuracy, charging state and runtime not verified |
| GPS (LC76G over I²C, sharing the bus with touch) | [gps_28](examples/gps_28/README.md) | ✅ hardware-verified 2026-11-10: fix with 7 satellites, HDOP 2.2; three minutes with no bus failures; taps still read correctly. The module's wake-after-idle step, which uses the touch chip's address, is skipped and was not exercised |
| Microphone capture | [mic_28](examples/mic_28/README.md) | ✅ hardware-verified 2026-11-10 for level: about 55–68 when quiet, 1549 and clipped with claps. No capture from this board has been listened to |
| Playing and recording at once | [audio_selftest_28](examples/audio_selftest_28/README.md) | ✅ hardware-verified 2026-11-10: the board heard its own tone, three of three runs |
| Clock, IMU, temperature/humidity sensor | [onboard_28](examples/onboard_28/README.md) | ✅ hardware-verified 2026-11-10: clock counting, 1.04 g at rest, about 23 °C and 47 %. The RTC battery socket, a BOOT press and sensor accuracy not tested |
| microSD | [sd_28](examples/sd_28/README.md) | ✅ hardware-verified 2026-11-10 with one 29.6 GB card at 4 MHz, display idle |
| Display, touch and audio together | [touch_audio_28](examples/touch_audio_28/README.md) | ✅ hardware-verified 2026-11-10: owner heard a clean tone while an on-screen counter kept running, and the microphone reading followed a clap |
| Whole-board check | [board_check_28](examples/board_check_28/README.md) | ✅ ten of ten checks passed, four consecutive runs, 2026-11-10 |

Board-specific notes are listed below.

The latest release is `v0.6.0`. See the [changelog](CHANGELOG.md). To report a result
from your own board or propose a change, see [CONTRIBUTING](CONTRIBUTING.md).

Porting firmware to the 3.5-inch board? The
[porting checklist](docs/PORTING-CHECKLIST.md) is one page of what every port
has had to get right, each item linked to its evidence.

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

- **Waveshare ESP32-C5-Touch-LCD-3.5** — phases 1–9 above: display, touch,
  shared-bus core, GPS, battery, task helpers, the Wi-Fi capstone, ES8311
  audio in both directions, the onboard clock, IMU and temperature/humidity
  sensor, and microSD. The status table states what was and was not tested
  for each. Two things worth knowing up front: the clock has no backup cell
  and needs USB or a LiPo to keep time, and the small speaker is far louder
  at 1 kHz and above than at 440 Hz. See
  [docs/BOARD-WS-C5-35.md](docs/BOARD-WS-C5-35.md).
- **Waveshare ESP32-C5-Touch-LCD-2.8** — the 2.8" twin (ST7789 240×320, CST3530
  touch). Display, touch, speaker, microphone, GPS, the onboard clock, IMU and
  temperature/humidity sensor, microSD and battery-voltage readout are all
  hardware-verified; the status table above gives the evidence and limits for
  each. Voltage accuracy, charging state and runtime capacity are unverified.
  See [docs/BOARD-WS-C5-28.md](docs/BOARD-WS-C5-28.md).

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

## Known limitations

Things that are true of the hardware or deliberately not covered, so "done"
has a definite meaning. Each links to its evidence.

**Hardware behaviour the BSP cannot change**
- **About one second of grey backlight glow** after power-on or the RST button,
  before any application code runs (3.5-inch; measured from video). Starting
  dark removes the static and leftover-picture effects that can follow it, but
  not this first second. See
  [Clean startup](docs/BOARD-WS-C5-35.md#clean-startup).
- **The 3.5-inch clock has no backup cell.** It keeps time on USB or a LiPo and
  loses it when both are gone. The 2.8-inch has a socket for a backup cell,
  which is untested.
- **The 3.5-inch speaker is weak below about 1 kHz**, and codec volume `0xE0`
  adds about 16 dB of digital gain, so a tone that is both low and loud clips.
  The examples play 1 kHz for that reason.
- **The microphone clips on loud nearby sound** at the default gain. Speech was
  clear at that gain; `setMicGain()` lowers it.
- **On the 2.8-inch, the battery reading does not show whether a battery is
  fitted**: it read about 4.2 V with none connected.
- **A boot-time PSRAM error line** (`MSPI Timing: Failed to allocate dummy
  cacheline for PSRAM memory barrier!`) appears with every example. It comes
  from a workaround for chips older than revision v1.2 that the pinned Arduino
  core attempts before it has set PSRAM up. The boards tested are v1.2, so it
  is expected to be harmless here; that is traced from source, not tested. See
  [HARDWARE-FINDINGS](docs/HARDWARE-FINDINGS.md).

**Not exercised**
- 2.8-inch GPS waking after a long idle. The step that would use the touch
  controller's address is skipped; a three-minute run needed no wake.
- Sensor, IMU and battery-voltage accuracy on either board, and the 2.8-inch
  charging state.
- The 2.8-inch BOOT button as an input, and a listened-to recording from the
  2.8-inch microphone.
- microSD cards other than the two tested, clocks above 4 MHz, and card use
  while the 2.8-inch display is drawing.
- Long-running sessions of any example.

**By design**
- There is no BSP SD driver; the examples use the Arduino `SD` library with the
  BSP's pins and shared-bus startup.
- The camera on camera-fitted boards is identified but not driven.
- The display library is the application's choice; the BSP owns bring-up, not
  drawing.

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
[whole-board check](examples/board_check_35/README.md),
[3.5-inch speaker switch, PWR button, header and charge-current check](examples/extras_35/README.md),
[2.8-inch GPS test](examples/gps_28/README.md),
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
                       gps · board begin()/name()
            ws_c5_28/  pins · expander (CH32V006) · touch (CST3530) ·
                       audio (ES8311 playback, mic capture) · battery voltage ·
                       rtc · imu · env · gps · board begin()/name()
  part/     drivers for parts fitted to both boards, used through each
            board's headers: pcf85063_rtc · qmi8658_imu · shtc3_env · lc76g_gps
examples/   standalone samples (PlatformIO unless noted)
            touch_redraw           touch + scrolling list, row selection, Toggle, Count (3.5")
            hello_board            display + touch (3.5", TFT_eSPI)
            hello_board_28         display + touch (2.8", ST7789 Arduino_GFX)
            audio_35               ES8311 three-tone speaker test (3.5", ESP_I2S)
            mic_35                 ES8311 microphone level test (3.5", ESP_I2S)
            sd_35                  microSD mount + write/read-back (3.5")
            onboard_35             clock, IMU, SHTC3 and BOOT button readout (3.5")
            audio_selftest_35      speaker-to-microphone loopback check, PASS/FAIL (3.5")
            touch_audio            list + TONE and MIC controls (3.5")
            extras_35              speaker switch, PWR button, header lines, charge current, PASS/FAIL (3.5")
            audio_28               ES8311 440 Hz beep (2.8", ESP_I2S)
            mic_28                 ES8311 microphone level test (2.8")
            audio_selftest_28      speaker-to-microphone loopback check (2.8")
            onboard_28             clock, IMU, SHTC3 and BOOT button readout (2.8")
            gps_28                 LC76G GPS over I²C with touch alongside (2.8")
            sd_28                  microSD mount + write/read-back (2.8")
            touch_audio_28         display, touch, tone and mic together (2.8", Arduino_GFX)
            board_check_35/_28     every onboard device, one PASS/FAIL table
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
