# 2.8-inch touch and audio

**Hardware-tested on November 10, 2026.** The owner heard a clean tone, saw the
on-screen counter keep running while it played, confirmed the layout looked
right, and read `mic 2500 CLIP` on screen after clapping.

A Waveshare ESP32-C5-Touch-LCD-2.8 sample that uses the display, touch,
speaker and microphone together. Build with
`pio run -d examples/touch_audio_28`. The speaker must be plugged in.

- **TONE** plays a one-second, nominal 440 Hz tone from a background task. A
  counter in the corner is redrawn ten times a second, so you can see the
  display keep running while the tone sounds.
- **MIC** captures one second from the microphone and shows the level, with
  `CLIP` if any sample reached full scale. The screen pauses for that second.

Playback and capture are one at a time. The display uses Arduino_GFX with the
ST7789 setup from [hello_board_28](../hello_board_28/); the audio level is the
one [audio_28](../audio_28/README.md) was verified at (PCM peak 1000, codec
volume `0xE0`).

## Hardware result

- Serial logged two tones played and a quiet MIC reading of 46.
- The tone is rendered once at startup. Generating samples while streaming is
  slower than real time on this chip and makes a tone pulse; see the 3.5-inch
  [touch_audio](../touch_audio/README.md) page.

Tested application SHA-256:
`1c6d64d42562014b2b7f25ed592085589a8892e86ef20419a54cdb0f011e119a`.

## Limits

Long sessions, and audio while the SD card or GPS is busy, are untested.
