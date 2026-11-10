# Bench helpers

Small host-side scripts used while hardware-testing the examples.

## serial_log.py

Resets a board over its USB serial port and prints what it says, with each
line stamped with the seconds since the reset:

```
python3 tools/bench/serial_log.py /dev/cu.usbmodem101 10 "[mic_35]"
```

The timestamps make it possible to line a log up against something done at the
bench, such as a clap two seconds after the capture started. Needs `pyserial`.

A flash that verifies does not show the application booted. Reading the first
seconds of serial output after a reset does.
