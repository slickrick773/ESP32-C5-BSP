# Consuming the ESP32-C5-BSP

How to depend on this BSP from a PlatformIO project, what an install delivers, and
what has actually been validated. This is the durable record of the clean-consumer
validation (it supersedes the scratchpad notes).

## Evidence levels

This document keeps three claims distinct — do not read one as another:

- **hardware-verified** — run on a real board (see
  [HARDWARE-FINDINGS.md](HARDWARE-FINDINGS.md) for the per-claim tags).
- **compile-tested** — builds cleanly in a fresh consumer project (below). Proves
  resolution + compilation, not runtime behaviour.
- **host-tested** — logic proven by native mocked tests (`test/host/`,
  `tools/c5flash/test/`), no board involved.

## What an install delivers

A PlatformIO **git** dependency clones the whole repo into the consumer's
`libdeps/`, so the install carries `src/`, `examples/`, `docs/`, `tools/`, and
`test/`. The custom **board definition, variant, partition table, and display
patch are therefore delivered** — inside `examples/` — they are simply not
auto-configured into the consumer's project. You copy them from the installed
`examples/` (or from this repo) into your project; see below. (A future *packaged*
release may ship a narrower file set; that is a separate, untested change.)

Drivers are included from the library `src/` root:

```cpp
#include <chip/esp32c5_i2c_bus.h>
#include <board/ws_c5_35/ws_c5_35_board.h>   // also _gps.h, _battery.h, _touch.h, _expander.h
```

The board surface is `WsC5_35::begin()` / `WsC5_35::boardName()` (directly in
`WsC5_35`); peripherals are sub-namespaces (`WsC5_35::Gps`, `::Battery`, `::Touch`,
`::Expander`).

## Pinning the dependency

Pin an exact commit (or a tag). Local git, for an unpublished revision:

```ini
lib_deps =
    ESP32-C5-BSP=git+file:///abs/path/to/ws-c5-bsp#<commit-or-tag>
```

PlatformIO records the pin in the installed revision, e.g.
`ESP32-C5-BSP @ 0.4.0+sha.<commit>`, and `XPowersLib` resolves transitively from
the BSP's `library.json` (the consumer does not declare it).

## Non-display consumer — **compile-tested**

Uses the shared bus + expander + touch + GPS + battery (no display, no TFT_eSPI,
no display patch). This path is **compile-tested** in a clean isolated project.
The complete, buildable program (and the display one) is preserved under
[consumer-check/](consumer-check) — copy it rather than transcribing the steps.

1. Create an isolated project (no `symlink://`, no shared build artifacts).
2. Copy the scaffolding the library delivers but does not auto-configure:
   ```
   cp    <bsp>/examples/hello_board/boards/ws-c5-35.json        boards/
   cp -R <bsp>/examples/hello_board/variants/ws-c5-35           variants/
   cp    <bsp>/examples/hello_board/partitions_ota_16mb.csv     .
   ```
3. `platformio.ini`:
   ```ini
   [env:consumer]
   platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.312-1/platform-espressif32.zip
   board = ws-c5-35
   framework = arduino
   board_build.variants_dir = variants
   board_build.partitions = partitions_ota_16mb.csv
   lib_deps = ESP32-C5-BSP=git+file:///abs/path/to/ws-c5-bsp#<commit-or-tag>
   ```
4. Add `src/main.cpp` — the complete program is
   [consumer-check/non-display/src/main.cpp](consumer-check/non-display/src/main.cpp)
   (includes the drivers above and calls `WsC5_35::begin()`, `Gps::begin()`,
   `Battery::begin()`, `Touch::read()`). Then `pio run`.

### Recorded evidence (isolated core, Python 3.13.16)

Built in a fresh, **unshared** `PLATFORMIO_CORE_DIR` so no other session could
mutate the packages mid-build:

```
PLATFORMIO_CORE_DIR=<fresh-core> <py3.13>/pio run -e consumer           # non-display
PLATFORMIO_CORE_DIR=<fresh-core> <py3.13>/pio run -e consumer_display   # display
```

| Build | Pinned commit (full SHA) | Toolchain | Result | Flash |
|-------|--------------------------|-----------|--------|-------|
| non-display | `b5f827160476065eb08c618115e42f8826d4a600` | fresh (cold download) | **SUCCESS** | 5.6% (353,078 B) |
| non-display | `2f74ad480cc2e02ffade6da6618e3b07b2a0e954` | fresh project, warm core | **SUCCESS** | 5.6% (353,078 B) |
| display | `2f74ad480cc2e02ffade6da6618e3b07b2a0e954` | fresh project, warm core | **SUCCESS** | 6.0% (380,334 B) |

For each build the pin was verified: `pio pkg list` → `ESP32-C5-BSP @ 0.4.0+sha.<short>`,
installed `git rev-parse HEAD` == the pinned full SHA, and the installed version
macros read 0.4.0. Non-display compiled BSP sources:
`chip/{esp32c5_i2c_bus,esp32c5_tasks}`, `board/ws_c5_35/{board,expander,touch,gps,battery}`;
the display path adds TFT_eSPI (patched for the C5 via `apply_c5_tft_patch.py`).

| Component | Resolved version (isolated core) |
|-----------|----------------------------------|
| platform espressif32 (pioarduino) | 55.3.312 |
| framework-arduinoespressif32 | 3.3.12 |
| framework-arduinoespressif32-libs | 5.5.5+sha.b774170ff46 |
| toolchain-riscv32-esp | 14.2.0+20260121 |
| tool-esptoolpy | 5.4.0 |
| XPowersLib (transitive) | 0.3.3 |

All rows are **compile-tested only** — resolution + compilation, not runtime.
Runtime behaviour is covered separately: hardware-verified in HARDWARE-FINDINGS; the
locking / short-read / recovery logic is host-tested in `test/host/`. (An earlier
`f4d6928` build in a *shared* warm core also passed; cold builds in a *shared* core
failed spuriously — the isolation note under *Known rough edges* explains why.)

> The final source/config candidate is validated by pinning a consumer to that
> commit; the result is appended in the docs-only evidence commit that follows it.

## Display consumer — **compile-tested (not hardware-verified)**

A display consumer additionally needs, from `examples/hello_board/`:
`apply_c5_tft_patch.py`, `tft_c5/` (the C5 processor port), `wsc5_user_setup.h`,
plus `bodmer/TFT_eSPI@^2.5.43` in `lib_deps` and `extra_scripts =
pre:apply_c5_tft_patch.py`. The BSP's `WsC5_35::begin()` brings the panel power and
ST7796 reset up (via the expander) before `tft.init()`. This path is now
**compile-tested** in the isolated core (see Recorded evidence); it is **not**
hardware-verified by this check — the panel/touch runtime is unproven here.

## Known rough edges (documented, not yet changed)

- The umbrella header `<ESP32C5_BSP.h>` exposes only version macros; drivers are
  included by path. Including only the umbrella gives no API.
- `test/` and the examples are delivered to consumers (no export filter). Harmless
  to the build (only `src/` compiles); export filtering is deferred pending a test
  of its effect on git vs packaged installs.
- **Build consumers in an isolated `PLATFORMIO_CORE_DIR` with a supported Python.**
  Sharing the core (`~/.platformio`) with another session or tool that is
  concurrently installing or mutating platform/framework packages can cause
  spurious resolution failures (e.g. `FRAMEWORK_LIBS_DIR` → `None`) that look like a
  project or library defect but are not — the same revisions build cleanly in a
  fresh, unshared core. Use Python 3.10–3.13 (the pioarduino platform refuses 3.14+).
  (A nested `platformio.ini` inside the cloned library is **not** a cause: the
  clean-room build of a revision that ships one succeeded.)
