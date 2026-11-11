# Board: Waveshare ESP32-C5-Touch-LCD-3.5

A 3.5" capacitive C5 board: ESP32-C5 (RISC-V, dual-band 2.4/5 GHz Wi-Fi + BLE),
ST7796 320×480 display, FT6336 touch, ES8311 audio, AXP2101 battery, all behind
a CH32V006 I/O expander on a shared I²C bus.

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
| I/O expander (CH32V006) | `0x24` on the touch bus — LCD reset, touch reset, backlight PWM, speaker amplifier enable (EXIO5), PWR button (EXIO6) |
| Audio (ES8311) | codec `0x18` on the touch bus → I²S BCK 23, WS 10, DOUT 25 (no MCLK) → NS4150B amp |
| Battery (AXP2101) | PMU on the touch bus; MX1.25 2-pin 3.7 V LiPo header (cell optional) |
| SD | CS 9 |
| GPS (LC76G) | over I²C on the **same** touch bus (SDA 27 / SCL 26) — command `0x50`, read `0x54`, wake `0x58`. The J3 header UART is not bonded to the chip; see HARDWARE-FINDINGS. |

## Board-specific notes

- **Start dark, reveal after the first frame.** Call `begin(0)` first, draw the
  complete first frame, then `Expander::backlight(255)`. This hides the panel's
  leftover memory during startup; see [Clean startup](#clean-startup) for what
  it does and does not remove. Plain `begin()` remains backward-compatible and
  starts at full brightness.
- **One shared SPI owner.** Display and SD share SPI2. Keep one live SPI object,
  bracket each device with its own transaction and chip select, and never tear
  down the bus merely because SD initialization failed. `prepareSharedSpi()` is
  available separately; `begin()` calls it automatically.
- **Classified touch samples.** `Touch::readSample()` reports `Contact`,
  `Released`, `Unavailable`, or `Error`. Existing `read()` and `poll()` remain
  compatible boolean APIs.

- **One shared I²C bus** (SDA 27 / SCL 26) carries the FT6336, ES8311, CH32V006,
  AXP2101 **and** the GPS. Coexistence depends on the bus discipline documented in
  HARDWARE-FINDINGS: per-read bus re-open + retry for touch, STOP-based reads for
  the codec, and a short-settle, touch-gated, timeout-restoring GPS cycle.
- **GPS, bring-your-own parser.** `Gps::tick(now)` parses NMEA into the built-in
  getters. A sketch that already has a GNSS parser can instead call
  `Gps::tick(sink, now, busy)` to get the identical LC76G transport + shared-bus
  discipline while each NMEA byte is handed to `sink(c, now)`; the built-in
  getters are not updated in that mode. Everything else (throttle, lock, reopen,
  wake, `busyUntil` defer, link counters) is the same.
- **No backlight GPIO / no LEDC.** Backlight PWM, LCD reset and touch reset all
  live on the `0x24` expander. Start-up drives both resets and EXIO5 high
  together, then pulses each reset; that order is the one verified on hardware.
  EXIO5 is the speaker amplifier enable, not panel power: see
  [the finding](HARDWARE-FINDINGS.md#exio5-is-the-speaker-amplifier-enable-exio6-reads-the-pwr-button).
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
| `0x24` | CH32V006 I/O expander |
| `0x34` | AXP2101 PMU |
| `0x38` | FT6336 touch |
| `0x51` | RTC |
| `0x6B` | IMU |
| `0x6E` | BF3901 camera control (on boards fitted with the camera) |
| `0x70` | SHTC3 temperature/humidity sensor |
| `0x50` / `0x54` / `0x58` | LC76G GPS — **only when an I²C GPS is wired** |

A boot scan that shows the onboard set but no `0x50` means the GPS is the
problem (SDA/SCL swapped, wrong pins, not in I²C mode, or unpowered), not the
bus. Note `0x51` is the RTC, not the GPS.

## Verified in this BSP (on hardware)

Display (ST7796) · touch (FT6336) · I/O expander (CH32V006) · GPS (LC76G over
I²C) · single-core task pinning · USB-JTAG console · battery (AXP2101, read via
XPowersLib with a cell on the MX1.25 header). Wi-Fi (2.4 + 5 GHz) and BLE are
proven on the fleet and documented in HARDWARE-FINDINGS.md, but are not yet
wrapped as BSP code here. microSD, the clock, the IMU and the temperature/
humidity sensor have first results below. See the verification ledger in HARDWARE-FINDINGS.md.

## Clean startup

Measured from phone video on November 10, 2026, comparing two builds that
differ only in when the backlight turns on.

**What you see at startup, and why**

| What | When | Avoidable in firmware? |
|---|---|---|
| A blank grey glow for about one second | After power-on or the RST button, before any application code runs | **No.** The backlight is already on at that point |
| The old picture fading out over about 0.2 s | At the instant RST is pressed | No |
| "TV static": bright speckled noise | Cold boot, when the display is switched on before the first frame is written | **Yes** |
| The previous picture reappearing | After RST, when the display is switched on before the first frame is written | **Yes** |
| The picture visibly being drawn in pieces | While the first frame is written | **Yes** |

The three avoidable effects are one thing: the panel has its own memory. After
power-on it holds random data; after a reset it still holds the last picture.
If the backlight is lit when the display is switched on, that memory is what
you see until the first frame overwrites it.

**The recipe**

```cpp
WsC5_35::begin(0);                         // first action: backlight off
// ... initialise the display library and draw the COMPLETE first frame ...
delay(150);                                // let the glass catch up with panel memory
WsC5_35::Expander::backlight(255);         // only now light the panel
```

With that order the video showed darkness after the first second of glow, then
the finished picture at once: no static, no old picture, no partial drawing.
With the backlight lit first, the same code showed all three.
[touch_redraw](../examples/touch_redraw/README.md) and
[touch_audio](../examples/touch_audio/README.md) follow that order; they light
the panel straight after the last draw, without the extra wait.

**Wait briefly before lighting.** The glass shows a new frame slightly after the
panel's memory receives it. In one firmware port to this board, lighting the
backlight immediately after the last draw call still showed static or the old
picture for a few frames on slow-motion video; a 150 ms wait between the last
draw and `backlight(255)` removed it. Five further ports used the same wait and
were clean on video. One more was clean with only about 60 ms of other start-up
work in between, so 150 ms is a safe value, not a measured minimum.

**What is not solved.** The first second of grey glow after power-on or RST.
An experimental bootloader that wrote the backlight off earlier did not remove
it (see [the evidence record](HARDWARE-FINDINGS.md#hide-panel-memory-until-the-first-frame)
and [the startup probe](../tools/startup_probe/README.md)). Removing it would
take a change to the expander chip's own firmware or to the hardware.

**Software restart is different.** After `esp_restart()` the screen went dark
within about a tenth of a second and there was no grey glow: the application
starts almost at once and the expander keeps its state.

## Speaker playback

ES8311 speaker playback is implemented in the BSP.
[audio_35](../examples/audio_35/README.md) plays three nominal 1 kHz tones,
then stops; it renders no display. On November 10, 2026 the owner judged them
clean and loud enough, and a laptop-microphone recording measured about 6%
harmonics and a pitch of 995 to 999 Hz (the recorder is not a calibrated
reference). Initialize the board expander/shared I²C bus before starting I2S
and configuring the codec. Expander initialization turns the speaker amplifier on
(EXIO5), which is why the test was silent without it.

With the display running, the
[touch_audio](../examples/touch_audio/README.md) sample's tone was judged clean
by the owner while the list scrolled. Its first version pulsed because it
generated samples slower than real time (about 24 ms per 16 ms block);
rendering the tone ahead of time fixed it.

## Microphone capture

ES8311 microphone capture is implemented in the BSP (`Audio::beginRecording()`):
onboard mic → codec ADC → I2S DIN on GPIO 24, sharing BCLK 23 / LRCK 10 with
playback, 16 kHz 16-bit. Playback and capture are one at a time.

Tested on November 10, 2026 with [mic_35](../examples/mic_35/README.md), for
level response only:

- Silent run in a quiet room: left-channel AC RMS about 23 in all three
  1-second windows.
- Clap run: about 23 while silent, then 190 and 2059 once clapping started.
- Earlier runs with background noise in the room gave silent readings up to
  about 118 and clap readings between about 775 and 2348.
- The right I2S slot read zero throughout; the data arrived on the left slot.
- Clap windows reached full scale in every run, so the default gain clips
  nearby claps.

A bench tool using the same driver and settings then saved one 3-second
capture of the owner counting aloud. Played back, the owner judged it clear
speech; it peaked at 6552 and did not clip. Noise floor and frequency response
are unmeasured. The ADC settings follow an existing 3.5-inch port of another
firmware.

### Microphone gain

`Audio::setMicGain(step)` sets the analog preamp, 0 to 10. A sweep on
November 10, 2026 in a quiet room rose at every step, from an AC RMS of about
2 at step 0 to about 26 at step 10. The speech capture above used step 10.

### Playing and recording at once

With the codec in its playback configuration (`Audio::begin()`), the ADC stays
enabled. Running I2S in both directions then returns the microphone on the
left slot and the codec's digital copy of the DAC input on the right slot.
[audio_selftest_35](../examples/audio_selftest_35/README.md) uses this to let
the board check its own speaker and microphone; it passed three of three runs
on November 10, 2026.

### Speaker level

Two measured facts set the default tone:

- **Codec volume `0xE0` adds about 16 dB of digital gain.** Lowering it to
  `0xC0` cut the measured level by a factor of about 6, which matches 0.5 dB
  per step with `0xBF` as unity. At `0xE0` a PCM peak above roughly 4900
  overruns the DAC. This is inferred from measurements, not a datasheet check.
- **The speaker barely reproduces 440 Hz.** The first speaker test played
  440 Hz at peak 12000. Recordings showed its third and fifth harmonics
  stronger than 440 Hz itself: it was clipped, and it sounded loud only
  because of those harmonics. A clean 440 Hz tone at the highest undistorted
  level was judged "very low" by the owner.

At the same undistorted level the owner found 1 kHz and 2 kHz progressively
louder than 440 Hz. The examples now play 1 kHz at peak 4500 and volume
`0xE0`, which measured about 6% harmonics. The board's own microphone agreed
at lower levels: about 1 to 2% harmonics for 1 kHz at peak 3000.

## microSD

The card is on the display's SPI bus (SCLK 6, MOSI 7, MISO 2) with chip select
GPIO 9. [sd_35](../examples/sd_35/README.md) passed on November 10, 2026:
a 29.8 GB SDHC card mounted at 4 MHz and a 47-byte file was written, read back
identical, and deleted. A bench build then used the card through the display
library's SPI object while the display redrew: 12 files of 4 KB written
between 480 display strips all read back identical. Higher clocks and other
cards are unverified. There is no BSP SD driver; the example uses the Arduino
`SD` library with the BSP's pins and shared-SPI startup.

## Real-time clock

`WsC5_35::Rtc` (`begin`, `read`, `set`) drives the clock at `0x51`. Its
registers behave like a PCF85063; the part marking has not been read. With
[onboard_35](../examples/onboard_35/README.md) on November 10, 2026 it accepted
a set, counted one second per second, and held a steady offset from the host
clock (within 1 s) over 40 minutes of resets and reflashes.

**The clock has no backup cell.** The vendor schematic powers it from the
PMU's RTC regulator, and the PMU's backup pin carries only a 2.2 µF capacitor.
With USB unplugged for 7 to 9 seconds and no LiPo fitted, the clock came back
reporting lost power, twice; switching on the PMU's backup charger for three
minutes first made no difference. On a second board with a LiPo fitted, the
clock kept its time through a 10-second unplug, with its offset from the host
clock unchanged within a second. Applications
should check the `integrityLost` result of `Rtc::read()`.
`Battery::setBackupCharge()` exists for boards modified with a rechargeable
backup cell.

## Temperature and humidity

`WsC5_35::Env` (`begin`, `read`) drives the SHTC3 at `0x70`. On November 10,
2026 it returned identity `0x0887`, which matches that part, and read about
28 to 30 °C and 37 % with both checksums valid. It measures the board, which
runs warmer than the room; accuracy is untested.

## IMU

`WsC5_35::Imu` (`begin`, `read`, `end`) drives the 6-axis IMU at `0x6B`, which
identifies as a QMI8658 (`WHO_AM_I` `0x05`). Configured for ±4 g and ±512 dps.
Readings on November 10, 2026, each pose held by hand:

| Pose | Reading |
|---|---|
| Flat, screen up | Z +1.00 g, gyro within ±0.6 dps |
| On edge, USB connector pointing down | X −0.93 g |
| Screen facing the viewer, USB connector to the right | Y −0.97 g |
| Turned over | Z about −1.17 g, gyro peak near 221 dps |

Accuracy and calibration are untested.

## BOOT button

GPIO 28, active low (`WsC5_35::BOOT_BUTTON`). It reads released when untouched.
With [onboard_35](../examples/onboard_35/README.md) on November 10, 2026, two
presses and releases were reported when the owner pressed it twice. GPIO 28 is
also a boot-mode strapping pin, so holding the
button through a reset enters download mode.

## Speaker switch, PWR button, header lines, charge current

Four small controls, all checked on one board on November 10, 2026 with
[extras_35](../examples/extras_35/README.md). The evidence behind the pin
assignments is in the
[hardware findings](HARDWARE-FINDINGS.md#exio5-is-the-speaker-amplifier-enable-exio6-reads-the-pwr-button).

- **Speaker amplifier:** `Expander::setSpeakerAmp(bool)`. On after board
  start-up. Off silences the speaker without touching the codec.
- **PWR button:** `Expander::powerButton(pressed)`. True while held. A long
  press switches the board off (vendor documentation; not tested here).
- **Header lines E8 to E15:** `Expander::headerMode`, `headerWrite`,
  `headerRead`. Inputs until set otherwise; an undriven input reads high.
  E8 and E9 passed a jumper-wire test. Output voltage and drive strength were
  not measured, and E10 to E15 were not tested through the header.
- **Charge current:** `Battery::setChargeCurrentMa(mA)` and
  `Battery::chargeCurrentMa(mA)`. The power chip starts at 200 mA on the board
  tested. The setting was read back, and on a board with a LiPo the battery
  voltage stepped with it, so the charging follows the setting; the current
  was not measured in milliamps. Never set more than the cell's rated charge
  current.

## Camera

Boards fitted with the camera answer at `0x6E`. Its identity registers
`0xFC`/`0xFD` read `39 01`, matching a BF3901. The BSP does not drive the
camera. A bus scan on such a board reads `18 24 34 38 51 6B 6E 70`.
