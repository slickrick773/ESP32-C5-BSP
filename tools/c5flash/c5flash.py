#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""c5flash — reliable flash read/write for ESP32-C5 boards over USB-Serial-JTAG.

Works around two problems seen on Waveshare ESP32-C5 boards (chip rev v1.2):
  * the esptool stub crashes, so everything runs on the ROM loader (--no-stub);
  * some transfers stall deterministically ("Packet content transfer stopped" /
    "chip stopped responding"), and long single erases drop USB.

Strategy: keep one esptool process per operation but keep each operation small
(writes: 512 KB segments; reads: 64 KB, falling back to 4 KB with a shifted start
offset), retry only the failing piece, then verify.

Needs esptool 5.x (pip install esptool, or set C5FLASH_ESPTOOL to its path).
Port: first /dev/cu.usbmodem* (macOS) or /dev/ttyACM* (Linux).

Usage:
  c5flash.py read  <start> <end> <out.bin>
  c5flash.py write <addr> <file> [<addr> <file> ...]      (verifies afterwards)
  c5flash.py info

Status: write + verify is exercised heavily (full bootloader+partitions+app in
~33 s on the 3.5" board, repeatedly, no buttons). read mode is less exercised
end to end — treat it as working-but-unproven. Assumes the rev-v1.2 behaviour of
always running --no-stub (the stub crashes at baud change and on reads).
"""
import glob, os, shutil, subprocess, sys, tempfile

# esptool 5.x: $C5FLASH_ESPTOOL if set, else `esptool` / `esptool.py` on PATH.
ESPTOOL = (os.environ.get("C5FLASH_ESPTOOL")
           or shutil.which("esptool") or shutil.which("esptool.py"))
SEG = 512 * 1024

# Flash params. On the C5, writing the bootloader segment (at 0x2000) with "keep"
# leaves a bad flash-mode byte in the image header, and the ROM rejects it:
# "invalid header: 0x...." -> ets_flash_boot assert -> watchdog reboot loop. The
# app never runs and the screen stays dark on every build. Set them explicitly so
# esptool patches the header. Override via env if your board differs.
FLASH_MODE = os.environ.get("C5FLASH_MODE", "dio")
FLASH_FREQ = os.environ.get("C5FLASH_FREQ", "80m")
FLASH_SIZE = os.environ.get("C5FLASH_SIZE", "keep")


def port():
    p = sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))
    if not p:
        sys.exit("no USB serial port found — is the board connected?")
    return p[0]


def et(*args, before="default-reset", after="no-reset", quiet=True):
    if not ESPTOOL:
        sys.exit("esptool not found: install esptool 5.x or set C5FLASH_ESPTOOL")
    cmd = [ESPTOOL, "--chip", "esp32c5", "--port", port(), "--no-stub",
           "--before", before, "--after", after, *args]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if not quiet:
        sys.stdout.write(r.stdout[-2000:])
    return r.returncode == 0, r.stdout + r.stderr


def read_piece(addr, n):
    for shift in (0, 2, 5, 16, 32, 7, 11):
        fd, path = tempfile.mkstemp(); os.close(fd)
        ok, _ = et("read-flash", hex(addr - shift), str(n + shift), path)
        data = open(path, "rb").read(); os.unlink(path)
        if ok and len(data) == n + shift:
            return data[shift:]
    if n > 0x1000:
        out = b""
        for a in range(addr, addr + n, 0x1000):
            d = read_piece(a, 0x1000)
            if d is None:
                return None
            out += d
        return out
    return None


def cmd_read(start, end, out):
    start, end = int(start, 0), int(end, 0)
    buf = bytearray()
    for a in range(start, end, 0x10000):
        n = min(0x10000, end - a)
        d = read_piece(a, n)
        if d is None:
            sys.exit(f"read failed at {a:#x}")
        buf += d
        print(f"\rread {a - start + n:>9}/{end - start} bytes", end="", flush=True)
    open(out, "wb").write(buf)
    print(f"\nwrote {out} ({len(buf)} bytes)")


def cmd_write(pairs):
    pieces, verify = [], []
    tmp = tempfile.mkdtemp()
    try:
        for addr_s, path in zip(pairs[::2], pairs[1::2]):
            addr, data = int(addr_s, 0), open(path, "rb").read()
            verify += [hex(addr), path]
            for i in range(0, len(data), SEG):
                p = os.path.join(tmp, f"seg_{addr + i:08x}.bin")
                open(p, "wb").write(data[i:i + SEG])
                pieces.append((addr + i, p))
        first = True
        for addr, p in pieces:
            for attempt in range(4):
                ok, outp = et("--baud", "115200", "write-flash", "-z", "--flash-mode", FLASH_MODE,
                              "--flash-freq", FLASH_FREQ, "--flash-size", FLASH_SIZE, hex(addr), p,
                              before="default-reset" if first else "no-reset")
                if ok:
                    break
                first = True  # reconnect with a reset on retry
            else:
                sys.exit(f"write failed at {addr:#x}:\n{outp[-400:]}")
            first = False
            print(f"wrote {os.path.getsize(p):>7} bytes at {addr:#09x}")
        ok, outp = et("verify-flash", *verify, before="no-reset", after="watchdog-reset")
        print("verify:", "OK" if ok and "mismatch" not in outp else "FAILED")
        if not ok:
            sys.exit(outp[-600:])
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def cmd_info():
    ok, outp = et("chip-id", after="watchdog-reset")
    seen = set()
    for line in outp.splitlines():
        if line.startswith(("Chip type", "BASE MAC")) and line not in seen:
            seen.add(line)
            print(line)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    c = sys.argv[1]
    if c == "read" and len(sys.argv) == 5:
        cmd_read(*sys.argv[2:])
    elif c == "write" and len(sys.argv) >= 4 and len(sys.argv) % 2 == 0:
        cmd_write(sys.argv[2:])
    elif c == "info":
        cmd_info()
    else:
        sys.exit(__doc__)
