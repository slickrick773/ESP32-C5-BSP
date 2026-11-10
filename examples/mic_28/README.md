# 2.8-inch microphone capture test

**Hardware-tested for level response on November 10, 2026.** On a 2.8-inch
board the left-channel level stayed at about 54 to 69 AC RMS through a silent
run and rose to about 1549, clipped, with claps. No captured audio from this
board has been listened to.

Build with `pio run -d examples/mic_28`. For verified ROM-loader flashing, see
[c5flash](../../tools/c5flash/README.md). Monitor USB serial at 115200. Flash
only to the Waveshare ESP32-C5-Touch-LCD-2.8.

The test captures three 1-second windows from the onboard microphone through
the ES8311 ADC and prints level statistics for each I2S channel, then stops.
Stay quiet for the first window and speak or clap during the others. It stores
and plays no audio and renders no display.

## Configuration

- Shared I²C: SDA 0, SCL 1, codec address `0x18`.
- I2S: BCLK 24, LRCK 25, MCU DIN 27; no external MCLK. DOUT is unused.
- The speaker amplifier (expander output 3) is switched off before the codec
  is configured and stays off.
- Codec: the board's playback register sequence, then DAC muted, analog
  microphone input, ADC volume `0xC8`, preamp gain step 10 of 10.

The method and settings are those of the 3.5-inch
[mic_35](../mic_35/README.md) example, where a capture was also played back
and judged clear speech. See that page for what the output can and cannot show.

## Limits

Recording quality on this board, gain suitability, and capture alongside the
display are unverified. Claps close to the board clip at the default gain.
