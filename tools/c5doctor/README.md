# c5doctor

For an ESP32-C5 board that looks dead: a dark screen, nothing on the display, a
flashing tool that failed part-way. The doctor finds out what is wrong, says so
in plain words, and can put a known-good test image on the board.

**A board like this is very rarely broken.** The chip has a small loader built
into it (the ROM) that cannot be erased. If that loader still answers over USB,
whatever is wrong is in the flash memory, and flash can be rewritten.

## What you need

- A computer with Python 3 (macOS or Linux).
- esptool 5.x: `pip install esptool`.
- A USB cable that carries data. Many cables carry power only; if no port
  appears, try another cable first.
- This repository (the doctor uses [`../c5flash`](../c5flash) to talk to the board).

## 1. Look, change nothing

```
python3 tools/c5doctor/c5doctor.py check
```

`check` writes nothing. The board restarts while it is examined. With more than
one board plugged in, add `--port /dev/...` before `check`.

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

```
python3 tools/c5doctor/c5doctor.py cure ws-c5-28-board-check.zip
```

**This replaces the firmware on the board.** Before writing, the doctor:

- shows what it is about to write and for which board, and asks you to type `yes`;
- saves the board's first 64 KB (bootloader, partition table, start-up choice)
  as a `.bin` file next to the pack.

Then it writes, verifies every byte, restarts the board, and runs the check
again. If the test program is the board check, its table is shown.

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

## If the chip does not answer

1. Try another USB cable, and a port on the computer itself, not a hub.
2. Put the board in download mode by hand: **hold BOOT, tap RST, release BOOT**.
   Run `check` again.
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

Not yet tried on a board: `restarting`, a board that needs download mode by
hand, Linux, and a board that really is beyond repair.

## Tests

```
python3 tools/c5doctor/test/test_c5doctor.py
```

Host only, no board: what the doctor concludes from what it reads, what a cure
writes where, and that it writes nothing when refused.
