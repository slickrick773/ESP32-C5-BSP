# Touch and redraw

A Waveshare ESP32-C5-Touch-LCD-3.5 sample using the BSP and an included UI renderer. Build with `pio run -d examples/touch_redraw`.

Drag the center list to scroll or tap a row to highlight it. Tap TOGGLE to
change its state, or COUNT to increment the counter. The first
complete frame triggers the backlight reveal. Later taps repaint only the footer
while the two controls keep stable hit regions. List motion repaints the list
viewport, and the latest drag offset is applied after each frame.

The example includes a strip renderer, bitmap font, touch gesture handling, and
a TFT_eSPI display adapter. It paints changed regions after the first frame.

## Board check before adding the list

On 2026-11-07, the Waveshare C5 3.5 application write at `0x10000` verified.
The user confirmed that Toggle changes state and Count increments by one per tap.
One cold power-up and one physical reset showed no visible fuzzy flash or
retained frame. These observations apply to this sample image. The separate
hello_board startup findings remain documented in HARDWARE-FINDINGS.md.

Application SHA-256: `46112040b8bc3a6f731a42bf772fabaddc6878caa5a84da6fdd3c61a58ce5f99`.

## Scrolling build board check

The application write verified. The user confirmed cold boot and physical reset
showed no visible fuzzy flash or retained frame. Scrolling and Toggle worked on
recheck after an initial intermittent Toggle miss.

Tested application SHA-256: `e8e57ee9ea70fe8e667ed85611b7f3f18e7f9db54e05ae2a01224c267c52e572`.

The source rename for publication produced a different application binary that
compiled successfully. The cold-boot and physical-reset observations above apply
only to the tested SHA-256, not to that rebuilt publication image.

## Published-source startup recheck — 2026-11-07

A clean build from published source commit
`367e8f5a85016556f563f0836b1f364f2b8e127e` was installed application-only and
verified with c5flash. The user confirmed clean startup in response to the
cold-boot and physical-reset check, reporting that it comes up great.
This is acceptance of this specific image; it does not resolve the separate
Hello Board startup findings or establish their electrical cause.

Tested application SHA-256:
`8b8c3a57a4f0eb3dfd76ca6eb5d3e61c3d2e50c3448a495ae83e5d31dd833acf` (412,384 bytes).
