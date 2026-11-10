# Contributing

Thanks for looking. This package exists to make the ESP32-C5 boards it covers
boring and reproducible, so the bar for a change is evidence, not volume.

## The one rule

**A capability is only called "supported" once it has run on a real board.**
A clean compile is not a hardware result. When you report or change something,
say which of these you have:

- **Hardware-verified**: you ran it on a board and saw it work. Say which board,
  which chip revision if you know it, and what you observed.
- **Source-verified**: you read the code or the schematic. Not a hardware result.
- **Unverified**: a theory. Welcome, as long as it is labelled.

[docs/HARDWARE-FINDINGS.md](docs/HARDWARE-FINDINGS.md) explains the tags and
shows what a finished entry looks like.

## Reporting a problem or a result

Open an issue with the matching template. The most useful reports include:

- The board (3.5-inch or 2.8-inch) and anything fitted to it: battery, GPS,
  speaker, microSD card.
- The BSP version or commit, and the example or firmware you ran.
- How the board was reset: RST button, power-on, or over USB by a flashing tool
  or serial monitor. It matters; see the USB-reset note in the findings doc.
- The serial log from boot, as text.

Before you paste a log, remove anything that identifies you or where you are:
GPS positions, Wi-Fi network names and MAC addresses, and paths from your own
machine.

## Building and testing

Each example is a standalone PlatformIO project. See
[docs/BUILD.md](docs/BUILD.md) for the toolchain, including the Python version
PlatformIO needs.

```
pio run -d examples/hello_board            # build one example
bash test/host/run.sh                      # host tests, no board needed
python3 tools/c5flash/test/test_c5flash.py # flasher tests, no board needed
```

CI builds every example on each pull request that touches code.

## Pull requests

- Keep a change to one thing.
- If you add an example or a capability, link it from the
  [README](README.md) status table and the [examples index](examples/README.md),
  and add a line to the [changelog](CHANGELOG.md).
- State what you tested on hardware and what you did not. "Builds, not run on a
  board" is a perfectly good thing to write.
- New hardware claims need the observation behind them: a log excerpt, a
  measurement, or a description of what you saw.

## Licence

By contributing you agree that your contribution is released under the
[MIT licence](LICENSE) that covers this repository.
