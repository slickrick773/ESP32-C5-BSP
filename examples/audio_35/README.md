# 3.5-inch speaker test

Build with `pio run -d examples/audio_35`. For verified ROM-loader flashing,
see [c5flash](../../tools/c5flash/README.md). Monitor USB serial at 115200.
Flash only to the Waveshare ESP32-C5-Touch-LCD-3.5.

The test plays three nominal 1 kHz tones (500 ms tone, 1500 ms silence), then
stops automatically. On November 10, 2026 the owner judged them clean and loud
enough, and a laptop-microphone recording measured about 6% harmonics and a
pitch of 995 to 999 Hz. It does not render a display, and initializes the
backlight off.

## Startup and tested configuration

Initialize the board expander and shared I²C bus before starting I2S and
configuring the codec. Adding expander initialization changed the observed
result from silence to clean playback; the specific electrical dependency
has not been isolated. The driver controls codec registers; the example owns
Arduino ESP_I2S and board startup. No dedicated amplifier-enable control is
implemented by the driver.

- Shared I²C: SDA 27, SCL 26, codec address `0x18`.
- I2S: BCLK 23, LRCK 10, MCU DOUT 25; no external MCLK. This example does not
  record; see [mic_35](../mic_35/README.md).
- Tested format: 16 kHz, 16-bit stereo with identical left/right samples.
- Tested tone: 1 kHz, PCM peak 4500; codec volume `0xE0`; DAC register
  `0x04 = 0x10`.

The codec sequence derives from the existing 3.5-inch PorkChop implementation.
No claim is made that the `0x04` register setting was independently
responsible for the result.

## Why 1 kHz at this level

The first version of this test played 440 Hz at PCM peak 12000. The owner
accepted it by ear, but recordings showed it was clipped: its third and fifth
harmonics were stronger than 440 Hz itself. Two things were going on:

- Codec volume `0xE0` adds about 16 dB of digital gain (measured: `0xC0` is
  about six times quieter), so a peak above roughly 4900 overruns the DAC.
- The small speaker barely reproduces 440 Hz. A clean 440 Hz tone at the
  highest undistorted level was judged "very low" by the owner; the clipped
  one only sounded loud because of its harmonics.

At the same undistorted level the owner found higher pitches louder, so the
test now plays 1 kHz at peak 4500. See the board notes
([Speaker level](../../docs/BOARD-WS-C5-35.md#speaker-level)).

## Shutdown and limits

After the test, codec setup failure, task creation failure, or a short I2S
write, the example attempts to mute/zero the DAC and stop I2S. Shutdown errors
are reported over serial; software success alone does not prove physical silence.
An I2S initialization failure returns before codec configuration.

Host tests cover codec register handling and failure paths with mocked I²C.
They do not validate physical wiring or analog behavior. Pitch accuracy,
and fault-injected hardware shutdown remain unverified. Audio with
the display running is covered by [touch_audio](../touch_audio/README.md). Microphone capture is covered by
[mic_35](../mic_35/README.md), tested for level response only. The 2.8-inch
playback implementation is separate.
