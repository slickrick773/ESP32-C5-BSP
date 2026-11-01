# 2.8-inch battery voltage

Standalone screen and USB-serial readout for the Waveshare ESP32-C5-Touch-LCD-2.8.
Build with `pio run -d examples/battery_28`. Console: 115200 baud.
Use the matching single-cell battery connector and check polarity before connecting.

The battery signal is read through the onboard I/O expander at I2C address
`0x24`, register `0x06`, on SDA 0 / SCL 1. The response is a little-endian
10-bit ADC sample. Eight samples, spaced by 5 ms, are averaged and rounded.
Nominal conversion: `millivolts = round(raw * 9900 / 1023)` (3.3 V reference,
3:1 divider). This is an estimated voltage, not a calibrated measurement.

Protocol and scaling source: Waveshare's
[Board_IO.cpp](https://github.com/waveshareteam/ESP32-C5-Touch-LCD-2.8/blob/0196dcb7e601d6faa1c33785a71f6b0c35b532e6/examples/arduino/libraries/board_config/src/Board_IO.cpp)
and adjacent `Board_IO.h`. This implementation uses the BSP shared-bus lock
and C5 reopen discipline for each sample, releasing the lock before waiting.
The battery driver only selects/reads the ADC register; it does not configure
outputs, reset the display/touch, enable audio, or change charger settings.
The screen example separately initializes the expander and display. Initialize the
bus once in setup before starting concurrent users.

`Battery::read()` leaves its caller's reading unchanged on failure. The
example prints an explicit failure instead of displaying a stale voltage.
Zero is valid ADC data but does not prove the battery is absent. No battery
percentage, presence, charging status, or USB-power detection is claimed.

Status: all seven host suites and the pinned `battery_28` PlatformIO build
passed. The serial readout and continued battery-only operation have now been observed;
voltage accuracy remains uncalibrated. A future accuracy check should compare
against a multimeter. USB-powered readings
alone do not verify battery-only operation or charging behavior. GPS and
simultaneous display/touch/audio operation are outside this test.

## Battery-only screen test

The example uses Arduino_GFX 1.6.9 and the existing 2.8-inch panel initialization.
The screen shows estimated voltage, raw ADC, elapsed seconds since boot and a
blinking dot. It does not infer the power source. USB logging is optional and
startup never waits for a USB host.

With the battery connected, first confirm the screen updates while USB is
connected. Unplug USB and watch for at least 30 seconds. Continuing seconds and
the blinking dot demonstrate continued execution; a frozen image does not.
Report a black screen, frozen timer or timer resetting to zero. A battery-only
cold start is a separate test. Voltage calibration remains unverified.

Initial serial-only firmware was flashed and verified on the 2.8-inch board;
it returned raw 425–426 (estimated 4113–4123 mV) with battery and USB connected.
The screen build was flashed with verification OK. The owner confirmed that
the counter and screen continued updating after USB was unplugged. This verifies
continued battery-only operation, not cold-start behavior or runtime capacity.
