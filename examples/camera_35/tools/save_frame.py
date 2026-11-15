#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Ask the camera_35 example for its last picture and save it as a PNG.

    python3 save_frame.py PORT out.png [--listen SECONDS]

Needs pyserial. Prints the board's log lines while it waits.
"""
import base64, struct, sys, time, zlib
import serial


def png(width, height, rgb):
    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))
    rows = b"".join(b"\x00" + rgb[y * width * 3:(y + 1) * width * 3] for y in range(height))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def rgb565_to_rgb(data):
    out = bytearray()
    for i in range(0, len(data), 2):
        v = data[i] | (data[i + 1] << 8)          # low byte first
        r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
        out += bytes(((r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31))
    return bytes(out)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    listen = float(sys.argv[sys.argv.index("--listen") + 1]) if "--listen" in sys.argv else 3.0
    port = serial.Serial(sys.argv[1], 115200, timeout=1)
    port.dtr = True                                # the board prints nothing without these
    port.rts = True
    end = time.time() + listen
    while time.time() < end:
        line = port.readline().decode("ascii", "replace").rstrip()
        if line:
            print(line)
    port.write(b"s")
    port.write(b"d")
    data, size, started = [], None, time.time()
    while time.time() - started < 60:
        line = port.readline().decode("ascii", "replace").strip()
        if "BEGIN" in line:
            size = tuple(int(n) for n in line.split()[-2:])
        elif "END" in line and size:
            break
        elif size:
            data.append(line)
        elif line:
            print(line)
    if not size:
        sys.exit("no picture received from the board")
    raw = base64.b64decode("".join(data))
    if len(raw) != size[0] * size[1] * 2:
        sys.exit("picture is %d bytes, expected %d" % (len(raw), size[0] * size[1] * 2))
    open(sys.argv[2], "wb").write(png(size[0], size[1], rgb565_to_rgb(raw)))
    print("saved %s (%dx%d)" % (sys.argv[2], *size))


if __name__ == "__main__":
    main()
