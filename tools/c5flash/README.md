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
- A C5 board on USB. The port is auto-detected when exactly one
  `/dev/cu.usbmodem*` (macOS) or `/dev/ttyACM*` (Linux) is present; with none or
  several, pass `--port DEV` (see [Port selection](#port-selection)).

## Usage
```
./c5flash.py [--port DEV] write <addr> <file> [<addr> <file> ...]  # writes, then verify
./c5flash.py [--port DEV] read  <start> <end> <out.bin>
./c5flash.py [--port DEV] info                                      # chip type + base MAC
```
Example (full image):
```
./c5flash.py write 0x2000 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 app.bin
```
**The bootloader goes at `0x2000` on the ESP32-C5**, not `0x0` like the S3/C3/C6 —
the 8 KB below it is reserved for the flash-encryption key manager, and the C5 ROM
loads the second-stage bootloader from `0x2000`. Writing it at `0x0` leaves `0x2000`
holding bootloader *code*, so the ROM reads a non-`0xE9` word there, prints
`invalid header` and reboot-loops to a dark screen. `pio run -t upload` handles this
automatically (the PlatformIO `espidf.py` builder sets `upload.bootloader_offset` =
`0x2000` for `esp32c5` / `esp32p4`); only hand-flashing needs the explicit offset.
Partition table stays at `0x8000`, app at `0x10000`. See
[../../docs/HARDWARE-FINDINGS.md](../../docs/HARDWARE-FINDINGS.md) → *Bootloader
offset is `0x2000` on the C5*.

## How it works
- **write** — splits each image into 512 KB segments, one `write-flash` per
  segment (up to 4 retries, reconnecting with a reset on retry), then a single
  `verify-flash` and a watchdog reset. Writes with explicit
  `--flash-mode dio --flash-freq 80m` so the bootloader header is patched
  correctly — **`keep` leaves a bad flash-mode byte and the C5 ROM rejects it**
  (`invalid header` → reboot loop → every build boots to a dark screen). Override
  with `C5FLASH_MODE` / `C5FLASH_FREQ` / `C5FLASH_SIZE` if your board differs.
- **read** — 64 KB windows; on a stall it retries with a shifted start offset,
  then falls back to 4 KB pieces.

## Status / caveats
- **write + verify** is well exercised — full bootloader + partitions + app in
  ~33 s on the 3.5" board, repeatedly, with no BOOT/RST buttons.
- **read** is *less* exercised end to end — treat it as working-but-unproven.
  Its chunking (including partial final chunks), short/failed responses, and the
  port handling are covered by host tests.
- Assumes the rev-v1.2 behaviour of always running `--no-stub`.

## Port selection

The port is resolved **once** per invocation and reused for every chunk and
retry. Auto-detection requires exactly one `/dev/cu.usbmodem*` (macOS) or
`/dev/ttyACM*` (Linux); with none or several it stops and asks for `--port DEV`
rather than guessing. An empty `--port=` is rejected, not treated as auto.

This retains the selected port **path** for the whole operation — not a
guarantee that the path still maps to the same physical device if the board
re-enumerates mid-operation (that is the cue to re-run).

## Tests

Host-only, no hardware (the port glob and the esptool transport are mocked):

```
python3 test/test_c5flash.py
```
