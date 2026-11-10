# 3.5-inch onboard devices readout

Build with `pio run -d examples/onboard_35`. For verified ROM-loader flashing,
see [c5flash](../../tools/c5flash/README.md). Monitor USB serial at 115200.
Flash only to the Waveshare ESP32-C5-Touch-LCD-3.5.

Once per second the example prints the real-time clock, the IMU's acceleration
and angular rate, and the board temperature and humidity. BOOT button presses
and releases are printed as they happen. At startup it reports whether USB
and a LiPo are present. No display is rendered and the backlight stays off.

If the clock reports that it lost power, the example sets it to the firmware's
build time so that it can be seen counting. That is not the real time.

## Hardware result (November 10, 2026)

Run on two boards: one with the camera and no LiPo, one with a LiPo. The
clock, IMU and temperature/humidity sensor were found and read on both.


| Device | Observed | Not tested |
|---|---|---|
| Clock at `0x51` | Found; accepted a set; counted one second per second; held a steady offset from the host clock (within 1 s) over 40 minutes of resets and reflashes. **No LiPo: lost its time** after USB was unplugged for 7 to 9 seconds, twice. **LiPo fitted (second board): kept its time** through 10 seconds unplugged, offset from the host unchanged within 1 s | Accuracy over days; how long a LiPo carries it |
| IMU at `0x6B` | Identity `0x05`; flat with the screen up: Z +1.00 g, gyro within ±0.6 dps. Stood on edge with the USB connector down: X −0.93 g. Screen facing the viewer with the USB connector to the right: Y −0.97 g. Turned over: Z about −1.17 g, gyro peak near 221 dps | Accuracy; the three poses were held by hand |
| BOOT button, GPIO 28 | Reads released when untouched; two presses and releases were reported when the owner pressed it twice | Behaviour when held through a reset (expected: download mode) |
| Temperature/humidity at `0x70` | Identity matched an SHTC3 (`0x0887`); read about 28 °C and 37 % with checksums valid | Accuracy. It measures the board, which runs warmer than the room |

## Notes

- **The clock has no backup cell on this board.** It is powered through the
  PMU, whose backup pin has only a small capacitor on the vendor schematic. It
  keeps time while USB or a LiPo powers the board and loses it when both are
  gone. Check the `integrityLost` result of `Rtc::read()`.
- The vendor's label names the clock PCF85063, the IMU QMI8658 and the sensor
  SHTC3; the identities read from the chips agree.
- GPIO 28 is also a boot-mode strapping pin. Holding BOOT through a reset
  enters download mode instead of running the application.
- All drivers take the shared I²C bus lock for every call.
