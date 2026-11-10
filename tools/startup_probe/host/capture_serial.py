#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reconnecting capture; optionally request probe esp_restart() trials.
Sets DTR/RTS false, but opening USB can still reset the ESP. Requires pyserial.
"""
import argparse
import json
import time
from pathlib import Path
import serial

p = argparse.ArgumentParser()
p.add_argument('--port', required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--seconds', type=float, default=45)
p.add_argument('--software-resets', type=int, default=0)
p.add_argument('--pwm-zero-at', type=float, help='Seconds before sending v5 PWM-zero command')
a = p.parse_args()
start = time.monotonic()
connection = None
resets = 0
next_reset = start + 6
pending = b''
pwm_zero_sent = False
v5_seen = False
with a.output.open('w') as out:
    def record(kind, data):
        out.write(json.dumps({'host_elapsed_s': round(time.monotonic()-start, 6),
                              'kind': kind, 'data': data})+'\n')
        out.flush()
    try:
        while time.monotonic() - start < a.seconds:
            try:
                if connection is None:
                    connection = serial.Serial(port=None, baudrate=115200, timeout=.2)
                    connection.dtr = False
                    connection.rts = False
                    connection.port = a.port
                    connection.open()
                    record('connected', 'requested port opened with DTR/RTS false; USB reset is possible')
                data = connection.read(4096)
                pending += data
                while b'\n' in pending:
                    line, pending = pending.split(b'\n', 1)
                    decoded = line.decode('utf-8', errors='replace').rstrip('\r')
                    v5_seen = v5_seen or '[exio_probe_v5]' in decoded
                    record('serial', decoded)
                if (a.pwm_zero_at is not None and not pwm_zero_sent and v5_seen
                        and time.monotonic() - start >= a.pwm_zero_at):
                    connection.write(b'0')
                    connection.flush()
                    pwm_zero_sent = True
                    record('request', 'PWM zero only; no mode/output writes')
                if resets < a.software_resets and time.monotonic() >= next_reset:
                    connection.write(b'r')
                    connection.flush()
                    resets += 1
                    record('request', 'esp_restart trial %d' % resets)
                    next_reset = time.monotonic() + 6
            except (serial.SerialException, OSError) as exc:
                record('disconnected', type(exc).__name__)
                if connection is not None:
                    connection.close()
                connection = None
                if pending:
                    record('partial', pending.decode('utf-8', errors='replace'))
                    pending = b''
                time.sleep(.2)
    finally:
        if pending:
            record('partial', pending.decode('utf-8', errors='replace'))
        if connection is not None:
            connection.close()
print('Capture complete; software-reset requests:', resets)
