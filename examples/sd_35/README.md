# 3.5-inch microSD test

Build with `pio run -d examples/sd_35`. For verified ROM-loader flashing, see
[c5flash](../../tools/c5flash/README.md). Monitor USB serial at 115200. Flash
only to the Waveshare ESP32-C5-Touch-LCD-3.5, with a FAT-formatted card fitted.

The test mounts the card, prints its type and size, writes a 47-byte file
`/bsp_sd_test.txt`, reads it back, compares it, deletes it, and prints `PASS`
or `FAIL`. Nothing else on the card is touched. No display is rendered and the
backlight stays off.

## Hardware result

On November 10, 2026 a 3.5-inch board with a 29.8 GB SDHC card reported:

```
[sd_35] mounted: SDHC/SDXC, 29850 MB
[sd_35] wrote and read back 47 bytes: match
[sd_35] result: PASS
```

That is one card, one board, at a 4 MHz SPI clock.

## Wiring and startup

The card shares the display's SPI bus: SCLK 6, MOSI 7, MISO 2, with its own
chip select on GPIO 9. `WsC5_35::begin(0)` resets the SPI peripheral and drives
both the display and SD chip selects inactive before the card is used.

## Limits

The display was not running during the test. Using the card while the display
is drawing on the same bus, clocks above 4 MHz, other card types and sizes,
large files, and hot insertion are unverified in this BSP.
