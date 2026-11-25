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

On November 15, 2026, on a 2.8-inch board, in Chrome on a Mac:

- On a healthy board the check agreed line for line with the command-line doctor.
- The board was then broken on purpose (bootloader written at `0x0`, none at
  `0x2000`; the command-line doctor counted 36 restarts in 8 seconds). From the
  page, the cure saved the first 64 KB, wrote all four pieces and read each back
  correctly, and the check afterwards said healthy. After an RST press the
  listen step showed the board check's full table.

- A board in that state restarts so fast that its USB port comes and goes (on
  the Mac it was there about 70% of the time; on a user's Windows machine it
  showed in the browser's list for about half a second at a time). The page
  keeps trying for 30 seconds and grabs the board each time it reappears. It
  caught the board after one pick from the list, and, once picked on an earlier
  try, with the list cancelled and nothing picked.

- On a 3.5-inch board that had run other firmware, saving the first 64 KB
  failed at first: the ROM's reply to the 64-byte block at `0x9580` (in the
  saved-settings area) stalls and the chip then stops answering. The same bytes
  read fine when the block starts anywhere else (eleven other offsets tried,
  all worked). The page now reaches the chip again and reads such a block as
  halves of two blocks either side. With that, the cure completed on that
  board, and the saved settings area in the backup file matched an independent
  read with the command-line tool, byte for byte.

On November 25, 2026, on the same 2.8-inch board, in Chrome on a Mac, broken
on purpose with a program that switches USB off as it starts and restarts
every 1 to 4 s (port there about 0.85 s at a time, 14 to 32% of the time):

- Clicking in time was hard. The first two tries were caught and the check
  read, but the cure stopped: the first 64 KB could not be read. The chip had
  left its loader before the cure began. A reset over the chip's own USB port
  does not reset its low-power watchdogs; a board caught while its bootloader
  runs keeps one armed, and in the loader it fires about 8 s later and starts
  the program. Python esptool turns them off on every connection over this
  port; esptool-js 0.7.0 does not for the C5. The page now does it the way
  Python esptool does.
- With that, the page caught the board by itself (2 misses), said `off usb`,
  and after waiting 15 s the cure saved the first 64 KB, wrote all four pieces
  and read each back correctly. A command-line check afterwards: `healthy`,
  start-up steady, board check running.
- `off usb` is new: everything stored looks right but the board keeps dropping
  off USB. The page does not watch the board start, so it uses how many times
  the board slipped away before it was caught (two or more). A board caught at
  the first try can still be called healthy.

Not tried: the other ways a board can be broken, the "restarting" finding from
the listen step, Edge, Linux, and catching a flickering board on Windows. On Windows a
user's board in the off-USB state showed for about 0.3 s, too quick to click;
the command-line doctor is the route there.

## Notes

- The page never uploads a helper program to the chip's memory. Every command
  is one the ROM itself understands.
- After a cure the board stays in download mode. Press RST to start it.
- When listening, the page raises the port's DTR and RTS lines. Without them
  the board's program prints nothing to USB (measured: 0 of 14 lines against 14
  of 14).
