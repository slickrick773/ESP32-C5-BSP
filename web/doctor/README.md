# Web doctor

The [board doctor](../../tools/c5doctor/README.md) as a web page: check and cure
an ESP32-C5 board from desktop Chrome or Edge, with nothing to install. It talks
to the chip's ROM over Web Serial; what it reads stays in the browser.

| File | What it is |
|---|---|
| `index.html`, `app.mjs` | The page |
| `doctor.mjs` | What it concludes from what it reads. Same findings and words as `c5doctor.py` |
| `rom.mjs` | The C5 ROM commands the pinned library gets wrong or lacks |
| `vendor/` | Espressif's esptool-js 0.7.0, unchanged, with its licences |
| `assemble.py` | Builds the folder a web server serves, from cure packs |
| `test/` | Host tests: `node --test web/doctor/test/doctor.test.mjs` |

## Build and try it

```
python3 web/doctor/assemble.py site ws-c5-35-board-check.zip ws-c5-28-board-check.zip
cd site && python3 -m http.server 8765 --bind 127.0.0.1
```

Then open `http://127.0.0.1:8765` in Chrome. The cure packs are the ones
attached to a release, or made with `c5doctor.py pack`.

## What was tried on hardware

On November 15, 2026, on a 2.8-inch board, in Chrome on a Mac: the check agreed
line for line with the command-line doctor; the cure saved the first 64 KB,
wrote all four pieces and read each back correctly; after an RST press the
listen step showed the board check's full table.

Not tried: the 3.5-inch board, a board broken on purpose, a board that keeps
restarting, download mode entered by hand, Edge, Windows and Linux.

## Notes

- The page never uploads a helper program to the chip's memory. Every command
  is one the ROM itself understands.
- After a cure the board stays in download mode. Press RST to start it.
- When listening, the page raises the port's DTR and RTS lines. Without them
  the board's program prints nothing to USB (measured: 0 of 14 lines against 14
  of 14).
