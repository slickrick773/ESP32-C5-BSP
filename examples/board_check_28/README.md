# 2.8-inch whole-board check

Build with `pio run -d examples/board_check_28`. For verified ROM-loader
flashing, see [c5flash](../../tools/c5flash/README.md). Monitor USB serial at
115200. Flash only to the Waveshare ESP32-C5-Touch-LCD-2.8.

One run after reset exercises every onboard device the BSP drives and prints
a table. Each line is `PASS`, `FAIL`, or `SKIP` where a check does not apply
(no card fitted, no GPS wired). Keep the board still and reasonably quiet for
the six seconds it takes. Nothing is drawn and the backlight stays off.

## Hardware result

On November 10, 2026, with a speaker, a microSD card and an LC76G GPS
attached, a 2.8-inch board reported:

```
[board_check_28]  PASS  I/O expander           CH32V006 at 0x24
[board_check_28]  PASS  touch controller       answered
[board_check_28]  PASS  battery monitor        battery input reads about 4210 mV (not a charge level)
[board_check_28]  PASS  real-time clock        advanced 1 s; time not set since power loss
[board_check_28]  PASS  IMU                    1.05 g total at rest
[board_check_28]  PASS  temperature/humidity   27.7 C, 38.1 %
[board_check_28]  PASS  audio codec            ES8311 configured
[board_check_28]  PASS  speaker to microphone  sent 2999, heard 902 (quiet 0.0)
[board_check_28]  PASS  microSD                29656 MB card, write/read-back matched
[board_check_28]  PASS  GPS module             LC76G answers at 0x50
[board_check_28] 10 passed, 0 failed, 0 skipped
[board_check_28] result: PASS
```

It passed four consecutive runs. The speaker must be plugged in for the audio
line to pass. The battery line read about 4.2 V with no battery fitted, so it
shows the monitor responds, not that a battery is present.

## What each line checks

| Line | Check |
|---|---|
| I/O expander | The CH32V006 answers and the board starts up |
| touch controller | The touch chip answers on the bus |
| battery monitor | The expander's battery-voltage reading returns a value |
| real-time clock | Two reads a little over a second apart differ by one or two seconds |
| IMU | Identity matches and it reads about 1 g at rest |
| temperature/humidity | Identity and checksums are valid and the values are plausible |
| audio codec | The ES8311 identifies and configures |
| speaker to microphone | A quiet 1 kHz tone is played and heard on the board's own microphone |
| microSD | Mount, write, read back, delete; SKIP if no card mounts |
| GPS module | Whether an LC76G answers at `0x50`; SKIP if none |

The display is not checked: it cannot be read back.

## Limits

A pass shows each device responds and the audio path is complete. It does not
measure accuracy, loudness or range. The clock check does not set the time.
The same source file builds for both boards; a build flag selects the board.
