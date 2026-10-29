# GPS on the Waveshare ESP32-C5-Touch-LCD-3.5

This board's GPS is an **LC76G**, and on this hardware it is reached over **I²C**,
not UART. This note explains how the I²C path works, why it is the one the BSP
uses, and what to know if you would rather bring a UART GPS instead.

## TL;DR

- The LC76G rides the **shared I²C bus** (SDA 27 / SCL 26), the same bus as the
  touch panel, the CH32V006 expander and the AXP2101.
- It needs **no extra GPIO** and leaves the **BOOT button free**, so flashing and
  download-mode recovery are unaffected.
- Verified on hardware: **14 satellites** with touch, audio and battery all running
  at the same time.

## Why I²C is the path on this board

Two board facts drive the choice:

1. **The J3 header UART is not bonded to the chip as shipped.** A plain UART GPS
   wired to the header is never heard — see
   [HARDWARE-FINDINGS.md](HARDWARE-FINDINGS.md) → *The J3 header UART is NOT bonded
   to the chip*. So a UART GPS has to be wired onto a spare GPIO instead (more on
   that below).
2. **The LC76G already speaks I²C**, and the bus it needs is already up for touch.
   So the GPS comes in over a bus the firmware is already running, with no new pins
   and no wiring onto a strapping line.

The practical upshot is that the I²C path costs nothing in GPIOs and touches none
of the boot/strapping pins, which keeps the BOOT button available for entering
download mode. On the C5, download-mode recovery leans on holding BOOT at
power-on (see [HARDWARE-FINDINGS.md](HARDWARE-FINDINGS.md) → *Bootloader offset is
`0x2000` on the C5*), so keeping that pin free is worth doing.

## How it works

The LC76G is read with Waveshare's official **`LC76G_RPI_I2C`** protocol on the
shared bus:

| Op | Address |
|----|---------|
| command | `0x50` |
| read    | `0x54` |
| wake    | `0x58` |

Because it shares the bus with touch, the transport follows the BSP's shared-bus
discipline:

- **One bus, one lock.** Every transaction goes through the shared lock; nothing
  re-opens or re-inits the bus per cycle.
- **STOP-based reads.** Read with a STOP (`endTransmission(true)` then
  `requestFrom`), not a repeated-start — a repeated-start read can wedge the bus
  for other devices on it.
- **Skip, don't stall.** If the bus is busy (a finger is down), skip this GPS tick
  rather than blocking touch; pick it up on the next pass.

The reference transport is `Gps::tick(sink, now, busy)` — it takes the bus lock,
yields when another owner holds it, and parses NMEA out to the sink. Reusing it
gives you the shared-bus behaviour for free.

### Wiring (external module)

If your unit's onboard LC76G is not populated and you are adding an I²C GNSS
module, it wires to the same shared bus:

| Module | Board |
|--------|-------|
| SDA | GPIO 27 |
| SCL | GPIO 26 |
| VCC | 3V3 |
| GND | GND |

No level shifting and no dedicated UART pins are involved.

## If you would rather use a UART GPS

It is possible, with two things to keep in mind on this board:

- **The free pins double as strapping pins.** Bringing a UART GPS in means wiring
  it onto a spare GPIO, and the convenient ones on this board are boot/strapping
  pins. Putting a UART on a boot-strap line means giving up BOOT-button polling in
  firmware, and it can make entering download mode for reflashing less
  straightforward. That is a workable trade — just a trade to make on purpose.
- **Never use GPIO13 / GPIO14.** Those are the native USB-Serial-JTAG D- / D+
  lines. Opening a hardware UART on them drops the board off USB at boot and breaks
  auto-download — see [HARDWARE-FINDINGS.md](HARDWARE-FINDINGS.md) → *Never assign a
  UART to GPIO13/14*.

For this board the I²C path avoids both of these, which is why it is the default
here. The UART path is documented for completeness and is a reasonable choice if
your build already needs a UART GPS and you can spare a strapping pin for it.

## See also

- [HARDWARE-FINDINGS.md](HARDWARE-FINDINGS.md) — the LC76G-over-I²C bring-up notes,
  the dead header UART, and the shared-bus discipline.
- [BOARD-WS-C5-35.md](BOARD-WS-C5-35.md) — the board's I²C address map and pinout.
