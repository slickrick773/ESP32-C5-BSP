# 2.8-inch GPS test

**Hardware-tested on November 10, 2026.** With an LC76G module on the board's
I2C socket, the example reached a fix with 7 satellites and HDOP 2.2, read
about 97 KB of NMEA in three minutes with no bus failures, and the touch
controller kept reporting taps correctly on the same bus.

Build with `pio run -d examples/gps_28`. For verified ROM-loader flashing, see
[c5flash](../../tools/c5flash/README.md). Monitor USB serial at 115200. Flash
only to the Waveshare ESP32-C5-Touch-LCD-2.8. No display is rendered.

## Wiring

Use the 4-pin socket labelled **I2C** next to the USB port (GND, 3V3, SDA,
SCL): GND to the module's GND, 3V3 to VCC, SDA to SDA, SCL to SCL. That is the
board's shared bus, SDA 0 / SCL 1. Match wires by the labels, not by colour.

## The address shared with touch

The LC76G uses three I2C addresses: `0x50` for commands, `0x54` for data, and
`0x58`, which it is written to only when recovering or waking after it idles.
On this board `0x58` is also the CST3530 touch controller. The example starts
the driver with `Gps::begin(false)`, which makes it never address `0x58`.

What the test showed:

- The normal read cycle (`0x50` and `0x54` only) worked without `0x58`.
- A touch baseline without the module counted five taps correctly; with the
  module on the bus and streaming, taps were still counted with in-range
  coordinates.
- A loose SCL jumper during one run produced bus failures, and the stream
  recovered by itself each time once contact returned.

Not exercised: the module going idle long enough to need its wake step, which
the driver skips in this mode. Build with `-DGPS28_USE_WAKE_ADDRESS=1` to try
the stock behaviour; that path is untested on this board.

## Output

Once a second: link health (bytes, cycles, failures), satellites in view, and
the position once fixed, then the touch count and last touch position. A fix
needs a view of the sky; indoors by a window it took about two and a half
minutes from a cold start.
