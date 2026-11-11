# 3.5-inch self-check: speaker switch, PWR button, header lines, charge current

Build with `pio run -d examples/extras_35`. For verified ROM-loader flashing,
see [c5flash](../../tools/c5flash/README.md). Monitor USB serial at 115200.
Flash only to the Waveshare ESP32-C5-Touch-LCD-3.5.

The board tests four features itself and shows a result line for each on the
screen and over serial. Press RST to run it again.

| Step | What it does | What you do |
|---|---|---|
| Charge current | Reads the setting, changes it one step, reads it back, restores it | Nothing |
| Header E8-E9 | Drives one line and reads the other, both directions, 40 readings | Fit one jumper wire between E8 and E9 before starting |
| Speaker switch | Plays a tone: 2 s on, 2 s with the amplifier off, 2 s on | Listen for tone, silence, tone |
| PWR button | Waits up to 15 s for one press and release | Press PWR once, briefly |

Without the wire the header step reports `NO WIRE`; without a press the button
step reports `NO PRESS`. Neither is a failure of the board.

## Result on hardware

One board, November 10, 2026:

- **Charge current:** PASS. Read 200 mA, set 300 and read 300, restored to 200.
- **Header E8-E9:** PASS, 40 of 40, with the wire fitted; `NO WIRE` without it.
- **Speaker switch:** a laptop-microphone recording showed the tone, then the
  room's background level for the two seconds the amplifier was off, then the
  tone again.
- **PWR button:** PASS, one press and release seen, no failed reads.

## Limits

- No battery was fitted for this example's run, so it shows only that the
  power chip stores the setting. A separate test on a board with a LiPo showed
  the charging follows the setting; see the
  [hardware findings](../../docs/HARDWARE-FINDINGS.md#battery-charge-settings-at-power-up).
  The current was not measured in milliamps.
- The header test proves logic levels through the header on E8 and E9 only.
  Output voltage and drive strength were not measured, and E10 to E15 were not
  tested through the header.
- The speaker step reports `SENT`, not PASS: the board cannot hear itself in
  this test. Use your ears, or
  [audio_selftest_35](../audio_selftest_35/README.md) for a measured check.
- A long PWR press switches the board off (vendor documentation; not tested
  here).

## Calls used

```cpp
WsC5_35::Expander::setSpeakerAmp(false);          // silence the speaker
bool pressed;
WsC5_35::Expander::powerButton(pressed);          // true while PWR is held
WsC5_35::Expander::headerMode(8, true);           // E8 as an output
WsC5_35::Expander::headerWrite(8, true);
bool level;
WsC5_35::Expander::headerRead(9, level);          // undriven inputs read high
WsC5_35::Battery::setChargeCurrentMa(400);        // after Battery::begin()
```

Never set a charge current above the cell's rated charge current. The setting
returns to the power chip's own value when the board is fully unpowered.
