# Touch and audio

**Hardware-tested on November 10, 2026.** With the display running, the owner
heard a clean tone from TONE, and TONE and MIC both responded to taps.

A Waveshare ESP32-C5-Touch-LCD-3.5 sample that combines the display, touch,
speaker and microphone. Build with `pio run -d examples/touch_audio`.

The scrolling list, renderer and first-frame backlight reveal are those of the
[touch and scrolling sample](../touch_redraw/README.md). The two footer
controls are:

- **TONE** — plays one 400 ms, nominal 440 Hz tone and counts it.
- **MIC** — captures one second from the microphone and shows the level (AC
  RMS of the left I2S slot), with `CLIP` if any sample reached full scale.

Each audio action starts I2S, runs to completion, and stops I2S again. Touch
and redraw pause while it runs. Playback and capture are one at a time.

The audio startup order and settings are those of the standalone
[audio_35](../audio_35/README.md) and [mic_35](../mic_35/README.md) examples,
which remain the hardware-tested references.

## Hardware result

- Serial logged every TONE and MIC action as successful, with touch still
  responding after each.
- The owner judged the tone clean. A laptop-microphone recording of it showed
  a steady level (about 3% variation across the tone), matching the standalone
  speaker test recorded the same way.
- MIC read about 20 to 29 in a quiet room.

Tested application SHA-256:
`16a3b75cedab0e4597829e57e4644fc0e90f92951aa861d5d8b77228cca69710`.

## A problem found and fixed

The first version computed the tone sample by sample while playing. On this
chip that took about 24 ms for each 16 ms block of audio, so the I2S stream
ran dry and the tone pulsed; the owner described it as like a rotary phone
dialling, and a recording showed the level varying by about 20%. The tone is
now rendered once at startup. Generate audio ahead of time, or measure that
your generator keeps up, before streaming it.

In that first session some taps had to be repeated; the owner noted this may
have been how the controls were tapped. In the retest every tap registered.

## Limits

Scrolling after an audio action was not specifically confirmed. Audio while
the display is actively redrawing, and long sessions, are untested: each
audio action here runs to completion before the display is touched again.
