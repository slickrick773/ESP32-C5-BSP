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
> the owner's working umbravox 2.8" port and now **run from this library**, so the
> display + touch phase is legitimately "supported" under the BSP's own law.
> **Speaker playback is also hardware-verified** (the owner heard `audio_28` beeps,
> 2026-10-31). **Battery voltage readout and continued battery-only operation are verified;
> voltage accuracy, recording and GPS remain unverified** (see the notes
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
| I/O expander (CH32V006) | `0x24` on the touch bus — LCD reset (EXIO1), touch reset (EXIO0), speaker amp enable (EXIO3), backlight PWM | **no EXIO5 panel rail** on this board |
| Audio (ES8311) | I²C `0x18`; BCLK **24**, LRCK **25**, MCU DOUT **26**, MCU DIN **27** (recording unverified); **no MCLK**, clock from BCLK; amp enable **EXIO3** | distinct 2.8-inch pinout |
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
  "connect-the-dots"; at 8 ms the stroke is smooth (verified with the owner on hw).

The host test `test/host/test_touch_28.cpp` exercises this decode (short-read
rejection, a valid contact, finger-up, out-of-range, and the landscape map)
against the real driver.

## Onboard I²C address map (verified by bus scan, 2026-10-31)

A scan of the SDA 0 / SCL 1 bus on the real board (expander brought up first)
found six devices:

| Addr | Device |
|---|---|
| `0x18` | **ES8311 audio codec** — same codec as the 3.5" |
| `0x24` | CH32V006 I/O expander *(verified)* |
| `0x51` | RTC |
| `0x58` | CST3530 touch *(verified)* |
| `0x6B` | IMU |
| `0x70` | onboard sensor |

What this tells the later phases, grounded in the scan rather than the 3.5"'s
parts list:

- **Audio = ES8311 at `0x18`**. Speaker playback is now verified with
  `audio_28`; see the corrected pin directions and playback details below.
- **Battery voltage uses the expander ADC**, not the 3.5-inch AXP2101.
  Waveshare's battery example reads two little-endian bytes from register
  `0x06` at `0x24`; the 10-bit sample uses nominal 3.3 V / 3:1 scaling.
  This corrects the earlier speculation that no I2C battery register existed.
  See [`battery_28`](../examples/battery_28/README.md) for the pinned source,
  conversion, failure behavior, and hardware-validation limits. Voltage is
  not a battery percentage or charge-state measurement.
- **GPS:** the LC76G is not wired in this scan. When it is, note that its `0x58`
  "wake" address **clashes with the CST3530 touch** already at `0x58` — resolve
  before adding GPS (confirm the LC76G's address set on this board).
- RTC `0x51`, IMU `0x6B` and the `0x70` sensor are onboard extras not yet
  wrapped as BSP drivers.

The bus scan alone does not verify battery or GPS operation. The battery
readout and USB-disconnect test are documented below; GPS remains unverified.

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

## Audio: ES8311 speaker playback

**Hardware-verified playback, 2026-10-31.** `audio_28` was built from this BSP
and flashed with `tools/c5flash` (verify OK). the owner confirmed audible repeating
beeps on the connected speaker: 0.5 seconds on, 1.5 seconds silent. The
waveform targets 440 Hz at the configured 16 kHz sample rate; actual output
frequency was not measured.
This is a listen test, not a measurement of audio quality or a recording test.

- ES8311 control: `0x18` on SDA0/SCL1, using the shared bus lock.
- I2S: **BCLK=24, LRCK=25, DOUT=26 (MCU → codec DSDIN), DIN=27
  (codec ADC → MCU, unused for playback)**. **MCLK is not wired**; the codec
  uses BCLK/SCLK. The initial pin list had DOUT/DIN reversed: DOUT=27 produced
  silence despite successful codec readbacks and I2S writes. DOUT=26 produced
  audible beeps, matching [Waveshare's BSP](https://github.com/waveshareteam/ESP32-C5-Touch-LCD-2.8/blob/main/examples/esp-idf/07_audio/components/esp32_c5_touch_lcd_2_8/include/bsp/esp32_c5_touch_lcd_2.8.h).
- Start **Arduino ESP_I2S** before configuring the codec: 16 kHz, signed 16-bit
  stereo, identical samples in both channels. Verified volume is `0xE0`, with
  PCM peak amplitude 1000 and 5 ms tone ramps. Send zero samples in the gaps.
- Amp enable is **EXIO3 (PA_CTRL)** on the `0x24` expander, not a GPIO.
  `Expander::setOutput()` preserves the other output bits and reports failures.
- `src/board/ws_c5_28/ws_c5_28_audio.*` owns codec registers and amplifier
  control only; ESP_I2S and tone generation stay in `examples/audio_28`.
  The register sequence follows the proven `porkchop-ws-c5` no-MCLK path.

The first playback implementation crackled throughout the beep. Reducing PCM
peak from 4000 to 1000 did not resolve it. Correcting DAC OSR register `0x04`
from `0x10` to Espressif's 16 kHz value `0x20` did not resolve it either.

**the owner confirmed no crackle on 2026-10-31** after precomputing the waveform and
feeding ESP_I2S from a dedicated priority-5 task, at peak 1000 / volume `0xE0`
/ DAC OSR `0x20`. Telemetry reported no short writes and a maximum 22 µs gap
between a write returning and the next write call. This supports the combined
PCM-delivery change; it does not prove whether scheduling or computation was
the individual cause. Peak 4000 has not been reverified with this task.

Temporary timing logs were removed after that listen test. The final logging
cleanup was rebuilt and flashed with c5flash (verify OK). the owner confirmed
the cleaned build still beeps without crackle on 2026-10-31. See
[audio_28](../examples/audio_28/README.md) for build/upload steps and test history.

**Not yet verified:** recording, battery-voltage accuracy, charging status and GPS. The LC76G `0x58`/touch clash
is still unresolved. Display/touch and audio were verified in separate examples;
simultaneous operation has not been claimed.

### Shutdown behavior and evidence limits

The example now retries amp-disable on codec setup failure, task creation
failure, or short I2S writes. It attempts silence while retrying and only calls
I2S shutdown after an acknowledged amp-disable write. Persistent I2C failure
leaves the retry loop active; simultaneous I2S failure means neither silence
nor physical clock continuity can be guaranteed. This failure-path change is
build/host-tested and flashed with c5flash (verify OK). the owner confirmed this build still plays without crackle on 2026-10-31;
hardware fault injection has not been performed. Normal
playback generation, task priority, pin mapping, and codec settings are unchanged.

`Audio::end()` disables only the amp, not the codec. Runtime bus operations are
locked, but expander initialization must occur before concurrent bus clients.
Host mocks check software behavior under injected faults, not physical wiring.
Vendor-source review supports pin/register choices; the listen test confirms
audible, crackle-free playback at the documented level, not ADC capture or
measured frequency/distortion.

## Battery voltage and battery-only check

The `battery_28` example reads the expander ADC and displays nominal voltage,
raw samples, a running seconds counter and a blinking indicator. The serial-only
build returned raw 425–426 (4113–4123 mV estimated) with USB and battery connected.
The screen build was flashed with verification OK; the owner confirmed continued
screen/counter updates after USB was unplugged. Voltage accuracy, cold-start on
battery, charging state and runtime capacity remain unverified. No battery
percentage is claimed. GPS wiring and operation remain a separate phase.
