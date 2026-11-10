# 2.8-inch audio self-test

**Hardware-tested on November 10, 2026:** passed three of three runs with the
speaker plugged in (microphone tone level 777, 728, 516; harmonics about 2%).

Build with `pio run -d examples/audio_selftest_28`. For verified ROM-loader
flashing, see [c5flash](../../tools/c5flash/README.md). Monitor USB serial at
115200. Flash only to the Waveshare ESP32-C5-Touch-LCD-2.8.

The board plays a half-second 1 kHz tone on its speaker while recording its
own microphone, checks that the tone came back, and prints `PASS` or `FAIL`.
The speaker must be plugged into the Speaker socket. No display is rendered.

It works the same way as the 3.5-inch
[audio_selftest_35](../audio_selftest_35/README.md): I2S in both directions,
microphone on the left slot, the codec's digital copy of the DAC input on the
right. On this board the amplifier is switched on for the test and off again
afterwards.

## Limits

The level heard varies with where the loose speaker sits relative to the
microphone, which is why the three runs differ. A failing case was not
produced on this board. See the 3.5-inch page for what a result means.
