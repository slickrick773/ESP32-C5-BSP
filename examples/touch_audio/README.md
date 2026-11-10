# Touch and audio

**Hardware-tested on November 10, 2026.** The owner heard a clean tone while
scrolling the list, the list kept moving during the tone, and the screen
looked normal.

A Waveshare ESP32-C5-Touch-LCD-3.5 sample that combines the display, touch,
speaker and microphone. Build with `pio run -d examples/touch_audio`.

The scrolling list, renderer and first-frame backlight reveal are those of the
[touch and scrolling sample](../touch_redraw/README.md). The two footer
controls are:

- **TONE** — plays a one-second, nominal 440 Hz tone from a background task
  and counts it. The list keeps scrolling and redrawing while it sounds.
- **MIC** — captures one second from the microphone and shows the level (AC
  RMS of the left I2S slot), with `CLIP` if any sample reached full scale.

MIC runs on the main loop, so touch and redraw pause for its one second.
Playback and capture are one at a time: MIC is ignored while a tone plays.

The audio startup order and settings are those of the standalone
[audio_35](../audio_35/README.md) and [mic_35](../mic_35/README.md) examples,
which remain the hardware-tested references.

## Hardware result

- Four tones were tapped while the owner dragged the list; each was logged as
  played, sounded clean, and the list moved during it.
- A bench build played the tone with and without continuous full-screen
  redraws (five per tone). Laptop-microphone recordings showed the same steady
  level in both cases (about 4% variation).
- On the final image MIC read 22 in a quiet room and 1345 (`CLIP`) during a
  clap, and a tone played afterwards.

Tested application SHA-256:
`378c2169a58a26a2dbbb8fa6d1139da15ae8eb2d7d0d96cc1f07ce653819978e`.

## A problem found and fixed

The first version computed the tone sample by sample while playing. On this
chip that took about 24 ms for each 16 ms block of audio, so the I2S stream
ran dry and the tone pulsed; the owner described it as like a rotary phone
dialling, and a recording showed the level varying by about 20%. The tone is
now rendered once at startup and played from its own task. Generate audio ahead of time, or measure that
your generator keeps up, before streaming it.

In that first session some taps had to be repeated; the owner noted this may
have been how the controls were tapped. In the retest every tap registered.

## Limits

Long sessions, and audio while the
SD card or other bus devices are busy, are untested. The tone uses the
audio_35 listening level, which measurements show is driven into clipping;
see the board notes.
