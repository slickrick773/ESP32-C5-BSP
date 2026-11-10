# 3.5-inch onboard devices readout

Build with `pio run -d examples/onboard_35`. For verified ROM-loader flashing,
see [c5flash](../../tools/c5flash/README.md). Monitor USB serial at 115200.
Flash only to the Waveshare ESP32-C5-Touch-LCD-3.5.

Once per second the example prints the real-time clock and the IMU's
acceleration and angular rate. BOOT button presses and releases are printed as
they happen. No display is rendered and the backlight stays off.

If the clock reports that it lost power, the example sets it to the firmware's
build time so that it can be seen counting. That is not the real time.

## Hardware result (November 10, 2026)

| Device | Observed | Not tested |
|---|---|---|
| Clock at `0x51` | Found; accepted a set; counted one second per second for 15 seconds; kept counting across a reset | Keeping time with power removed; accuracy |
| IMU at `0x6B` | Identity `0x05`; lying flat and still it read 1.00 g total, about +1.00 g on Z, gyro within ±0.6 dps. Turned over by hand, Z went to about −1.17 g and the gyro peaked near 221 dps | Which way each axis points on the board; accuracy |
| BOOT button, GPIO 28 | Reads released when untouched; two presses and releases were reported when the owner pressed it twice | Behaviour when held through a reset (expected: download mode) |

## Notes

- The clock's registers behave like a PCF85063 and the IMU identifies as a
  QMI8658. Neither part marking has been read.
- GPIO 28 is also a boot-mode strapping pin. Holding BOOT through a reset
  enters download mode instead of running the application.
- Both drivers take the shared I²C bus lock for every call.
