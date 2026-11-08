# Consumer check — reproducible BSP consumer projects

The complete, buildable consumer programs used to validate the clean-consumer
install (see [../CONSUMER-SETUP.md](../CONSUMER-SETUP.md)). Two paths:

- [`non-display/`](non-display) — bus + expander + touch + GPS + battery, no
  TFT_eSPI. **compile-tested.**
- [`display/`](display) — adds the ST7796 via TFT_eSPI + the C5 display patch.
  **compile-tested; not hardware-verified by this check.**

## Setup (either path)

These projects carry only the consumer-specific parts (`platformio.ini`,
`src/main.cpp`). From the project dir, copy the scaffolding the library delivers
inside its examples (it IS delivered in a git install, just not auto-configured):

```
cp    <bsp>/examples/hello_board/boards/ws-c5-35.json        boards/
cp -R <bsp>/examples/hello_board/variants/ws-c5-35           variants/
cp    <bsp>/examples/hello_board/partitions_ota_16mb.csv     .
```

The **display** path additionally needs the patch and its inputs:

```
cp    <bsp>/examples/hello_board/apply_c5_tft_patch.py       .
cp -R <bsp>/examples/hello_board/tft_c5                      .
cp    <bsp>/examples/hello_board/wsc5_user_setup.h           .
```

Edit `platformio.ini` so `lib_deps` pins the BSP to the tag or commit you want
(the `git+file://` path points at your local clone; a published consumer would use
the GitHub URL instead). Then:

```
pio run
```

## Recorded results

See [../CONSUMER-SETUP.md](../CONSUMER-SETUP.md) → *Recorded evidence* for the exact
validated commit, the resolved toolchain versions, and the build output of each
path.

## Follow-up validation — 2026-11-07

The earlier follow-up builds of both isolated consumers used a local source
snapshot. They did not exercise the CI workflow's Git-pinned dependency install.

A separate check assembled fresh consumer projects with the workflow's board,
variant, partition and display-patch inputs, then installed the BSP through
`git+file://<checkout>#367e8f5a85016556f563f0836b1f364f2b8e127e`.
Both installed dependency repositories resolved to that exact commit. The display
consumer included the pending first-frame reveal change; BSP library sources came
from the pinned commit, not the uncommitted checkout.

The checks use PlatformIO 6.2.0 and a separate core copied from a warm toolchain
cache. This is local Git dependency resolution and compilation, not a cold-cache
installation, a GitHub-hosted Ubuntu workflow run, or physical acceptance. The
future release commit's Git install remains untested until that commit exists.

Both fresh projects passed again with Python 3.13.16 (matching CI's Python 3.13
series), PlatformIO 6.2.0, Arduino 3.3.12 and framework libraries
5.5.5+sha.b774170ff46. Non-display used 352,726 bytes of flash; display used
379,914 bytes. An earlier local run also passed under Python 3.14.7; the
Python 3.13 repeat is the CI-version comparison. All are compile-only checks.
