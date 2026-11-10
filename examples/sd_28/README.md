# 2.8-inch microSD test

**Hardware-tested on November 10, 2026.** A 29.6 GB SDHC card mounted at 4 MHz
and a 47-byte file was written, read back identical, and deleted.

Build with `pio run -d examples/sd_28`. For verified ROM-loader flashing, see
[c5flash](../../tools/c5flash/README.md). Monitor USB serial at 115200. Flash
only to the Waveshare ESP32-C5-Touch-LCD-2.8, with a FAT-formatted card fitted.

The test mounts the card, prints its type and size, writes
`/bsp_sd_test.txt`, reads it back, compares it, deletes it, and prints `PASS`
or `FAIL`. Nothing else on the card is touched. No display is rendered.

The card shares the display's SPI bus (SCLK 6, MOSI 7, MISO 8) with chip
select on GPIO 23. `WsC5_28::begin(0)` resets the SPI peripheral and drives
both chip selects inactive before the card is used.

## Limits

One card, one board, display idle. Use while the display is drawing, clocks
above 4 MHz and other cards are unverified on this board. The 3.5-inch board's
[sd_35](../sd_35/README.md) page describes a bench test with the display
running.
