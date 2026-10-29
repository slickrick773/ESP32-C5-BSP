# Consumer check — reproducible BSP consumer projects

The complete, buildable consumer programs used to validate the clean-consumer
install (see [../CONSUMER-SETUP.md](../CONSUMER-SETUP.md)). Two paths:

- [`non-display/`](non-display) — bus + expander + touch + GPS + battery, no
  TFT_eSPI. **compile-tested.**
- [`display/`](display) — adds the ST7796 via TFT_eSPI + the C5 display patch.
  **compile-tested; not hardware-verified by this check.**

## Setup (either path)

These projects carry only the consumer-specific parts (`platformio.ini.txt`,
`src/main.cpp`). The config ships as **`platformio.ini.txt`, not `platformio.ini`**,
on purpose: a git install clones the whole BSP repo into the consumer's `libdeps/`,
and a nested `platformio.ini` there makes PlatformIO's build trip
(`FRAMEWORK_LIBS_DIR` resolves to `None`) — so copy it into place when you use it.

From the project dir, copy the scaffolding the library delivers inside its examples
(it IS delivered in a git install, just not auto-configured):

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

Copy the template into place and pin the BSP:

```
cp platformio.ini.txt platformio.ini    # (display path: also copy the patch inputs below)
```

Then edit `lib_deps` in `platformio.ini` to pin the BSP to the tag or commit you
want (the `git+file://` path points at your local clone; a published consumer would
use the GitHub URL instead), and build:

```
pio run
```

## Recorded results

See [../CONSUMER-SETUP.md](../CONSUMER-SETUP.md) → *Recorded evidence* for the exact
validated commit, the resolved toolchain versions, and the build output of each
path.
