# c5doctor

For an ESP32-C5 board that looks dead: a dark screen, nothing on the display, a
flashing tool that failed part-way. The doctor finds out what is wrong, says so
in plain words, and can put a known-good test image on the board.

**No Python?** The same doctor runs as a web page in desktop Chrome or Edge,
with nothing to install: <https://slickrick773.github.io/ESP32-C5-BSP/>. Tried
on a 2.8-inch board in Chrome on a Mac, 2026-11-15, including a board broken on
purpose; not yet tried on the 3.5-inch board, Edge, Windows or Linux. Its source
is in [web/doctor](../../web/doctor/README.md).

**A board like this is very rarely broken.** The chip has a small loader built
into it (the ROM) that cannot be erased. If that loader still answers over USB,
whatever is wrong is in the flash memory, and flash can be rewritten.

## What you need

- A computer with Python 3: macOS or Linux, or Windows (host-tested only, not yet tried on a Windows machine).
- esptool 5.x: `pip install esptool`. That also installs pyserial, which the
  doctor uses to find the board's port by its USB maker (on Windows a COM
  port such as `COM4`) and to catch a board that is on USB only for a moment.
- A USB cable that carries data. Many cables carry power only; if no port
  appears, try another cable first.
- This repository (the doctor uses [`../c5flash`](../c5flash) to talk to the board).

## 1. Look, change nothing

```
python3 tools/c5doctor/c5doctor.py check
```

`check` writes nothing. The board restarts while it is examined. With more than
one board plugged in, add `--port /dev/...` (on Windows `--port COM4`) before
`check`.

On Windows, type `python` instead of `python3` and use backslashes:
`python tools\c5doctor\c5doctor.py check`.

What you should see is a short report and one finding:

| Finding | What it means | Fixed by `cure` |
|---|---|---|
| `healthy` | The chip answers and a bootloader, partition table and program are in place. Not bricked. | n/a |
| `bootloader at zero` | A bootloader was written at `0x0`, where other ESP32 chips keep it. The C5 looks at `0x2000`. The usual result of a tool set up for another chip. | yes |
| `no bootloader` | Nothing to start from at `0x2000`. | yes |
| `no table` | The partition table is missing or damaged. | yes |
| `no app` | No program where the partition table points. | yes |
| `restarting` | Everything stored looks right but the program crashes at start-up, over and over. | yes |
| `no answer` | The chip did not answer at all. See below. | no |

## 2. Put a known-good image on it

A **cure pack** is a zip holding a bootloader, a partition table and a test
program for one board, with a manifest that is checked before anything is
written. The test program is this repository's board check: it tries every
device on the board and prints a PASS/FAIL table.

Ready-made packs are attached to the
[latest release](https://github.com/slickrick773/ESP32-C5-BSP/releases/latest):
`ws-c5-35-board-check.zip` for the 3.5-inch board and `ws-c5-28-board-check.zip`
for the 2.8-inch board. Download the one for your board and leave it zipped.

```
python3 tools/c5doctor/c5doctor.py cure ws-c5-28-board-check.zip
```

**This replaces the firmware on the board.** Before writing, the doctor:

- shows what it is about to write and for which board, and asks you to type `yes`;
- saves the board's first 64 KB (bootloader, partition table, start-up choice)
  as a `.bin` file next to the pack.

Then it writes, verifies every byte, restarts the board, and runs the check
again. If the test program is the board check, its table is shown, and the
board draws the same table on its own screen and turns the backlight on, so
you can see it is alive.

**Use the pack for your board.** The chip cannot tell the doctor which board it
is soldered to. A pack for another board is written just the same and can leave
the screen dark; nothing is harmed, and the right pack fixes it.

### Making a cure pack

Build one of the board checks, then pack the build folder:

```
cd examples/board_check_28 && pio run && cd ../..
python3 tools/c5doctor/c5doctor.py pack examples/board_check_28/.pio/build/board_check_28 ws-c5-28 ws-c5-28-board-check.zip
```

Any PlatformIO build for the C5 can be packed the same way.

A build carries the folder paths of the computer it was made on. Before sharing
a pack, build with those mapped away, for example
`PLATFORMIO_BUILD_FLAGS="-ffile-prefix-map=$HOME=/b" pio run`.

## If the board is on USB only for a moment

Some boards show up for less than a second and vanish again, over and over: a
program that keeps restarting, or one that switches USB off as it starts. A web
page or a flashing tool cannot be pointed at a port that quick. Run `check` or
`cure` with the board plugged in and leave it: for up to 15 seconds the doctor
waits for the port, and the moment it appears sends the reset that holds the
chip in its loader, where USB stays steady. If nothing happens in that time,
tap RST once while it waits.

## If the chip does not answer

1. Try another USB cable, and a port on the computer itself, not a hub.
2. Put the board in download mode by hand: **hold BOOT, tap RST, release BOOT**.
   Run `check` again. On one 2.8-inch board (2026-11-25) holding BOOT at
   start-up, at power-on or with RST, left no USB device at all, on two ports
   of a Mac; on that board use the catch above instead.
3. If it answers now, run `cure`.

If the chip never answers, even in download mode with a good cable, nothing sent
over USB can repair it. The doctor says so; it does not guess.

## What has been tried on a board

On 2026-11-14 two working boards were broken on purpose and cured each time:

| Board | Break | `check` said | After `cure` |
|---|---|---|---|
| 3.5-inch | bootloader at `0x0`, blank at `0x2000` | `bootloader at zero`, 48 restarts in 8 s | `healthy`, board check PASS |
| 3.5-inch | partition table blanked | `no table`, 219 restarts in 8 s | `healthy`, board check 9 passed, 0 failed, 1 skipped |
| 2.8-inch | bootloader at `0x0`, blank at `0x2000` | `bootloader at zero`, 36 restarts in 8 s | `healthy`, board check 8 passed, 1 failed, 1 skipped |
| 2.8-inch | program blanked | `no app`, 151 restarts in 8 s | `healthy`, board check 8 passed, 1 failed, 1 skipped |

The one failed line on the 2.8-inch board was speaker to microphone, with no
speaker connected. A board check that fails is about the board's parts, not
about how it was flashed; the doctor says so.

On 2026-11-25 a 2.8-inch board was given a program that switches USB off as
it starts and restarts every 1 to 4 s, copying a user's board: its port showed
for about 0.85 s at a time, 21 to 32% of the time. The user's showed for about
0.3 s on Windows, which this Mac could not make that short. Three rounds of
breaking and `cure` with nothing pressed: each time the doctor reached the chip,
saved the first 64 KB, wrote and verified, and the board check ran (8 passed,
1 failed, 1 skipped: no speaker). On this break `check` wrongly says `healthy`,
because the program is fine as stored and the doctor cannot yet tell that it
turns USB off.

Not yet tried on a board: `restarting`, Windows, a board that needs download mode by
hand, Linux, and a board that really is beyond repair.

## Tests

```
python3 tools/c5doctor/test/test_c5doctor.py
```

Host only, no board: what the doctor concludes from what it reads, what a cure
writes where, and that it writes nothing when refused.
