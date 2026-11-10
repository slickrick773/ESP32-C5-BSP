# ESP32-C5 Hardware Findings

The reason this BSP exists. Every line below was paid for in bench time across a
fleet of five independent ESP32-C5 firmware ports. The point of writing it down
is so the next C5 author does not pay for it again.

## How to read a claim here

There are three different things people mean by "the C5 can do X", and conflating
them is how nights get burned:

1. **Silicon** — the die is physically capable of it (per the chip spec).
2. **IDF-exposed** — the ESP-IDF / Arduino-ESP32 core actually gives you an API
   that reaches it, and it compiles.
3. **Works-on-hardware** — it was run on a real board and observed to do the
   thing.

A clean compile is level 2. It is **not** level 3, and on the C5 the gap between
them is wide and full of silent failures (see the UART2 trap below). Nothing in
this BSP is claimed "supported" until it has been observed at level 3 on a
physical board. Each finding is tagged:

- **[HW-VERIFIED]** — observed on real hardware, with the observation noted.
- **[IDF]** — compiles and the API exists; not independently bench-confirmed here.
- **[SOURCE-VERIFIED]** — inspected version-pinned source/configuration; not a hardware result.
- **[USER-REPORTED]** — supplied hardware evidence, not independently reproduced here.
- **[UNVERIFIED]** — a working theory or an inference, flagged so no one builds on
  it as if it were fact.

Dates are when the observation was made.

---

## Chip-level (ESP32-C5, any board)

### The C5 is single-core

**[HW-VERIFIED]** The ESP32-C5 is a single RISC-V core. FreeRTOS runs in unicore
mode. Any task-pinning code copied from a dual-core S3/ESP32 must be written
chip-aware — pin to `portNUM_PROCESSORS > 1 ? 1 : 0` rather than a hard-coded
core 1, or `xTaskCreatePinnedToCore(..., 1, ...)` silently lands on the only core
there is. Not a bug on its own, but it hides the assumption that bites when code
moves back to a dual-core part.

**[HW-VERIFIED 2026-10-23, this BSP]** `src/chip/esp32c5_tasks` wraps this:
`workerCore()` returns 0 on the C5 (not 1), and `createPinnedTask()` pins to it.
The `single_core` example confirmed on hardware: `coreCount = 1`, a task made
with `createPinnedTask()` and the main loop both run on **core 0**
(`createPinnedTask -> pdPASS (pinned to core 0)`, `[loop] core 0 of 1`).

### Console is on USB-Serial-JTAG, not a UART

**[HW-VERIFIED]** Boot and panic logs come out the **native USB** (USB-Serial-JTAG
peripheral), read at **115200**. There is no separate USB-UART bridge chip in the
path on the boards tested. Consequence: the hardware UART0 pins are free for your
own use (e.g. a GPS), because the console is not sitting on them.

**Arduino `Serial` needs `ARDUINO_USB_CDC_ON_BOOT=1` to reach that USB port.**
[HW-VERIFIED 2026-10-22.] This is a sharp edge: the board manifest defaults it to
`0`, and with `0`, ESP-IDF `log_*` output and the panic backtrace still come out
USB-Serial-JTAG, but **Arduino `Serial.print()` goes to UART0 / GPIO11 instead** —
so a sketch's own logs are invisible on the port you flash and monitor on (they
are on a header pin). Symptom: the board runs fine, flashing works over USB, but
`pio device monitor` / a pyserial read on the usbmodem port shows nothing from
your code (a crash, though, would still print there). Fix: build the sketch with
`-DARDUINO_USB_CDC_ON_BOOT=1` so `Serial` is the USB CDC. The `hello_board`
example does this.

Reading logs when scripted: use **pyserial** on the usbmodem port at 115200.
`pio device monitor` needs an interactive TTY and fails when scripted.

Download mode: native-USB auto-download works hands-free **on firmware that
leaves USB alone** (flashed dozens of times with no button dance). It depends on
the running firmware, though: a build that hijacks **GPIO13/14** (a UART GPS, see
below) kills native USB, so esptool fails with `No serial data received` and you
must enter download mode by hand — **hold BOOT, tap RST, release BOOT**, then
flash.

**Never assign a UART to GPIO13/14 — they are the native USB D-/D+ lines.**
[HW-VERIFIED] Because the console rides native USB (above), GPIO13 and GPIO14
are the USB-Serial-JTAG D-/D+ pins. Opening a hardware UART on them — e.g. a
GPS wired to `GPS_RX 13` / `GPS_TX 14` — makes the board **drop off USB at
boot**: no console, and auto-download breaks so every reflash needs the
BOOT+RST dance. Observed in a downstream port that inherited a UART GPS on
14/13; undefining that GPS restored USB and auto-download. There is no reason
to put a UART there anyway — this board's GPS is on the shared I²C bus (see the
GPS section below), and the header UART is dead as shipped (below).

### The USB link stalls on some data patterns during large transfers

**[HW-VERIFIED 2026-10-26].** Over the native USB link the C5's ROM loader stalls
**deterministically at fixed addresses** on large transfers — the same block
fails every time (`Packet content transfer stopped` / `chip stopped
responding`), so it is data/address-dependent, not random noise or a bad cable
(the same image transfers fine once chunked). A single large erase (≈2 MB) also
drops USB, while 512 KB erases are fine. Work around it with small operations
plus a retry on the failing piece: **write** in 512 KB segments, **read** in
64 KB windows falling back to 4 KB with a shifted start offset. This board also
runs everything on the ROM loader — the esptool **stub crashes** on this rev (at
the baud change and on reads), so always `--no-stub`. The
[`tools/c5flash`](../tools/c5flash) helper in this repo implements all of this.

### Flashing must set the flash mode, or the C5 won't boot (and the screen stays dark)

**[HW-VERIFIED 2026-10-26].** Writing the bootloader (the segment at `0x2000` —
see the offset note below) with esptool `--flash-mode keep` leaves a **bad
flash-mode byte in the image header**;
the ROM rejects it — `invalid header: 0x….` → `ets_flash_boot` assert → watchdog
reboot loop. The app never runs, so **every build "boots" to a dark screen**
(backlight on) — trivially mistaken for a display/SPI/driver bug (it cost a long
chase). **The tell is in the ROM log (`invalid header`) — read the console before
you touch the display.** Fix: write with explicit `--flash-mode dio
--flash-freq 80m` (a merged factory image written at `0x0`, or per-segment with
the bootloader at `0x2000`). `tools/c5flash` now sets these by default (it used
`keep`, which was the bug). Enter download mode hands-free with `esptool --before
usb-reset` once the board boots a healthy app — no BOOT-button hold needed.

### Bootloader offset is `0x2000` on the C5, not `0x0`

**[HW-VERIFIED 2026-10-29].** The C5 ROM loads the second-stage bootloader from
`0x2000` — the 8 KB below it is reserved for the flash-encryption key manager —
unlike the S3/C3/C6, which load it from `0x0`. So hand-flash the bootloader at
`0x2000`, the partition table at `0x8000`, and the app at `0x10000`. Writing the
bootloader at `0x0` instead leaves `0x2000` holding bootloader *code*; the ROM
reads that non-`0xE9` word as the image header and reboot-loops on `invalid
header: 0x…` — the **same dark-screen symptom as the flash-mode bug above, a
different cause**. Seen on a rev-v1.2 board (ROM `esp32c5-eco3-20250704`): the
rejected header value was exactly the bootloader's own bytes at its internal
offset `0x2000`; re-flashing the bootloader at `0x2000` is the fix. `pio run -t
upload` already gets this right — the PlatformIO `espidf.py` builder derives
`upload.bootloader_offset` = `0x2000` for `esp32c5` / `esp32p4` — so only
hand-flashing via `c5flash` / `esptool` needs the explicit offset. (Source:
ESP-IDF C5 bootloader guide + the on-disk pio builder.)

### Legacy BLE advertising under internal-RAM pressure (3.3.10)

**[HW-VERIFIED 2026-11-04, failure reproduced over USB serial]** On the
Waveshare 3.5-inch board running a memory-constrained test application, the
Arduino NimBLE advertising path repeatedly reports `Error enabling advertising;
rc=519` after initialization. This verifies the failure, not a universal
minimum-RAM requirement or successful over-air interoperability.

**[USER-REPORTED hardware measurements, 2026-11-04]** The supplied firmware
measurements are approximately 20 KB internal free during WiFi scanning, 55 KB
after `WiFi.mode(WIFI_OFF)`, and 12.5 KB after BLE init + GATT. Initialization
and host sync succeed; even flags + one 128-bit UUID without scan response fail
at advertising start. These heap figures have not been independently reproduced
in that application. WiFi is off during the failing attempt; coexistence tuning
is not the proposed remedy.

`519 = 0x200 + 0x07`: NimBLE's HCI error namespace plus Memory Capacity Exceeded.
The failure is at advertising enable, not the earlier payload-setting step.
This is consistent with controller resource exhaustion, but the return code
alone does **not** prove a failed general-heap allocation, its size, or
fragmentation. Controller-private pools can also exhaust.

#### Measure the eligible heap, not the PSRAM-inclusive heap

**[SOURCE-VERIFIED, installed Arduino 3.3.10 and IDF v5.5.4]**
`ESP.getFreeHeap()` uses `MALLOC_CAP_INTERNAL` (not an explicit `8BIT` mask).
`ESP.getMaxAllocHeap()` uses that same mask for the largest block. A largest
block measured with only `MALLOC_CAP_8BIT` includes PSRAM and cannot diagnose
controller allocation failures. The C5 controller allocation helpers use
`INTERNAL | 8BIT | DMA`; record this more restrictive mask as well:

```cpp
#include "esp_heap_caps.h"
static void logBleHeap(const char *phase) {
    const uint32_t caps[] = {
        MALLOC_CAP_INTERNAL,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT | MALLOC_CAP_DMA
    };
    for (unsigned i = 0; i < 3; ++i) {
        Serial.printf("%s caps=0x%lx free=%u largest=%u min=%u\n", phase,
            (unsigned long)caps[i],
            (unsigned)heap_caps_get_free_size(caps[i]),
            (unsigned)heap_caps_get_largest_free_block(caps[i]),
            (unsigned)heap_caps_get_minimum_free_size(caps[i]));
    }
}
```

The isolated bench results below demonstrate both capability exhaustion and
fragmentation. To establish which applies in the full application, capture before WiFi shutdown, after shutdown and scan-result cleanup, before
BLE init, after host sync/GATT registration, immediately before advertising,
and immediately after the return. Record allocation failures with
`heap_caps_register_failed_alloc_callback`; the callback must not allocate or
block. Log the saved size/capabilities afterward. Distinguish deliberate test
pressure failures from new failures during advertising.

The [heap probe](../examples/ble_heap_probe) provides these masks, controlled
pressure, fragmentation and raw GAP return codes. A threshold is conditional on
the exact binary, allocation history and advertising mode. Establish it with
fresh-boot first-start trials, and report lowest passing and highest failing
**(free, largest)** pairs. Compare similar totals with different largest blocks:
success after coalescing/freeing a large block implicates fragmentation; a failed
request larger than all eligible free RAM implicates total capacity. Both can
limit different runs. Do not convert a net heap delta into the peak allocation
requirement or treat repeated starts using retained allocations as cold trials.

#### Bench results: free INTERNAL is not the controller's free heap

**[HW-VERIFIED 2026-11-04, isolated probe on the user-confirmed board]** Stock
3.3.10 / IDF v5.5.4, requested FQBN/options, 8 MB PSRAM reported, WiFi never
started. Each row below begins with a fresh boot, BLE init, minimal GATT server,
and successful advertising/empty scan-response data configuration. Pressure is
applied **after** configuration and **before the first legacy connectable
advertising enable**. All numbers are bytes immediately before enable.

| Trial | INTERNAL free | INTERNAL largest | INTERNAL\|8BIT\|DMA free | DMA-eligible largest | Result |
|---|---:|---:|---:|---:|---|
| Baseline | 172,856 | 155,636 | 156,940 | 155,636 | `rc=0`, active, no allocation failures |
| ~12.5 KB | 12,648 | 12,276 | 268 | 188 | **`rc=519`, inactive**, failed internal/DMA allocation |
| ~20 KB | 20,264 | 15,348 | 4,348 | 4,084 | `rc=0`, active, no allocation failures |
| ~18 KB | 18,088 | 15,348 | 2,172 | 2,036 | `rc=0`, active, no allocation failures |
| Lowest clean sampled point | 17,544 | 15,348 | 1,628 | 1,524 | `rc=0`, active, no allocation failures |
| Lower point | 17,272 | 15,348 | 1,356 | 1,268 | `rc=0`, active, **repeated 292-byte allocation failures** |
| Still lower | 16,728 | 15,348 | 812 | 724 | `rc=0`, active, **repeated 20-byte allocation failures** |
| Fragmented: alternate pressure blocks freed | 92,888 | 12,276 | 78,604 | 244 | `rc=0`, active, **repeated 292-byte allocation failures** |

For all rows, INTERNAL and INTERNAL|8BIT measurements matched; DMA eligibility
removed a substantial portion of usable memory. At normal/near-boundary points,
**15,916 bytes of free INTERNAL RAM were not DMA eligible**. At the ~12.5 KB
point, pressure also consumed some of that non-DMA memory. The largest INTERNAL
block alone therefore hides the actual exhausted resource.

The fragmentation trial proves a separate limit: 78,604 eligible free bytes
cannot satisfy a 292-byte contiguous allocation when the largest eligible block
is 244 bytes. Releasing the remaining pressure blocks let the active controller
finish allocating; the failure counter stopped advancing in the recorded
recovery interval. Retrying `start` while already active returned `2`; this is
not a second cold-start trial.

**Measured bounds, not a universal minimum:** in this allocation pattern, the
lowest clean sampled first-start point was 1,628 eligible free bytes with a
1,524-byte largest block; 1,356/1,268 already produced continuing failures.
Three additional fresh-boot repeats of the 1,628/1,524 point had zero failures
through approximately four seconds after start; two additional 1,356/1,268
repeats each recorded 28 failures by that observation point.
Thus the clean-start boundary is bracketed within those sampled pairs, not
specified to the byte. A largest block of 244 is demonstrably insufficient for
the observed 292-byte request. The exact minimum largest block independently
of total free was not established. The clean runs retained 1,424 bytes after
start; that is a net delta, not a measured peak requirement.

**A zero GAP return code and `active=1` are insufficient acceptance criteria.**
The pressure tests exposed continued controller allocation failures after that
success indication. No second BLE receiver or application peer was used here:
these are host/controller start and allocator measurements, **not confirmed
over-air discovery or a working application-level transfer**. The full
application's exact heap layout/GATT workload still needs the same instrumentation.

The probe also logged `Failed to allocate dummy cacheline for PSRAM memory
barrier!` at boot while reporting 8 MB PSRAM. Its cause was not investigated;
these runs do not validate a custom PSRAM-host build.

Evidence: [raw enable trials](hardware-evidence/ble-2026-11-04/enable-trials.log),
[boundary repeats](hardware-evidence/ble-2026-11-04/repeat-trials.log),
[build/source hashes](hardware-evidence/ble-2026-11-04/build-metadata.json), and
[probe source](../examples/ble_heap_probe/ble_heap_probe.ino).

#### PSRAM and the stock core

**[SOURCE-VERIFIED; PSRAM-host build not hardware-tested here]**
`CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL=y` routes allocations through the
host's configurable allocator to `SPIRAM | 8BIT`. It is selectable when PSRAM
capability allocation or PSRAM malloc integration is enabled. Controller
internal/DMA allocations remain internal, and ordinary task-stack allocation
is not automatically migrated by this option.

In this stock build, `CONFIG_BT_LE_MSYS_INIT_IN_CONTROLLER=y` delegates shared
MSYS initialization to `r_esp_ble_msys_init`. Therefore **do not promise that
EXTERNAL moves the shared MSYS pools**, or that it recovers the entire reported
42 KB. Measure the host savings separately. The configured MSYS payload storage
alone is 24 × 128 + 24 × 320 = 10,752 bytes, before bookkeeping; it is not a
measurement of movable memory.

Arduino's BLE C++ wrapper is compiled from source, but its IDF NimBLE host and
controller support are supplied in prebuilt SDK libraries. A sketch `-D` flag or
local `sdkconfig.defaults` does not rebuild those libraries. Use a consistent
custom Arduino SDK/library build, or Arduino as an ESP-IDF component with a
matching supported IDF version, and rebuild affected components. Do not edit
only `sdkconfig.h`: that can make headers disagree with the linked library.
PSRAM enabled and `huge_app` alone do not relocate existing internal allocations;
`huge_app` changes flash partition capacity.

#### Configuration levers for a custom build

**[SOURCE-VERIFIED availability; memory benefit/bridge stability UNVERIFIED]**
These are configuration candidates, not measured advertising fixes:

| Lever | Stock 3.3.10 / relevance |
|---|---|
| `BT_NIMBLE_MAX_CONNECTIONS` | 3; try 1 for one bridge peer. C5 controller defaults derive from the NimBLE setting. `BT_LE_MAX_CONNECTIONS` is gated off with NimBLE, so it is the wrong knob for this build. |
| `BT_LE_CONN_RESERVED_MEMORY_COUNT` | 2, capped by maximum connections. With max connections 1, reserve 1; setting 0 defers memory demand until connect and can merely move the failure. |
| `BT_LE_EXT_ADV_RESERVED_MEMORY_COUNT` | 2, capped by configured advertising instances (default mapping here is 1). Kconfig documents preallocation versus later allocation failure for extended instances. Lowering it is not an established fix for legacy enable; legacy behavior needs measurement. |
| `BT_NIMBLE_MSYS_1_BLOCK_COUNT/SIZE`, `BT_NIMBLE_MSYS_2_BLOCK_COUNT/SIZE` | 24/128 and 24/320. Counts can reduce shared pool footprint in a rebuilt SDK. Start with conservative count reductions and test traffic; retain adequate block sizes for the actual ATT/ACL use. Corresponding `BT_LE_MSYS_*` controls are gated off with NimBLE. |
| `BT_NIMBLE_TRANSPORT_ACL_FROM_LL_COUNT`, `BT_NIMBLE_TRANSPORT_EVT_COUNT`, `BT_NIMBLE_TRANSPORT_EVT_DISCARD_COUNT` | 24, 30, 8. Reducing counts can reduce baseline resource demand; excessive reduction causes packet/event starvation. No measured reduction of the single enable allocation is established. |
| `BT_NIMBLE_ROLE_CENTRAL`, `BT_NIMBLE_ROLE_OBSERVER`, `BT_NIMBLE_GATT_CLIENT` | Enabled; disable unused roles/client in a peripheral-only bridge. Keep peripheral, broadcaster and GATT server. |
| `BT_NIMBLE_ATT_PREFERRED_MTU`, `BT_NIMBLE_ATT_MAX_PREP_ENTRIES` | 256, 64. Tune to protocol requirements; smaller MTU needs application chunking and is not itself a controller advertising-memory fix. |
| Host/controller task stacks | Both 5120 bytes. Reduce only after stack high-water measurements under connections, security and transfers. |
| `BT_CTRL_BLE_ADV_REPORT_*`, duplicate-scan caches | Concern scan reports/observer operation, not this peripheral's advertising payload. Not a demonstrated enable fix. |
| `BT_CTRL_BLE_MAX_ACT` / `BTDM_*` | Other-controller-family advice; not the C5 NimBLE control to paste into this build. |

All listed SDK Kconfig changes require rebuilding the affected SDK components in
this stock-library workflow. Some controller configuration fields exist at the
low-level init API, but the normal `BLEDevice::init()` path owns initialization;
independently changing fields without keeping host/controller limits consistent
is not a validated shortcut.

A conservative custom-build starting point (not a complete sdkconfig):

```ini
CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL=y
# CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_INTERNAL is not set
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1
CONFIG_BT_LE_CONN_RESERVED_MEMORY_COUNT=1
# CONFIG_BT_NIMBLE_ROLE_CENTRAL is not set
# CONFIG_BT_NIMBLE_ROLE_OBSERVER is not set
# CONFIG_BT_NIMBLE_GATT_CLIENT is not set
CONFIG_BT_NIMBLE_ROLE_PERIPHERAL=y
CONFIG_BT_NIMBLE_ROLE_BROADCASTER=y
CONFIG_BT_NIMBLE_GATT_SERVER=y
```

Enable board-appropriate PSRAM and confirm it initializes before NimBLE; resolve
Kconfig dependencies using the actual SDK, then inspect generated sdkconfig and
map files. Keep security required by the bridge. Leave buffer sizes and stacks
unchanged for the first PSRAM-host comparison so the effect can be isolated.

#### Recommended application sequence

1. With the stock core, stop scanning and scan callbacks, release scan results
   (`WiFi.scanDelete()` for Arduino scan results), turn WiFi off and verify the
   returned status. Free application-owned capture/scan queues; WiFi shutdown
   does not free those allocations for the application.
2. Move eligible UI/history/capture buffers explicitly to PSRAM, checking every
   allocation. Keep ISR/cache-disabled/DMA-constrained memory in its required
   region. Avoid assuming default `malloc` chooses PSRAM for small objects.
3. Instrument the DMA-eligible heap and recover at least the tested clean
   comparison point (4,348 bytes free / 4,084 largest at ~20 KB INTERNAL) before
   comparing the full application. This is a bring-up target, not a proven
   connection/transfer budget. Initialize BLE, register only the required GATT
   service, and **start
   advertising before rebuilding optional UI/worker allocations**. Budget for
   connection, subscription, security and notification bursts too. Starting
   early changes allocation order; it does not create memory.
4. If stock-core memory remains insufficient, compare the custom PSRAM-host,
   single-peer build above. Then reduce selected pool counts one change at a
   time, recording the same heap snapshots and bridge workload results.
5. Require remote discovery, an application-level transfer and reconnect before
   claiming interoperability. A successful GAP start alone is only
   controller/host enable success.

Source anchors (version-pinned):
[Arduino heap helpers](https://github.com/espressif/arduino-esp32/blob/3.3.10/cores/esp32/Esp.cpp),
[Arduino advertising](https://github.com/espressif/arduino-esp32/blob/3.3.10/libraries/BLE/src/BLEAdvertising.cpp),
[C5 controller allocation](https://github.com/espressif/esp-idf/blob/v5.5.4/components/bt/controller/esp32c5/bt.c),
[BT allocators](https://github.com/espressif/esp-idf/blob/v5.5.4/components/bt/porting/mem/bt_osi_mem.c),
[shared MSYS initialization](https://github.com/espressif/esp-idf/blob/v5.5.4/components/bt/porting/mem/os_msys_init.c),
[NimBLE Kconfig](https://github.com/espressif/esp-idf/blob/v5.5.4/components/bt/host/nimble/Kconfig.in),
[C5 Kconfig](https://github.com/espressif/esp-idf/blob/v5.5.4/components/bt/controller/esp32c5/Kconfig.in).

### Dual-band radio: `set_band_mode()` must come AFTER `esp_wifi_start()`

**[HW-VERIFIED]** The C5 is 2.4 + 5 GHz. The order of operations is the trap:
`esp_wifi_set_band_mode()` has to be called **after** `esp_wifi_start()`, not
between init and start. Called too early it does not take, and you are quietly
left on a single band wondering why the 5 GHz channel list comes back empty.

Working sequence (receive/scan path, verified):

```
esp_wifi_init(...);
esp_wifi_set_mode(WIFI_MODE_STA);
esp_wifi_start();                                 // <-- start first
esp_wifi_set_promiscuous(true);
esp_wifi_set_band_mode(WIFI_BAND_MODE_AUTO);      // <-- THEN band mode
```

### 5 GHz channel enumeration is driver-gated, not fixed

**[HW-VERIFIED]** You do not get to pick 5 GHz channels off a static list. After
the band is up, the radio applies the active country's channel list and that is
the real set you can tune. The robust pattern is to walk the band's channel bits
(`BIT_NUMBER_TO_CHANNEL(b, WIFI_BAND_5G)`) and keep only the channels
`esp_wifi_set_channel()` actually accepts — the accepted set is the answer, not
your assumption. On the units tested, a meaningful subset of the 28 bits was
accepted; the exact count is region- and build-dependent, so enumerate, don't
hard-code.

5 GHz scanning/receiving itself is **[HW-VERIFIED]** working on these boards. This
BSP does not add radio features on top — that space is crowded and is explicitly
not what this package competes on.

---

## Board-level (Waveshare ESP32-C5-Touch-LCD-3.5)

### The J3 header UART is NOT bonded to the chip as shipped

**[HW-VERIFIED] 2026-10-21, two separate units.** This is the single most
expensive finding in the fleet. On the 3.5 as shipped:

- The GPS module's TX measured a clean **3.247 V** at the wire tip, so the module
  and the wire are good.
- A firmware pin-probe (digitalRead with an internal pull-down, ~1.5 s per pin)
  saw **zero edges on every reachable header GPIO — 12, 11, 0, 1.** Pins 26/27
  read high only from their I²C pull-ups (zero edges). The data is simply not
  arriving at any header GPIO.

**Conclusion: on this board you cannot get GPS (or any UART peripheral) in over
the J3 header by wiring alone.** Do not spend another night reseating wires — it
is a board routing limitation, not a wiring or firmware fault.

Which pins *do* reach the chip on the header: **SDA (27) and SCL (26)** are
connected (the shared I²C bus), and **3V3 / GND** are real. That is the opening
the GPS solution uses.

> **R-selector mechanism — [SCHEMATIC-CONFIRMED] 2026-10-23.** The official
> Waveshare schematic (see Sources) confirms what the fleet had only inferred
> from behaviour: several module GPIOs sit on a 3-pad 0-ohm selector — pad 1 is
> the chip pin, pad 2 is the on-board function, pad 3 is the J3 header — marked
> `0R NC` (the `0R` is fitted to pad 2, pad 3 is left `NC`):
>
> | Selector | Module GPIO | Pad 2 (0R, fitted) | Pad 3 (NC → header) |
> |---|---|---|---|
> | R55 | GPIO0 | CAM_D0 | GPIO0_OUT → J3-15 |
> | R58 | GPIO1 | CAM_D1 | GPIO1_OUT → J3-17 |
> | R56 | GPIO11 | CAM_PCLK | U0TXD → J3-26 |
> | R59 | GPIO12 | CAM_VSYNC | U0RXD → J3-28 |
> | R57 | GPIO9 | SD card CS | GPIO9_OUT → J3-19 |
>
> The designators match the fleet's guess exactly (R56/R59 for GPIO11/12, R55/R58
> for GPIO0/1), and R57 adds GPIO9: **header pin J3-19 is also NC as shipped**,
> not a live SD-card pin. With the header side (pad 3) left open on all of them,
> GPIO0/1/9/11/12 route to their on-board function even on no-camera units —
> which is precisely why the firmware pin-probe saw **0 edges** on the header's
> GPIO12 on two boards.
>
> **Which pad is connected is proved on-board, not just assumed:** R57's pad 2 is
> the SD-card CS, and the SD card *works* — so pad 2 (the fitted `0R`) is the
> connected side and pad 3 (`NC`) is the open header side, on every selector.
> That pins the read direction empirically. Schematic confirms the *mechanism*,
> the working SD card confirms the *pad direction*, and the two-board pin-probe
> confirms the *as-shipped* open state. A rework (move a selector's `0R` from pad
> 2 to pad 3) would restore that header pin but cut its on-board function (for
> R57, the SD card); the shipping, no-rework solution is I²C (below), no solder.
>
> The same schematic also confirms the pin map: GPIO26 = SCL, GPIO27 = SDA
> (shared I²C), the LCD SPI pins (MISO 2, DC 5, SCLK 6, MOSI 7, CS 8), and the
> J3 header breakout. One nuance to reconcile before relying on the labels: the
> schematic's EXIO assignments read **EXIO0 = LCD_RST, EXIO1 = TP_RST** (this
> driver's comments have them swapped) and **EXIO5 = PA_CTRL** (audio amp, not
> panel power), with the backlight on a dedicated EXIO PWM. The expander driver
> is hardware-verified working as written, so the bit assignments are left
> unchanged; treat those two comment labels as functional, not authoritative,
> pending a hardware re-test.

### GPS: LC76G over I²C on the shared bus (the crown jewel)

**[HW-VERIFIED] 2026-10-21 — 14 satellites with touch/audio/battery all normal.**
Because the header UART is dead (above), GPS rides the **shared I²C bus**
(SDA 27 / SCL 26) using Waveshare's official `LC76G_RPI_I2C` protocol. Hard-won
details that make it coexist with touch:

- **Multi-frame atomic transaction.** Per Quectel, no other slave may be
  addressed mid-transaction. Running the whole read cycle synchronously on the
  single-threaded main loop guarantees that for free, so the existing touch /
  audio / battery drivers stay untouched.
- **Short inter-step settle (~3 ms, not the demo's 10 ms).** The module
  clock-stretches, so a short settle keeps NMEA clean while keeping the
  main-loop hitch small. A ~50 ms/cycle hitch (demo timing) jittered tap/gesture
  timing badly enough that **taps registered as swipes** — GPS was fine, touch
  was the casualty.
- **Gate the read on touch-idle.** Skip the GPS cycle while the screen is being
  touched (gate on the UI's `lastTouch`, e.g. ≥ 400 ms idle). This is what keeps
  touch alive.
- **Save and restore the Wire timeout around the read.** The GPS read needs a
  generous (~300 ms) bus timeout for the clock-stretch; if you leak that timeout
  it lingers on subsequent touch reads. Restore the previous timeout at the end
  of every cycle.
- **Baud note (for the UART path, if the selector rework is ever done):** the
  LC76G defaults to **115200, not 9600** as Waveshare's demos state —
  [HW-VERIFIED] on the wire (clean NMEA, a real fix, 10 sats, HDOP 1.06).

**[HW-VERIFIED 2026-10-22, this BSP]** The `gps_i2c` example got a strong fix on
hardware (29 satellites, HDOP ~0.6) over the shared bus, with the whole proven
discipline in play (reopen-once, 300 ms timeout guard, bus lock held for the
multi-frame cycle). Two bring-up notes worth having:

- **Confirm the module with a bus scan before suspecting firmware.** A one-shot
  I²C scan at boot settles it in seconds. On this board the onboard chips answer
  at `0x18` (codec), `0x24` (expander), `0x34` (AXP2101), `0x38` (touch), `0x51`
  (RTC), `0x6B` (IMU) and `0x70`. **The LC76G shows up at `0x50`** when wired
  right. If `0x50` is absent but the onboard set is present, the bus is healthy
  and the module is the problem — SDA/SCL swapped, wrong module pins, not in I²C
  mode, or unpowered. (Note `0x51` is the RTC, not the GPS — don't be fooled.)
- **SDA/SCL swapped is the classic miss.** First bring-up here had the external
  LC76G's SDA/SCL reversed: every onboard device answered, `0x50` did not, and
  the GPS cycle failed every time. Un-swapping them put `0x50` on the bus and
  NMEA flowed immediately.

### LC76G over UART, if you ever rework the selector — [UNVERIFIED on this board]

Documented only so the fleet facts are in one place; **not reachable on the 3.5
as shipped** (header UART finding above). On C5 ports where the UART *is* wired:

- Use **`Serial1`** (or UART0) with explicit pins, **not `Serial2`**. The C5's
  "UART2" is the **LP-UART** (HP_NUM=2, LP_NUM=1) with **fixed pins RX 4 / TX 5**.
  `Serial2` compiles but the Arduino HAL rejects other pins ("LP UART requires RX
  pin to be set to 4"), so a hard-coded UART2 GPS silently gets no data. This is
  the level-2-vs-level-3 gap in one sentence.
- At 115200, give the UART a **big RX buffer** (`setRxBufferSize(2048)`) and drain
  generously, or the ~11.5 KB/s stream overruns the default 256-byte buffer and
  nothing parses. Symptom: **rx byte counter climbs while sat count stays 0** —
  data is arriving; it is a buffer/parse problem, not wiring.

### Shared I²C bus discipline

**[HW-VERIFIED].** The FT6336 touch, the ES8311 codec, the CH32V006 expander, and
the AXP2101 PMU **all share SDA 27 / SCL 26** (plus GPS, above). Three patterns
keep the bus from wedging:

- **Per-read bus re-open (touch).** The C5's early `Wire.begin()` leaves the I²C
  controller unable to complete reads (writes ACK, reads return 0). The fix is to
  `Wire.end()` + `Wire.begin()` on each touch read and validate the result
  (1–2 points, on-panel coords). This was the hardest single bug in the C5
  bring-up. Reads are also intermittently flaky even with the re-open, so retry a
  couple of times within one poll before reporting "no touch".
- **STOP-based reads (ES8311).** A repeated-start read (`endTransmission(false)`)
  errors on the C5 and leaves the bus mid-transaction, which breaks touch. The
  codec must read with a **STOP** (`endTransmission(true)` then `requestFrom`).
- **Re-open once per transaction, never between steps (GPS).** Same re-open
  quirk; do it once before the transaction and never mid-cycle.

The BSP puts all three behind one chip-level core (`src/chip/esp32c5_i2c_bus`):
`reopen()` (the one re-open), a `TimeoutGuard` (save/restore Wire's timeout), a
non-blocking `lock()` so an owner can skip a frame instead of stalling behind a
long transaction, and a 9-clock `recover()` for a wedged bus.

### The touch-break story (why FT6336 feels the way it does)

**[HW-VERIFIED 2026-10-22.]** Getting touch from "works but stiff" to smooth was
three separate lessons, all on this shared bus:

1. **Re-open ONCE per read, not per retry.** The first cut re-opened the bus
   (`Wire.end()`/`Wire.begin()`) before *each* of three read attempts. A re-open
   measures ~2.2 ms on this board, so three per read put idle polling at
   ~150 Hz and added latency you could feel. Re-opening once at the top of the
   read and then retrying the register read *without* re-opening again put idle
   polling at ~**444 Hz (~2.2 ms/poll)** — the re-open is the whole cost, so
   paying it once is the fix. The first read still happens right after a
   re-open, which is what makes it complete.
2. **Bridge dropped reads (dropout grace).** The FT6336 intermittently returns
   "no points" for a single read *even with a finger held down*. Reported raw,
   that reads as a finger-lift, so a drag keeps letting go and a still-hold grab
   loses its timer. Holding the last contact for a short grace window (~90 ms)
   across a missed read bridges the glitch; a real release outlasts the window
   and still registers as up. This is what `Touch::poll()` adds over
   `Touch::read()`.
3. **Skip, don't stall, when the bus is busy.** Touch takes the bus lock
   non-blocking: if another owner (a GPS cycle) holds it, touch returns a
   dropped read for that frame rather than blocking, and the grace window above
   hides the gap. A blocking touch read behind a ~50 ms GPS cycle is exactly
   what made taps register as swipes on the first GPS bring-up.
4. **Pace the loop; don't spin the draw flat out.** [HW-VERIFIED 2026-10-22.]
   With the read paced but the main loop otherwise free-running, redrawing every
   iteration (thousands of `drawLine`/`fillCircle` a second) saturated the
   single C5 core and the display SPI, which starved the touch reads so points
   landed far apart and a drag came out as jagged connect-the-dots. The read
   data was fine the whole time (a paced diagnostic traced a clean, dense drag);
   the draw was eating the core. Pacing the loop to ~10 ms (~100 Hz) and acting
   once per tick made the stroke smooth. Lesson: on a single-core C5, a tight UI
   loop has to pace itself — `poll()` pacing the *bus* is necessary but not
   sufficient if the *draw* is unpaced. The `hello_board` loop shows the shape.
5. **Under load, give touch priority.** [HW-VERIFIED 2026-10-23, capstone.]
   With Wi-Fi scanning, GPS cycling and the battery being sampled all at once,
   touch went slightly janky — not from the bus (GPS defers while touched) but
   from the display work: the periodic status redraw (`fillRect` + text) and the
   battery read's bus re-open landed mid-stroke. Fix on the single core: while a
   finger is down, do *only* touch — skip the status redraw, the battery sample
   and the GPS cycle; they all resume the instant the finger lifts, and a
   scribble is brief. Plus connect contacts into a stroke (isolated dots read as
   a dotted line). With both, the stroke is smooth with all four subsystems
   live. Wi-Fi scans async so the radio never blocks the loop, and the dual-band
   `set_band_mode()` is called after the driver starts.
6. **Gate reads on the FT6336 INT line to kill phantom touches.**
   [HW-VERIFIED 2026-10-23, `touch_int`.] A blind poll of the FT6336 with no
   finger down can return a stale/phantom point — on a UI that showed up as a
   menu row self-selecting at boot and taps landing a row or two off. The fix
   is to only read when the INT line says a finger is down. On this board
   **FT6336 INT = GPIO3** (TP_INT, schematic-confirmed), with an external 10K
   pull-up (so `pinMode INPUT`, no internal pull). Set FT6336 reg **`0xA4 = 0`**
   (interrupt-polling mode) so INT is a stable **level** (low = finger), not a
   per-touch pulse — required, or the level read is flaky. Then skip the I²C
   read whenever `digitalRead(INT) != LOW`. The win is *skipping the read*, not
   interrupt latency; a level read each poll is enough. Verified on hardware:
   untouched reports zero reads (no phantom), a finger still tracks at the full
   poll rate. This is opt-in in the BSP (`Touch::beginInt(gpio)`); the polling
   `read()` stays the default, since not every C5 board wires TP_INT. Calibration
   is the consumer's concern, not the BSP's. (First hit and fixed on the
   SquachWatch port; mirrored here.)

### NeoPixel is on GPIO27 — which is the I²C SDA line

**[HW-VERIFIED 2026-10-26].** Firmware that inherits a Pancake-style config drives
a NeoPixel status LED on **GPIO27**. On this board GPIO27 is the shared **I²C SDA**
line (touch, expander, PMU and GPS all sit on it), so every LED update writes the
pin and corrupts whatever I²C transaction is in flight. There is no usable
NeoPixel here — leave the feature off on this board. This bus corruption is also
the cause of the **black panel after a USB reset** (see that finding below);
disabling the NeoPixel fixes both.

> **Related [UNVERIFIED]:** the touch driver's per-read bus re-open (the "stiff"
> triple `Wire.end()/begin()`, below) may have been compensating for this
> NeoPixel bus corruption. With the LED disabled it may be removable — to be
> confirmed on hardware before dropping it, since it also lives in the touch
> bring-up path.

### Backlight dimming is through the expander, not a `TFT_BL` pin

**[HW-VERIFIED 2026-10-26].** There is no backlight GPIO — `TFT_BL` is `-1`. The
CH32V006 expander (`0x24`) drives the backlight PWM, so a brightness level is a
write to the expander's PWM channel, not `ledcWrite()` on a pin. A config that
leaves `TFT_BL = -1` **without** routing through the expander gets no dimming at
all — only the on-screen level bar moves while the panel stays full brightness.
Live dimming confirmed via the expander PWM path.

### Hide panel memory until the first frame

**[OBSERVED FAILURE 2026-11-05; cause narrowed 2026-11-05 — see the measured blocks below].**
Full dark-boot acceptance is **FAILED**. The user still observes uninitialized
panel illumination on cold boot and the previous frame during reset. In a
three-delay diagnostic, illumination persisted before expander initialization,
then the display stayed dark until the completed UI was revealed. Video timing
has not been correlated with execution timestamps; it does not locate the flash
within ROM, second-stage bootloader, or application execution.

`begin(0)` requests zero backlight PWM before panel initialization and keeps it
zero until the consumer reveals its first frame. Moving this earlier in setup,
writing PWM before mode/power changes, and calling it from Arduino `initVariant()`
have **not** eliminated the cold-boot flash. These observations do not establish
an unavoidable hardware limitation or the expander's power-on defaults.

A bounded early bootloader I2C write was tested on hardware and **did not help.**
**[OBSERVED FAILURE 2026-11-05.]** With the experimental IDF 5.5.2 after-init
bootloader installed (PWM=0 written to `0x24` reg `0x05` in `bootloader_after_init`;
`[dark-start] result=0 attempts=1` — ACKed; flash verification passed) and
`hello_board` holding `begin(0)` dark until its first frame, the user still
observes **both** the fuzzy cold-boot flash and the retained previous frame on
reset. The application boots and normal display and touch are confirmed.

**Conclusion.** Full dark-boot acceptance remains **FAILED**. The behaviour is now
characterised (see the measured blocks below): the backlight is on at power-up, and
the expander's PWM output controls it — a write of `0x00` to expander register
`0x05` is observed to darken the panel, a physical RST restores the glow and
re-defaults the PWM response, and esp_restart (SW reset) does not. The glow appears
in ROM download mode with neither the second-stage bootloader nor the application
running, so it does not require any user-flashed ESP firmware. The tested ESP-side
suppressions — `begin(0)`, an `initVariant()` hook, and the after-init bootloader
PWM=0 write — did not eliminate the observed flash. Whether an earlier ROM-level
action could pre-empt it is **unresolved**: the first-illumination timing against
ROM startup and power application was not measured. Candidate approaches to remove
it entirely — a change to the CH32's power-up state (its firmware, not published by
the vendor) or a hardware gate on the backlight — are plausible, not established
requirements. **Not established** either: whether the expander actively drives the
backlight line high at power-up or the line defaults high undriven (the raw register
readback does not settle this), and the flash's exact optical duration (no scope or
photodiode).

*Source facts (kept distinct from the conclusion).* Expander PWM drives backlight
brightness at runtime (live dimming, HW-VERIFIED above). The BSP and the vendor
`07_lvgl_demo/Board_IO.cpp` both write 8-bit expander registers and write PWM=0
before enabling power; vendor `01_exio/io_extension.cpp` instead sends 16-bit
mode/output values — which form the installed CH32 firmware actually expects is
**not established**, and an initialization/protocol mismatch is not ruled out. An
absent vendor getter does not prove the device cannot read back mode/output/PWM.
The schematic labels CH32 PC3 as EXIO_PWM (LCD_BL via Q2/R40), PA7/RESET as
connected to CHIP_PU, EXIO5 as PA_CTRL and EXIO6 as SYS_OUT, while vendor code
names EXIO5 BOARD_POWER_EXIO; the board's actual EXIO5/EXIO6 functions and whether
the CH32 is reset with the ESP are **unresolved.** Do not change expander
power/reset outputs until these are established.

*Hypotheses.* The three candidates were: **A** illumination only *before* the early
write; **B** illumination *continues after* the acknowledged write; **C**
illumination *stops, then returns* during later init. The download-mode isolation
result — the backlight glows with neither the second-stage bootloader nor the
application running — points to **A**: the glow is present without any user-flashed
ESP write, and a software PWM=0 write ends it. The tested writes and hooks
(`begin(0)`, `initVariant()`, after-init bootloader PWM=0) did not eliminate the
flash; the first-illumination timing against ROM startup was not measured, so
whether `bootloader_before_init()` or a ROM-level hook could pre-empt it is
**unresolved** — untested, not demonstrated dead-ends. Cold boot and
reset behave differently: the retained previous frame is seen on reset; on one true
cold boot the final image was the live application, not a retained frame, but
whether stale GRAM briefly shows during the cold transient was not characterised.
What remains unmeasured is the flash's optical duration (no scope/photodiode).
Preserve rollback, flash layout, watchdogs and ROM
recovery when evaluating any hardware-level candidate. See the
[experimental probe and recovery plan](../tools/startup_probe/README.md).

**[MEASURED 2026-11-05 — expander read-probe v1, reads only, no register writes.]**
A read-only app (`examples/exio_probe/`, flashed at `0x10000` over the installed
*experimental* bootloader) read `0x24` at application start via pointer write +
repeated START + 2-byte `requestFrom`. **Raw bytes returned** (each register
ACKed; `requestFrom` returned 2 bytes): `mode 0x02 → 23 FF`, `output 0x03 → 23 00`,
`input 0x04 → 37 00`, `pwm 0x05 → 00 FF`, `adc 0x06 → 01xx–02xx (differs between
reads)`. The ADC responses differed; this does not validate the semantics of other registers.

These are **raw readings only.** This capture does **not** establish: that two
returned bytes identify a 16-bit firmware (a 2-byte `requestFrom` returns 2 bytes
regardless of register width); that the `pwm` low byte `00` was *caused* by the
bootloader's PWM-off write (it may be a prior or default state); or that
`mode`/`output` = `0x23` — which equals the previous `hello_board` run's final
writes — establishes register retention or any visual-symptom mechanism. Because
the probe ran *after* the experimental bootloader's PWM write, it cannot show an
untreated PWM baseline. Reset reason, sample timing, an untreated (stock-bootloader)
baseline, and separately captured cold-power / reset-button / software-reset runs
are all required before these readings can be interpreted; register values cannot
establish optical timing or acceptance.

**[REPORTED 2026-11-05 — read-probe v2, stock bootloader restored.]**
The preceding investigation reported restoration and verification of the stock
bootloader, followed by these raw observations: AXP2101 register 0x00 returned
0x20; a flasher-triggered reset reported ESP reset reason POWERON(1), while
`esp_restart()` reported SW(3); expander responses were `02→23FF`, `03→2300`,
`04→3700`, `05→00FF`. These observations do not prove physical supply removal,
CH32 reset coupling, register width or persistence mechanisms. An ESP POWERON
reason alone is not proof that the entire board lost power.

The v2 timestamp was `micros()`, not a measured offset from application entry or
power application. It also replaced the requestFrom return count with bytes
consumed; those counts should be recorded separately. Its first stored sample
could be missed when USB reconnected, since only later samples were repeated.

On takeover, a fresh ROM read of 0x2000..0x8000 matched the verified original
bootloader-region backup byte-for-byte. This, rather than the absence of a log
line, establishes which bootloader is installed. The v3 probe saves its earliest
setup sample before console initialization and replays that same sample on each
report. It records ESP timer timestamps and deltas from setup entry for each
transaction, reset reason, boot identifier, pointer-selection status, requested
count, returned count, actual count and raw bytes. It performs no register-value
writes. Its PMU reading is raw evidence, not proof that no battery or other supply
is attached. Physical cold-power and button-reset captures remain outstanding.

**[MEASURED 2026-11-05 — v3 after user-reported long unplug/reconnect.]**
The board reappeared through the hub. The captured boot reported USB(11), with
raw responses `02→FFFF`, `03→0000`, `04→0000`, `05→FFFF` and PMU `00→20`.
Three subsequent serial-requested esp_restart trials each reported SW(3), with
the same responses for expander 0x02..0x05. Each pointer selection succeeded,
requested/reported/stored counts matched, and per-transaction timestamps were
recorded. ADC responses varied. USB-reset boots appeared when opening the host
serial connection despite configuring DTR/RTS false; the capture must not be
called passive or assumed to preserve the first power-on sample. v4 adds a
checksum-protected four-boot RTC history to investigate that measurement gap.
These responses differ from the preceding state, but do not identify register
width, CH32 reset coupling or physical backlight timing.

**[MEASURED/USER-OBSERVED 2026-11-05 — v4 post-power history.]**
After the requested unplug/reconnect, the user reported that the screen “stays
glowing.” RTC history preserved an initial POWERON(1) sample and the later
USB(11) sample under different boot identifiers. Both returned `02→FFFF`,
`03→0000`, `04→0000`, `05→FFFF`; the first PWM read spanned ESP timer
24452..24975 us (2334 us after setup entry at its start), not elapsed time from
physical power application. All recorded transport statuses/counts were valid.
This pairs a visible persistent glow with those raw responses; it does not yet
measure the physical PWM waveform or prove which read bytes carry PWM state.

**[MEASURED 2026-11-05 — v5 controlled PWM-only write.]**
With the stock bootloader still installed, v5 sent a single explicit write of
0x00 to expander register 0x05. The transaction returned status 0; immediate
reads returned `00` for length one and `00FF` for length two, replacing the
preceding `FFFF` two-byte response. Subsequent samples preserved mode/output
responses `FFFF`/`0000`. No mode, output or reset registers were written. This
establishes a response change associated with the PWM write. The user subsequently
confirmed that the glow disappeared and the screen was dark. Thus a PWM-only
zero write is sufficient to suppress the persistent glow in this tested state,
without mode/output initialization. The response latency was not measured, and
the second byte's semantics remain undetermined. This does not yet locate the
brief flash in the normal display application's boot sequence.
A subsequent serial reopen/USB(11) reset and one explicit esp_restart/SW(3)
reset each preserved the observed PWM response `00FF`, with mode/output still
`FFFF`/`0000`, and no new PWM write. The user then confirmed the screen was dark before pressing physical RST and
that the glow returned after RST. The saved button-reset boot reported
POWERON(1), with raw PWM response `FFFF` (mode/output `FFFF`/`0000`); the later
USB(11) boot preserved those readings. Thus the tested RST event changed both
the PWM response and visible state, unlike esp_restart. This is consistent with
resetting or reinitializing the expander, but does not directly measure its reset
pin or prove the schematic's exact coupling. It disproves treating all reset
types as equivalent or assuming PWM stays unchanged across the reset button.
After another PWM-zero write, the user held RST for approximately five seconds:
the screen stayed dark while held and the glow returned after release. This
further narrows the observed transition to after reset release in this test;
it does not measure the exact latency or identify the CH32 instruction that
changes the pin.



**[USER-OBSERVED + ROM-VERIFIED 2026-11-05 — download-mode isolation.]**
The user held BOOT, tapped/released RST and released BOOT, then reported that the
screen glowed in download mode. ROM-only esptool connected with `--before no-reset`
and `--after no-reset`; verification of the original bootloader region succeeded
without requesting another reset. This confirms ROM download access for the
observed condition, where the second-stage bootloader and application are not
running. The observed glow therefore does not require execution of either.

The tests support investigating the expander's startup PWM/output behavior as
a possible source of the startup glow. They do not identify its firmware implementation, an
undocumented persistent setting, a physical PWM waveform, or a guaranteed
ESP-only fix for the brief flash on a normal boot. The inspected vendor resources
and host drivers do not provide an identified CH32 firmware image/source or a
documented persistent default-off command. A lasting fix for illumination while
the ESP remains in download mode needs control independent of that ESP
bootloader/application, such as an expander startup change or a hardware gate.
Both require board-specific validation and recovery provisions. A faster ESP
hook may shorten normal-start illumination but is not an accepted fix yet.

After these captures, stock bootloader was left intact and hello_board was
rebuilt with the isolated core, flashed only at 0x10000 and verified. The
read/controlled-PWM probe is no longer the installed application. Dark-boot
acceptance for this restored Hello Board image remains FAILED.

**[MEASURED/USER-OBSERVED 2026-11-05 — real-app cold boot after restore.]**
On a real cold power-up of the restored hello_board app, the user observed a
brief boot transient that resolved to the live application screen: serial
reported `[hello] expander up, touch present`, and the panel showed the app's
own content (board name, `touch: FT6336 OK`, `tap the screen`) with a user-drawn
touch stroke tracking correctly. The final image is the live application, not a
retained frame. The brief flash was observed alongside the startup illumination history
documented above; its electrical cause and optical duration remain unmeasured,
and whether `begin(0)`
measurably shortens it was not tested. The
retained-previous-frame symptom did not persist to the final image on this cold
boot; it is seen on reset, and whether stale GRAM briefly shows during the cold
transient was not characterised.

**[USER-OBSERVED 2026-11-07 — same-board application comparison.]**
The user reported a clean cold boot and physical reset with a consumer
application using the published first-frame startup API. On the same board,
a verified application-only write of the current hello_board example reproduced
a fuzzy cold-boot flash. After drawing on the display, a physical reset showed
a fuzzy flash and part of the previous drawing in a different color. The
consumer application was restored and the application write verified. This
comparison records the observed runs. The reason for the difference and the
optical timing remain unmeasured. This hardware observation concerns the working
consumer application, not the compile-only `docs/consumer-check` fixtures.

The standalone scrolling sample also passed user-observed cold boot and physical
reset with no visible fuzzy flash or retained frame. Its tested application
SHA-256 is `e8e57ee9ea70fe8e667ed85611b7f3f18e7f9db54e05ae2a01224c267c52e572`.
The publication source rename produced a different, successfully compiled binary;
that build has no corresponding hardware acceptance. The earlier ROM-download
mode glow and Hello Board observations remain open evidence, not proof of an
electrical cause or measured optical timing.

**[USER-OBSERVED 2026-11-07 — published-source sample recheck.]**
A clean build of the scrolling sample from published source commit
`367e8f5a85016556f563f0836b1f364f2b8e127e` was installed application-only with
c5flash and verification OK. The user confirmed clean startup in response to
the cold-boot and physical-reset check, reporting that it comes up great.
Tested application SHA-256:
`8b8c3a57a4f0eb3dfd76ca6eb5d3e61c3d2e50c3448a495ae83e5d31dd833acf`
(412,384 bytes). This is a separate accepted image from the earlier scrolling
build. It does not transfer acceptance to Hello Board or establish the electrical
cause or optical timing of its startup flash. Individual control behaviors were
not separately reported during this recheck.

### Battery: the AXP2101 fuel gauge reads over plain I²C (no XPowersLib needed)

**[HW-VERIFIED 2026-10-26].** The AXP2101 PMU (`0x34`) exposes a fuel gauge you can
read with raw I²C register reads — handy where XPowersLib can't be pulled in (e.g.
a CI that doesn't install it):

- `0x03` chip ID — `0x4A` identifies the AXP2101.
- `0x18` bit 3 — fuel-gauge enable (set it if clear).
- `0x00` (STATUS1) bit 3 — battery present / cell fitted.
- `0xA4` — battery percent.

Read `0xA4` only while STATUS1 bit 3 says a cell is fitted; with no cell, hide the
reading rather than showing a bogus value. Verified: percent shown and rising
while charging on a cell-fitted unit, and correctly hidden on a cell-less unit.

### Panel can go black after a USB reset (NeoPixel on GPIO27) — and, separately, on a USB→battery switch

**[HW-VERIFIED cause, 2026-10-27].** A black panel (backlight still on) right after
an esptool USB-triggered reset is the **NeoPixel on GPIO27**. GPIO27 is the shared
**I²C SDA** line (see *NeoPixel is on GPIO27*, above), so the LED drive on reset
corrupts the bus the display's expander/touch bring-up rides on. **Disabling the
NeoPixel (Marauder [#1665](https://github.com/justcallmekoko/ESP32Marauder/pull/1665))
fixes it.** A hardware **RST** clears a stuck panel in the meantime. An earlier
note here guessed a power-rail dip "not a GPIO/signal conflict" — that was wrong
for this case; it is the GPIO27/SDA conflict.

**[symptom HW-VERIFIED, cause UNVERIFIED].** A *separate* black panel seen when
power switches **USB→battery** is **not** confirmed to share this cause, and #1665
is not known to fix it — still uninvestigated (a power-rail dip across the
switchover remains the guess). A hardware **RST** clears it too. Flagged so a dark
screen right after a reset or a power switch isn't mistaken for a bring-up failure.

### Display, touch, expander, battery, audio — bring-up summary

**[HW-VERIFIED].** Full detail and pin map in [BOARD-WS-C5-35.md](BOARD-WS-C5-35.md).
Headline: ST7796 320×480 over SPI; FT6336 touch and ES8311 audio on the shared
I²C bus; CH32V006 expander at `0x24` owns LCD reset, touch reset, and backlight
PWM (so there is no backlight GPIO and no LEDC on this board); AXP2101 PMU on the
same bus, read via XPowersLib (reports `NONE` with no cell attached but still
answers USB/charge state). The panel is rated 60 MHz but holds at an 80 MHz push
clock on the units tested.

---

## Verification ledger

| Capability | Level | Evidence |
|---|---|---|
| Legacy BLE enable under pressure | HW-VERIFIED, isolated probe | `rc=519` at 12,648 INTERNAL free / 268 DMA-eligible free; clean sampled start at 17,544 / 1,628; fragmentation also causes failures, 2026-11-04 |
| Single-core / unicore | HW-VERIFIED | core 0 of 1; createPinnedTask pins to core 0, 2026-10-23 |
| Console on USB-JTAG @115200 | HW-VERIFIED | Boot/panic logs observed |
| Arduino `Serial` on USB (CDC_ON_BOOT=1) | HW-VERIFIED | 2026-10-22, hello_board |
| Download = BOOT+RST | HW-VERIFIED | Flashing procedure in use |
| 5 GHz scan / receive | HW-VERIFIED | Channels accepted, frames seen |
| `set_band_mode` after `start` | HW-VERIFIED | Band empty if called earlier |
| Header UART bonded to chip | **FALSE** (HW-VERIFIED) | 0 edges, 2 units, 2026-10-21 |
| GPS LC76G over I²C | HW-VERIFIED | 14 sats + touch alive 2026-10-21; BSP 29 sats HDOP 0.6 2026-10-22 |
| Touch FT6336 (re-open + retry) | HW-VERIFIED | Bring-up |
| Display ST7796 @80 MHz | HW-VERIFIED | 2026-10-18 |
| Battery AXP2101 | HW-VERIFIED | BSP: 89% / 4.15 V / charging / VBUS 5.1 V, 2026-10-23 |
| Expander CH32V006 | HW-VERIFIED | Backlight/resets |
| R-selector camera-side mechanism | SCHEMATIC-CONFIRMED | R55/R56/R58/R59, official schematic, 2026-10-23 |
| LC76G over UART on the 3.5 | UNVERIFIED | Not reachable as shipped |
| NeoPixel LED on GPIO27 | **unusable** (HW-VERIFIED) | GPIO27 is I²C SDA; LED writes corrupt the bus, 2026-10-26 |
| Battery AXP2101 via raw I²C regs | HW-VERIFIED | %/charge without XPowersLib; 0x03=0x4A, 0x18.3, 0x00.3, 0xA4, 2026-10-26 |
| Backlight dimming via expander PWM | HW-VERIFIED | live dimming; `TFT_BL = -1`, driven on the CH32V006, 2026-10-26 |
| USB stalls on large transfers | HW-VERIFIED | chunk (~512 KB/4 KB) to work around, 2026-10-26 |
| ES8311 speaker playback (3.5-inch) | HW-VERIFIED (owner listen) | BSP `audio_35`: three 440 Hz tones heard clean, 2026-11-10; pitch accuracy and display+audio UNVERIFIED |
| ES8311 microphone capture (3.5-inch) | level response HW-VERIFIED, recording quality UNVERIFIED | BSP `mic_35`: left-slot AC RMS ~23 silent, ~2059 with claps (clipped at default gain), right slot zero, 2026-11-10; no captured audio listened to |
| Panel black on USB→battery / post-reset | symptom HW-VERIFIED, cause UNVERIFIED | RST clears; power-dip suspected, 2026-10-26 |

**BSP re-verification (this repo).** The `hello_board` example confirmed, on
hardware 2026-10-22: the CH32V006 expander bring-up, the ST7796 display
painting, the FT6336 touch read (via live serial touch coordinates), and Arduino
`Serial` on the native USB. Touch was observed usable but *stiff* at this phase
(every read does up to three full bus re-opens and the poll is unpaced) —
smoothing it is a Phase 2 target.

---

## Sources

- Official Waveshare schematic — confirms the R-selector mechanism, the pin map,
  and the EXIO assignments. Linked, not vendored (© Waveshare; link stays current
  with any revision):
  <https://github.com/waveshareteam/ESP32-C5-Touch-LCD-3.5/blob/main/hardware/schematic/ESP32-C5-Touch-LCD-3.5.pdf>
