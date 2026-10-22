# Building

Each example under [`examples/`](../examples) is a standalone PlatformIO project
that carries its own board manifest, variant, partition table, TFT_eSPI user
setup and the ESP32-C5 processor patch — so it builds without inheriting any
other project's toolchain. Build from the example's own directory:

```
pio run -e <env>            # compile
pio run -e <env> -t upload  # compile + flash
```

## Toolchain requirements

- **PlatformIO Core 6.2.x.**
- **The `pioarduino` platform fork**, pinned per example in `platformio.ini`
  (the stock `espressif32` platform has no ESP32-C5 support).
- **Python 3.10–3.13 for the interpreter PlatformIO runs under.** This is a real
  trap: the `pioarduino` platform **refuses Python 3.14+** with

  ```
  ERROR: Python version must be between 3.10 and 3.13.
  ```

  and Homebrew's `platformio` formula may already run on its own Python 3.14
  venv, so `/opt/homebrew/bin/pio` fails this check before it compiles a single
  file. (The fleet pins CI to Python 3.13 for the same reason.) If your `pio`
  is on 3.14+, build under a 3.13 virtualenv:

  ```
  python3.13 -m venv ~/.venvs/pio313
  ~/.venvs/pio313/bin/pip install "platformio==6.2.0"
  ~/.venvs/pio313/bin/pio run -e <env>
  ```

## Flashing and console

Console and flashing are over **native USB (USB-Serial-JTAG) at 115200** — there
is no separate USB-UART bridge on these boards. If a board won't auto-enter
download mode: **hold BOOT, tap RST, release BOOT**, then upload.

## The ESP32-C5 TFT_eSPI patch

TFT_eSPI 2.5.43 has no C5 branch. Each display example ships the C5 processor
port (`tft_c5/`, RockBase IoT's port — see that directory's README) and a
`pre:` script that installs it into the downloaded TFT_eSPI and adds the two
dispatch lines. The script **raises** if it can't anchor the patch, because a
silently-unpatched driver is a solid white screen on an otherwise-fine board.
It is idempotent and no-ops once the C5 branch is present.
