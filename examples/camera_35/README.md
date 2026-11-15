# 3.5-inch camera

For Waveshare ESP32-C5-Touch-LCD-3.5 boards sold with the camera (a BF3901).
On a board without one the screen and serial log say `camera NOT FOUND at 0x6E`.

Build with `pio run -d examples/camera_35`. For verified ROM-loader flashing,
see [c5flash](../../tools/c5flash/README.md). Monitor USB serial at 115200.

The example identifies the camera, loads its start-up settings, then receives
240x320 RGB565 pictures and draws each one on the display, with a status line
above it. Serial commands: `s` prints the receive counters, `d` sends the
last picture as text. `tools/save_frame.py PORT out.png` (needs pyserial) asks
for a picture and saves it as a PNG.

## Hardware result

On November 15, 2026, on one camera-fitted board:

```
[camera_35] camera BF3901 found, waiting for a picture
[camera_35] camera BF3901: picture received
[camera_35] pictures 162, last receive 106 ms, draw 32 ms
[camera_35] received 163, wrong length 1, wrong header 0, last 155584 bytes, starts FF FF FF 00 FF FF FF 40
```

- Pictures arrived continuously, about seven a second including the draw.
  In that run 162 of 163 passed the header check on all 320 lines; one was
  rejected for its length. A second run rejected 1 of 50.
- The owner saw a live picture on the screen: the right way up with the board
  upright, and not mirrored.
- **The picture is rough and has a pink cast.** A picture pulled over USB
  showed the same cast, so it comes from the camera's settings or lens, not
  from the display. The colour settings have not been tuned.

## Wiring

| Signal | Reaches | Known from |
|---|---|---|
| Control | shared I2C bus, address `0x6E` | read on hardware |
| Data 0, data 1 | GPIO 0, GPIO 1 | vendor schematic; pictures received |
| Pixel clock | GPIO 11 | vendor schematic; pictures received |
| Frame signal (low during a picture) | GPIO 12 | vendor schematic; pictures received |
| Camera clock | 24 MHz oscillator on the board, always running | vendor schematic |
| Reset | resistor and capacitor only | vendor schematic |
| Power-down | expander line EXIO7, pulled low (camera awake) | vendor schematic; not driven or tested |

The camera shares nothing with the display or the microSD card except the I2C
bus. GPIO 11 and 12 are the UART0 pins, so the serial console must stay on USB
(`-DARDUINO_USB_CDC_ON_BOOT=1`) while the camera is in use.

## How it works

The camera sends each picture two bits at a time, most significant bits first,
on its own clock. The ESP32-C5 parallel-input peripheral (PARLIO RX) collects
them into bytes while the frame signal is low. Each picture is 155,524 bytes:
a 4-byte header, then 320 lines of a 6-byte header and 480 pixel bytes.
`Camera::captureFrame()` checks every header and returns the 153,600 pixel
bytes, low byte of each pixel first. The receive buffer comes from internal
memory if there is room, otherwise from PSRAM; the example keeps its picture
in PSRAM.

The driver uses one call from a private header of the framework
(`parlio_rx_unit_trigger_fake_eof`), as Espressif's own camera code does,
because one picture is larger than the peripheral can count. It was built and
run with the pinned Arduino core 3.3.12 (ESP-IDF 5.5.5) only.

The start-up settings are Espressif's BF3901 table as changed by Waveshare for
RGB565; that one file (`ws_c5_35_camera_regs.h`) keeps its Apache-2.0 licence.

## Limits

One board, one resolution and format, indoors. Not tested: colour accuracy,
focus, exposure in bright or dim light, the power-down line, saving pictures
to the card, Wi-Fi or audio running at the same time, long runs, and any
other core version.
