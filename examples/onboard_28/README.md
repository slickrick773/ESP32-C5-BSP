# 2.8-inch onboard devices readout

**Hardware-tested on November 10, 2026.** On a 2.8-inch board the clock was
found, accepted a set and counted one second per second; the IMU identified
itself and read 1.04 g total at rest; the temperature/humidity sensor read
about 23 °C and 47 % with valid checksums.

Build with `pio run -d examples/onboard_28`. For verified ROM-loader flashing,
see [c5flash](../../tools/c5flash/README.md). Monitor USB serial at 115200.
Flash only to the Waveshare ESP32-C5-Touch-LCD-2.8.

Once per second the example prints the real-time clock, the IMU's acceleration
and angular rate, and the board temperature and humidity. BOOT button presses
and releases are printed as they happen. No display is rendered.

If the clock reports that it lost power, the example sets it to the firmware's
build time so that it can be seen counting. That is not the real time.

## Notes

- The drivers are shared with the 3.5-inch board
  ([onboard_35](../onboard_35/README.md)): PCF85063 clock at `0x51`, QMI8658
  IMU at `0x6B`, SHTC3 sensor at `0x70`, here on SDA 0 / SCL 1.
- This board has a socket labelled RTC for a clock backup cell. Whether the
  clock keeps time from a cell fitted there is untested.
- At rest the IMU read about −1.03 g on Z with the board lying as tested; axis
  orientation on this board is not characterised.
- A BOOT button press, IMU movement and sensor accuracy are untested on this
  board. GPIO 28 is a boot-mode strapping pin: holding BOOT through a reset
  enters download mode.
