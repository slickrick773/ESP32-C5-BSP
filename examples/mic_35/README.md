# 3.5-inch microphone capture test

**Hardware-tested for level response only (November 10, 2026).** On a 3.5-inch
board the left-channel level stayed at about 23 AC RMS through a silent run
in a quiet room, and rose to about 2059 with claps. No captured
audio has been stored or listened to, so recording quality is unverified.

Build with `pio run -d examples/mic_35`. For verified ROM-loader flashing,
see [c5flash](../../tools/c5flash/README.md). Monitor USB serial at 115200.
Flash only to the Waveshare ESP32-C5-Touch-LCD-3.5.

The test captures three 1-second windows from the onboard microphone through
the ES8311 ADC and prints level statistics for each I2S channel (minimum,
maximum, mean, AC RMS), then stops. Stay quiet for the first window and speak
or clap during the others. It stores and plays no audio, renders no display,
and initializes the backlight off.

## What the output can and cannot show

- The microphone data arrives on the left I2S slot. The right slot read zero
  in testing and is reported as `(unused slot)`.
- A left channel reported as `FLAT` received a constant value: no signal arrived.
- `CLIPPED` means samples reached full scale. Loud nearby sound did this at
  the default gain.
- RMS that rises when you make noise shows the microphone path responds.
- Neither result establishes that the captured data is intelligible audio.

## Startup and configuration

Same order as [audio_35](../audio_35/README.md): board expander and shared I²C
bus, then the I2S master, then the codec.

- Shared I²C: SDA 27, SCL 26, codec address `0x18`.
- I2S: BCLK 23, LRCK 10, MCU DIN 24; no external MCLK. DOUT is unused.
- Format: 16 kHz, 16-bit stereo read.
- Codec: the playback register sequence, then DAC muted, analog microphone
  input `0x14 = 0x1A`, ADC volume `0x17 = 0xC8`.

The ADC settings come from an existing 3.5-inch port of another firmware,
where its author reported a responding microphone spectrum view. That is
evidence about that port, not about this example.

## Limits

Recording quality, gain suitability, simultaneous playback and capture, and
operation alongside the display are all unverified. The speaker
test is a separate example and is unchanged.
