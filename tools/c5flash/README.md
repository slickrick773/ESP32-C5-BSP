# c5flash

Reliable flash **read / write / info** for ESP32-C5 boards over USB-Serial-JTAG,
built to survive the C5's USB quirks (see
[../../docs/HARDWARE-FINDINGS.md](../../docs/HARDWARE-FINDINGS.md) → *The USB link
stalls on some data patterns*).

On these boards (chip rev v1.2) the esptool stub crashes, so everything runs on
the ROM loader (`--no-stub`), and large transfers stall deterministically at
fixed addresses. `c5flash` keeps each operation small and retries only the piece
that failed.

## Requirements
- Python 3
- **esptool 5.x** — `pip install esptool`, or point `C5FLASH_ESPTOOL` at the binary.
- A C5 board on USB. The port is auto-detected (first `/dev/cu.usbmodem*` on
  macOS, `/dev/ttyACM*` on Linux).

## Usage
```
./c5flash.py write <addr> <file> [<addr> <file> ...]   # writes, then verify-flash
./c5flash.py read  <start> <end> <out.bin>
./c5flash.py info                                       # chip type + base MAC
```
Example (full image):
```
./c5flash.py write 0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 app.bin
```

## How it works
- **write** — splits each image into 512 KB segments, one `write-flash` per
  segment (up to 4 retries, reconnecting with a reset on retry), then a single
  `verify-flash` and a watchdog reset.
- **read** — 64 KB windows; on a stall it retries with a shifted start offset,
  then falls back to 4 KB pieces.

## Status / caveats
- **write + verify** is well exercised — full bootloader + partitions + app in
  ~33 s on the 3.5" board, repeatedly, with no BOOT/RST buttons.
- **read** is *less* exercised end to end — treat it as working-but-unproven.
- Assumes the rev-v1.2 behaviour of always running `--no-stub`.
