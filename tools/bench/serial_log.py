#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reset an ESP32-C5 board over USB-Serial-JTAG and print its serial output.

Each line is prefixed with the seconds since the reset, so a log can be lined
up against something done at the bench (a clap, a tap, a cable pulled).

    serial_log.py PORT SECONDS [FILTER]

PORT     e.g. /dev/cu.usbmodem101 or /dev/ttyACM0
SECONDS  how long to listen after the reset
FILTER   optional substring; only lines containing it are printed

Needs pyserial. The reset is the RTS pulse esptool uses for a hard reset; it
restarts the application and does not enter download mode.
"""
import sys
import time

import serial


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    port, seconds = sys.argv[1], float(sys.argv[2])
    needle = sys.argv[3] if len(sys.argv) > 3 else ""

    link = serial.Serial()
    link.port = port
    link.baudrate = 115200
    link.timeout = 0.05
    link.dtr = False
    link.rts = False
    link.open()
    link.rts = True
    time.sleep(0.1)
    link.rts = False

    start = time.time()
    pending = b""
    while time.time() - start < seconds:
        try:
            chunk = link.read(4096)
        except serial.SerialException:
            # The port drops briefly while the chip restarts; reopen and go on.
            time.sleep(0.2)
            try:
                link.close()
                link.open()
            except serial.SerialException:
                pass
            continue
        pending += chunk
        while b"\n" in pending:
            raw, pending = pending.split(b"\n", 1)
            text = raw.decode("utf-8", "replace").strip()
            if needle in text:
                print(f"t={time.time() - start:6.2f}s {text}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
