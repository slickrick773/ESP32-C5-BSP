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

Download mode: flashing is native-USB auto-download on the units tested (no
button dance needed). If auto-enter ever fails: **hold BOOT, tap RST, release
BOOT**, then flash.

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

> **R-selector theory — [UNVERIFIED].** The leading explanation (not visually
> confirmed) is that the factory leaves the 0-ohm selectors (R56/R59 for
> GPIO11/12, R55/R58 for GPIO0/1) set to the *camera* side even on no-camera
> units, so GPIO12 never reaches J3. If true, a resistor rework (move the GPIO12
> selector to the header side) would restore header UART. **This has not been
> probed under magnification or confirmed against the schematic — treat it as a
> hypothesis, not a fix to recommend.** The shipping, no-rework solution is I²C
> (below), which needs no soldering.

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

**[HW-VERIFIED].** The FT6336 touch, the ES8311 codec, the CH32V00x expander, and
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

### Display, touch, expander, battery, audio — bring-up summary

**[HW-VERIFIED].** Full detail and pin map in [BOARD-WS-C5-35.md](BOARD-WS-C5-35.md).
Headline: ST7796 320×480 over SPI; FT6336 touch and ES8311 audio on the shared
I²C bus; CH32V00x expander at `0x24` owns LCD reset, touch reset, and backlight
PWM (so there is no backlight GPIO and no LEDC on this board); AXP2101 PMU on the
same bus, read via XPowersLib (reports `NONE` with no cell attached but still
answers USB/charge state). The panel is rated 60 MHz but holds at an 80 MHz push
clock on the units tested.

---

## Verification ledger

| Capability | Level | Evidence |
|---|---|---|
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
| Battery AXP2101 | HW-VERIFIED | Readout |
| Expander CH32V00x | HW-VERIFIED | Backlight/resets |
| R-selector camera-side theory | UNVERIFIED | Not probed/schematic-confirmed |
| LC76G over UART on the 3.5 | UNVERIFIED | Not reachable as shipped |

**BSP re-verification (this repo).** The `hello_board` example confirmed, on
hardware 2026-10-22: the CH32V00x expander bring-up, the ST7796 display
painting, the FT6336 touch read (via live serial touch coordinates), and Arduino
`Serial` on the native USB. Touch was observed usable but *stiff* at this phase
(every read does up to three full bus re-opens and the poll is unpaced) —
smoothing it is a Phase 2 target.
