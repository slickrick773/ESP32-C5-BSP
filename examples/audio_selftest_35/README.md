# 3.5-inch audio self-test

Build with `pio run -d examples/audio_selftest_35`. For verified ROM-loader
flashing, see [c5flash](../../tools/c5flash/README.md). Monitor USB serial at
115200. Flash only to the Waveshare ESP32-C5-Touch-LCD-3.5.

The board plays a half-second 1 kHz tone on its own speaker while recording
its own microphone, checks that the tone came back, and prints `PASS` or
`FAIL`. It needs no listener, cable or second device. It renders no display
and keeps the backlight off.

## Hardware result

Three consecutive runs on November 10, 2026 all passed with nearly identical
numbers:

```
[audio_selftest_35] sent to DAC (right slot): quiet 0.0, tone 2998.7 (expected 3000)
[audio_selftest_35] microphone (left slot):   quiet 0.3, tone 997.2, peak 1084
[audio_selftest_35] microphone harmonics:     2nd 0.7%, 3rd 1.0% of the tone
[audio_selftest_35] result: PASS
```

Microphone tone level across the three runs: 997, 997, 1006.

A bench copy with the codec volume forced to zero, so the speaker stayed
silent, reported `FAIL`: the two digital-path checks still passed and the two
microphone checks failed (tone level 2.1). So a dead speaker path is caught.

## How it works

I2S runs in both directions at once. With the codec in its playback
configuration the ADC stays enabled, so one stereo read returns:

- **left slot** — the microphone;
- **right slot** — the codec's digital copy of what was sent to the DAC.

The example compares a silent half second with a tone half second on each
slot. The level is deliberately quiet (PCM peak 3000, codec volume `0xC0`),
where the returned tone measured about 1% harmonics. It is not the listening
level used by [audio_35](../audio_35/README.md).

## What a result means

- `PASS` shows the I2S pins, codec, amplifier, speaker and microphone all
  work together on that board.
- It is a level check at one frequency. It does not measure loudness,
  frequency response or how the speaker sounds to a person.
- The thresholds were chosen from one board. The silent-speaker failure was
  produced on hardware; a dead microphone or a wiring fault was not.
- A loud room could raise the quiet reading; run it somewhere reasonably quiet.
