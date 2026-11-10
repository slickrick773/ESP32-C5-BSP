# 3.5-inch whole-board check

Build with `pio run -d examples/board_check_35`. For verified ROM-loader
flashing, see [c5flash](../../tools/c5flash/README.md). Monitor USB serial at
115200. Flash only to the Waveshare ESP32-C5-Touch-LCD-3.5.

One run after reset exercises every onboard device the BSP drives and prints
a table. Each line is `PASS`, `FAIL`, or `SKIP` where a check does not apply
(no card fitted, no GPS wired). Keep the board still and reasonably quiet for
the six seconds it takes. Nothing is drawn and the backlight stays off.

## Hardware result

On November 10, 2026, on a 3.5-inch board with a LiPo, a microSD card and an
LC76G GPS attached:

```
[board_check_35]  PASS  I/O expander           CH32V006 at 0x24
[board_check_35]  PASS  touch controller       answered
[board_check_35]  PASS  power management       AXP2101: USB present, LiPo connected
[board_check_35]  PASS  real-time clock        advanced 1 s
[board_check_35]  PASS  IMU                    1.01 g total at rest
[board_check_35]  PASS  temperature/humidity   23.0 C, 50.9 %
[board_check_35]  PASS  audio codec            ES8311 configured
[board_check_35]  PASS  speaker to microphone  sent 2999, heard 1640 (quiet 0.6)
[board_check_35]  PASS  microSD                59638 MB card, write/read-back matched
[board_check_35]  PASS  GPS module             LC76G answers at 0x50
[board_check_35] 10 passed, 0 failed, 0 skipped
[board_check_35] result: PASS
```

It passed two consecutive runs. A failing or skipped line was not produced on
this board.

## What each line checks

| Line | Check |
|---|---|
| I/O expander | The CH32V006 answers and the board starts up |
| touch controller | The touch chip answers on the bus |
| power management | The AXP2101 answers; reports USB and LiPo presence |
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
