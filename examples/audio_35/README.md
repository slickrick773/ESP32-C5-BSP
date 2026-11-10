# 3.5-inch speaker test

Build with `pio run -d examples/audio_35`. For verified ROM-loader flashing,
see [c5flash](../../tools/c5flash/README.md). Monitor USB serial at 115200.
Flash only to the Waveshare ESP32-C5-Touch-LCD-3.5.

The owner confirmed clean playback on November 10, 2026. The test plays three
nominal 440 Hz tones (500 ms tone, 1500 ms silence), then stops automatically.
It does not render a display, and initializes the backlight off.

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
- Tested PCM peak: 12000; codec volume `0xE0`; DAC register `0x04 = 0x10`.

The codec sequence derives from the existing 3.5-inch PorkChop implementation.
These exact settings were retained for the clean listen test; no claim is made
that this register setting was independently responsible for the result.

## Shutdown and limits

After the test, codec setup failure, task creation failure, or a short I2S
write, the example attempts to mute/zero the DAC and stop I2S. Shutdown errors
are reported over serial; software success alone does not prove physical silence.
An I2S initialization failure returns before codec configuration.

Host tests cover codec register handling and failure paths with mocked I²C.
They do not validate physical wiring or analog behavior. Pitch accuracy,
fault-injected hardware shutdown, and simultaneous display/audio operation
remain unverified. Microphone capture is covered by
[mic_35](../mic_35/README.md), tested for level response only. The 2.8-inch
playback implementation is separate.
