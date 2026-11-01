# 2.8-inch audio listen test

Build with `pio run -d examples/audio_28`. Upload with
`pio run -d examples/audio_28 -t upload --upload-port <port>`, then monitor at
115200 baud. For reliable ROM-loader flashing, see
[tools/c5flash](../../tools/c5flash/README.md).

**Hardware-verified playback, 2026-10-31:** flashed to the connected 2.8-inch
board with `tools/c5flash` (verify OK); the owner confirmed hearing the repeating
beeps generated for 440 Hz at the configured 16 kHz sample rate. Actual
output pitch was not measured. Each beep lasts 0.5 seconds, followed by 1.5 seconds of silence.
The verified settings are codec volume `0xE0` and PCM peak amplitude 1000 (clean listen test; see investigation below).
This confirms audible playback, not recording or measured audio quality.

The example owns Arduino ESP_I2S (bundled with the pinned Arduino core):
16 kHz, 16-bit stereo, identical left/right samples. Start I2S before codec
configuration. BCLK=24, LRCK=25, DOUT=26, DIN=27 (unused for playback), MCLK=-1
(not wired). ES8311 uses BCLK/SCLK as its clock source at I2C address 0x18 on
SDA0/SCL1. The amplifier enable is EXIO3 on the 0x24 expander, not a GPIO.

The BSP audio driver owns only codec registers and amp control; it has no
ESP_I2S or raw i2s_std dependency. Its register sequence follows the local
porkchop-ws-c5/PorkChop_CYD/es8311.h reference. The expander output API preserves
the display/touch reset bits. Recording, battery and GPS are outside this test.

## Corrected data-pin directions

Use MCU-relative names: **DOUT=GPIO26** feeds the codec DAC (DSDIN), and
**DIN=GPIO27** receives the codec ADC output. The initial pin list reversed
these. With DOUT=27, the codec ID, register readbacks and I2S writes all
succeeded, but the speaker was silent; changing DOUT to 26 produced audible
beeps. This matches [Waveshare's BSP pin definitions](https://github.com/waveshareteam/ESP32-C5-Touch-LCD-2.8/blob/main/examples/esp-idf/07_audio/components/esp32_c5_touch_lcd_2_8/include/bsp/esp32_c5_touch_lcd_2.8.h).

Temporary register dumps and periodic streaming logs have been removed.
Startup and failure messages remain. the owner confirmed no crackle with the
precomputed waveform and dedicated priority-5 audio task at PCM peak 1000,
codec volume `0xE0`, and DAC OSR `0x20`. Timing telemetry was subsequently
removed, and the cleaned build was rebuilt and flashed with c5flash
(verify OK). the owner confirmed the cleaned build still beeps without crackle
on 2026-10-31. Playback at peak 4000 has not been reverified with the audio task.

## Crackle investigation (uncommitted)

- the owner heard crackling throughout the beep at PCM peak 4000, volume `0xE0`.
- Reducing PCM peak to 1000 made playback quieter but did not remove crackle.
- The oversampling test kept peak 1000 and changed only codec register `0x04` DAC
  oversampling from `0x10` to `0x20`, matching the 16 kHz / 4096000 Hz
  coefficient row in [Espressif's ES8311 driver](https://github.com/espressif/esp-adf/blob/master/components/esp_codec_dev/device/es8311/es8311.c)
  (source blob `b563bae92e4bf69b73d55692348edd97b1d239eb`). The copied
  porkchop sequence used `0x10`. Build and host tests pass; c5flash verification
  passed. the owner confirmed that crackle remained after this correction.

- The successful diagnostic changed PCM delivery: the half-second waveform is
  precomputed in internal static memory and fed by a priority-5 task, with
  no trigonometry or steady-state logging in the producer. Peak remains 1000,
  volume remains `0xE0`, DAC OSR remains `0x20`. That diagnostic build reported block
  counts, time between write return and next write call, and short writes.
  This timing measurement is not a direct DMA-underrun counter. Build and
  c5flash verification passed. Live results: zero short writes, maximum feed
  gap 22 microseconds; the owner confirmed no crackle on 2026-10-31. This supports
  keeping precomputation and the dedicated task together; it does not isolate
  which change removed the crackle. Temporary timing logs were then removed.

## Failure shutdown and verification limits

Normal playback still uses the hardware-tested precomputed waveform and
priority-5 task at PCM peak 1000. After codec setup failure, task creation
failure, or a short I2S write, the example retries `Audio::end()` until the
expander acknowledges amp-disable. It attempts zero-sample writes between
retries and yields even if writes fail immediately. It does not resume the
tone or call I2S `end()` while amp-disable is unconfirmed. A persistent fault
keeps this recovery loop active; if I2S writes also fail, silence and continued
physical clocks cannot be guaranteed. Error messages report that uncertainty.
I2S shutdown failure after amp-disable is also reported.

`Audio::end()` only disables the amplifier; it does not mute/power down the
codec. A successful I2C write is an acknowledgement, not electrical proof of
PA state. Runtime codec/expander operations use the shared-bus lock; initial
`Expander::begin()` is not locked and must run before concurrent bus users.
The example follows that startup ordering.

Host tests exercise the real shutdown helper with transport failures, including
lock/NACK recovery, persistent faults, failed silence writes, and shutdown
ordering. They do not establish physical wiring or analog behavior. Pin and
codec-setting regressions can be caught by independent vendor-source review;
real hardware tests are still needed to confirm operation. The shutdown change
was built, host-tested and flashed with c5flash (verify OK). the owner confirmed this build still plays without crackle on 2026-10-31;
hardware fault injection has not been performed.
