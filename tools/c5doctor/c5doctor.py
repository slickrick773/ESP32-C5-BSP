#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
c5doctor — find out what is wrong with an ESP32-C5 board that looks dead, and
put a known-good test image on it.

A board with a dark screen is very rarely broken. Usually the chip's built-in
loader (the ROM) still answers over USB and only what is stored in flash is
wrong. The doctor asks the ROM, reads back what is stored, says which part is
wrong in plain words, and can write a known-good image and check the result.

It builds on c5flash (same folder level: ../c5flash) and needs what that needs:
Python 3 and esptool 5.x (pip install esptool, or set C5FLASH_ESPTOOL).

Usage:
  c5doctor.py [--port DEV] check              look, change nothing (the board restarts)
  c5doctor.py [--port DEV] cure <pack> [--yes] write a known-good image, then check again
  c5doctor.py pack <build-dir> <board> <out.zip>
                                              make a cure pack from a PlatformIO build folder

`check` never writes. `cure` REPLACES the firmware on the board; before it does,
it saves the first 64 KB of flash (bootloader, partition table and start-up
choice) next to the pack as a .bin, and asks you to type yes unless --yes is given.

Exit codes: 0 healthy, 1 something is wrong that `cure` can fix, 2 the doctor
could not reach the board, 3 the cure did not take.

What it cannot do: if the ROM never answers (no USB port appears, or the chip
does not reply even in download mode), nothing sent over USB can fix the board.
The doctor says so instead of guessing.
"""
import glob, hashlib, json, os, struct, sys, time, zipfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "c5flash"))
import c5flash

BOOTLOADER_AT = 0x2000          # On the C5 the ROM loads the bootloader from here, not from 0.
TABLE_AT = 0x8000
TABLE_SIZE = 0xC00
IMAGE_MAGIC = 0xE9
ENTRY_MAGIC = b"\xAA\x50"
APP_DESC_MAGIC = b"\x32\x54\xCD\xAB"
SAVE_SIZE = 0x10000
GENERIC_NAME = "arduino-lib-builder"   # What every Arduino build calls itself: it tells nothing.

# What `diagnose` can conclude, worst first.
NO_ANSWER, NO_BOOTLOADER, BOOTLOADER_AT_ZERO, NO_TABLE, NO_APP, RESTARTING, HEALTHY = (
    "no_answer", "no_bootloader", "bootloader_at_zero", "no_table", "no_app", "restarting", "healthy")

ADVICE = {
    NO_ANSWER: "The chip did not answer. Try: a different USB cable (many carry power only), a port on the "
               "computer itself, then download mode by hand: hold BOOT, tap RST, release BOOT, and run this "
               "again. If it never answers in download mode, no software can fix it.",
    NO_BOOTLOADER: "Nothing the chip can start from is stored at 0x2000. The screen stays dark and the chip "
                   "restarts over and over. `cure` fixes this.",
    BOOTLOADER_AT_ZERO: "A bootloader is stored at 0x0, where other ESP32 chips keep it. The C5 looks at "
                        "0x2000, finds none, and restarts over and over. This is the usual way a C5 gets "
                        "\"bricked\" by a flashing tool set up for another chip. `cure` fixes this.",
    NO_TABLE: "The bootloader is there but the partition table is missing or damaged, so it cannot find a "
              "program to run. `cure` fixes this.",
    NO_APP: "The bootloader and partition table are there, but no program is stored where they point. "
            "`cure` fixes this.",
    RESTARTING: "Everything stored looks right, but the board keeps restarting. The program on it is "
                "crashing at start-up. `cure` replaces it with a known-good test image.",
    HEALTHY: "The chip answers, and a bootloader, a partition table and a program are all in place. This "
             "board is not bricked.",
}


# --- reading what is stored -------------------------------------------------

def parse_table(raw):
    """The partitions in a table, or None if this is not a table. Each is a dict:
    label, type, subtype, offset, size."""
    if not raw or len(raw) < 32 or raw[:2] != ENTRY_MAGIC:
        return None
    found = []
    for at in range(0, len(raw) - 31, 32):
        entry = raw[at:at + 32]
        if entry[:2] != ENTRY_MAGIC:
            break                                   # The checksum entry or blank flash: the end.
        kind, subtype, offset, size = struct.unpack_from("<BBII", entry, 2)
        label = entry[12:28].split(b"\0", 1)[0].decode("ascii", "replace")
        found.append({"label": label, "type": kind, "subtype": subtype, "offset": offset, "size": size})
    return found or None


def apps_in(table):
    return [p for p in table or [] if p["type"] == 0]


def otadata_in(table):
    for p in table or []:
        if p["type"] == 1 and p["subtype"] == 0:
            return p
    return None


def describe_app(raw):
    """What a program says about itself, or None if these bytes are not a program."""
    if not raw or raw[0] != IMAGE_MAGIC:
        return None
    said = {"name": "", "version": "", "built": ""}
    if len(raw) >= 0x90 and raw[0x20:0x24] == APP_DESC_MAGIC:
        text = lambda a, n: raw[a:a + n].split(b"\0", 1)[0].decode("ascii", "replace")
        said = {"version": text(0x30, 32), "name": text(0x50, 32), "built": (text(0x80, 16) + " " + text(0x70, 16)).strip()}
    return said


def diagnose(answers, at_zero, at_bootloader, table_raw, app_heads, restarts=None):
    """One conclusion from what was read. `app_heads` is the first bytes of each
    program partition, in table order. `restarts` is how many times the board was
    seen to restart while listened to, or None if it was not listened to."""
    if not answers:
        return NO_ANSWER
    if not at_bootloader or at_bootloader[0] != IMAGE_MAGIC:
        return BOOTLOADER_AT_ZERO if at_zero and at_zero[0] == IMAGE_MAGIC else NO_BOOTLOADER
    table = parse_table(table_raw)
    if not table or not apps_in(table):
        return NO_TABLE
    if not any(describe_app(head) is not None for head in app_heads):
        return NO_APP
    if restarts is not None and restarts >= 3:
        return RESTARTING
    return HEALTHY


# --- talking to the board ---------------------------------------------------

def chip_facts():
    """(answers, facts). Tries a reset into the loader first, then a board that was
    put into download mode by hand."""
    for before in ("default-reset", "no-reset", "default-reset"):
        ok, out = c5flash.et("flash-id", before=before)
        if ok:
            facts = {}
            for line in out.splitlines():
                for key, name in (("Chip type:", "chip"), ("Detected flash size:", "flash"), ("BASE MAC:", "address"), ("MAC:", "address")):
                    if line.startswith(key) and name not in facts:
                        facts[name] = line.split(":", 1)[1].strip()
            return True, facts
        time.sleep(1)
    return False, {}


def listen_for_restarts(port, seconds=8):
    """How many times the board announces a restart while listened to, and the lines
    of a board check if one is running. (None, []) if this Python has no pyserial."""
    try:
        import serial
    except ImportError:
        return None, []
    restarts, lines, pending = 0, [], b""
    until = time.time() + seconds
    link = None
    while time.time() < until:
        try:
            if link is None:
                link = serial.Serial(port, 115200, timeout=0.2)
            pending += link.read(4096)
        except (OSError, serial.SerialException):
            link = None                              # The port goes away for a moment at each restart.
            time.sleep(0.2)
            continue
        while b"\n" in pending:
            line, pending = pending.split(b"\n", 1)
            text = line.decode("utf-8", "replace").strip()
            if text.startswith("ESP-ROM:") or "invalid header" in text:
                restarts += 1
            if "board_check" in text:               # The BSP's board check: its table of results.
                lines.append(text)
    if link is not None:
        link.close()
    return restarts, lines


def examine(listen=True, seconds=8):
    """Read everything `diagnose` wants. Returns (conclusion, report lines, table)."""
    report = []
    answers, facts = chip_facts()
    if not answers:
        return NO_ANSWER, report, None
    report.append("chip:        %s" % facts.get("chip", "answers, type not read"))
    report.append("flash:       %s" % facts.get("flash", "size not read"))
    if facts.get("address"):
        report.append("address:     ends %s" % facts["address"][-8:])
    at_zero = c5flash.read_piece(0, 16)
    at_bootloader = c5flash.read_piece(BOOTLOADER_AT, 16)
    table_raw = c5flash.read_piece(TABLE_AT, TABLE_SIZE)
    table = parse_table(table_raw)
    report.append("bootloader:  %s" % ("present at 0x2000" if at_bootloader and at_bootloader[0] == IMAGE_MAGIC else
                                       "MISSING at 0x2000" + (" (one is at 0x0)" if at_zero and at_zero[0] == IMAGE_MAGIC else "")))
    report.append("partitions:  %s" % ("%d entries" % len(table) if table else "MISSING or damaged"))
    heads = []
    for part in apps_in(table):
        head = c5flash.read_piece(part["offset"], 0x100)
        heads.append(head)
        said = describe_app(head)
        if said is None:
            report.append("program:     none in '%s' (0x%x)" % (part["label"], part["offset"]))
        else:
            name = " ".join(x for x in (said["name"], said["version"]) if x) or "present, says nothing about itself"
            if said["name"] == GENERIC_NAME:
                name = "an Arduino program (it does not name itself)"
            report.append("program:     '%s' (0x%x): %s%s" % (part["label"], part["offset"], name, ", built " + said["built"] if said["built"] else ""))
    restarts, check_lines = None, []
    stored_ok = diagnose(True, at_zero, at_bootloader, table_raw, heads) == HEALTHY
    c5flash.et("chip-id", after="watchdog-reset")       # Let the board start what it has.
    if listen:
        restarts, check_lines = listen_for_restarts(c5flash.PORT, seconds)
        if restarts is None:
            report.append("start-up:    not watched (this Python has no pyserial)")
        else:
            report.append("start-up:    %s" % ("restarted %d times in %d s" % (restarts, seconds) if restarts >= 3 else "steady"))
    report += ["  " + line for line in check_lines]
    conclusion = diagnose(True, at_zero, at_bootloader, table_raw, heads, restarts if stored_ok else None)
    return conclusion, report, table


def cmd_check():
    conclusion, report, _ = examine()
    print("\n".join(report))
    print("\nfinding: %s\n%s" % (conclusion.replace("_", " "), ADVICE[conclusion]))
    return {HEALTHY: 0, NO_ANSWER: 2}.get(conclusion, 1)


# --- cure packs -------------------------------------------------------------

PACK_FILES = ("bootloader.bin", "partitions.bin", "firmware.bin")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def cmd_pack(build_dir, board, out):
    parts = {}
    for name in PACK_FILES:
        with open(os.path.join(build_dir, name), "rb") as source:
            parts[name] = source.read()
    table = parse_table(parts["partitions.bin"])
    if parts["bootloader.bin"][0] != IMAGE_MAGIC or describe_app(parts["firmware.bin"]) is None or not apps_in(table):
        sys.exit("that folder does not hold a bootloader, a partition table and a program")
    said = describe_app(parts["firmware.bin"])
    label = said["name"] if said["name"] and said["name"] != GENERIC_NAME else os.path.basename(os.path.normpath(build_dir))
    manifest = {"chip": "esp32c5", "board": board, "program": label, "version": "" if said["name"] == GENERIC_NAME else said["version"],
                "files": {name: sha(data) for name, data in parts.items()}}
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as pack:
        pack.writestr("manifest.json", json.dumps(manifest, indent=2) + "\n")
        for name, data in parts.items():
            pack.writestr(name, data)
    print("wrote %s: %s for the %s" % (out, label, board))


def open_pack(path):
    """(manifest, {name: bytes}) from a pack, refusing one whose files do not match
    what its manifest says."""
    with zipfile.ZipFile(path) as pack:
        manifest = json.loads(pack.read("manifest.json"))
        parts = {name: pack.read(name) for name in PACK_FILES}
    if manifest.get("chip") != "esp32c5":
        sys.exit("this pack is not for the ESP32-C5")
    for name, data in parts.items():
        if manifest.get("files", {}).get(name) != sha(data):
            sys.exit("the pack is damaged: %s does not match its manifest" % name)
    return manifest, parts


def cure_plan(parts):
    """What to write and where: (address, bytes) in order. The start-up choice is
    blanked so the bootloader runs the first program slot, which is where the
    pack's program goes."""
    table = parse_table(parts["partitions.bin"])
    apps, choice = apps_in(table), otadata_in(table)
    if not apps:
        sys.exit("the pack's partition table has no place for a program")
    plan = [(BOOTLOADER_AT, parts["bootloader.bin"]), (TABLE_AT, parts["partitions.bin"])]
    if choice:
        plan.append((choice["offset"], b"\xFF" * choice["size"]))
    plan.append((apps[0]["offset"], parts["firmware.bin"]))
    if len(parts["firmware.bin"]) > apps[0]["size"]:
        sys.exit("the pack's program does not fit its own partition table")
    return plan


def cmd_cure(pack_path, yes):
    manifest, parts = open_pack(pack_path)
    plan = cure_plan(parts)
    answers, facts = chip_facts()
    if not answers:
        print("finding: no answer\n" + ADVICE[NO_ANSWER])
        return 2
    if "C5" not in facts.get("chip", "C5"):
        sys.exit("this is not an ESP32-C5 (%s): nothing written" % facts["chip"])
    print("This will REPLACE the firmware on the board with: %s %s, for the %s." %
          (manifest.get("program") or "a test program", manifest.get("version", ""), manifest.get("board", "?")))
    print("Make sure that is the board you have: an image for another board can leave the screen dark.")
    if not yes and input("Type yes to go on: ").strip().lower() != "yes":
        print("nothing written")
        return 1
    saved = os.path.splitext(pack_path)[0] + ".before-%s.bin" % time.strftime("%Y%m%d-%H%M%S")
    before = c5flash.read_piece(0, SAVE_SIZE)
    if before is None:
        sys.exit("could not read the board's first 64 KB to save it: nothing written")
    with open(saved, "wb") as keep:
        keep.write(before)
    print("saved the first 64 KB as it was: %s" % saved)
    import shutil, tempfile
    folder = tempfile.mkdtemp()
    try:
        pairs = []
        for address, data in plan:
            path = os.path.join(folder, "at_%08x.bin" % address)
            with open(path, "wb") as piece:
                piece.write(data)
            pairs += [hex(address), path]
        c5flash.cmd_write(pairs)                     # Exits with the reason if a write or the verify fails.
    finally:
        shutil.rmtree(folder, ignore_errors=True)
    time.sleep(2)
    conclusion, report, _ = examine(seconds=20)      # Long enough for a board check to print its table.
    print("\n".join(report))
    print("\nfinding after the cure: %s\n%s" % (conclusion.replace("_", " "), ADVICE[conclusion]))
    return 0 if conclusion == HEALTHY else 3


def main(args):
    explicit, rest = c5flash.parse_port_opt(args)
    yes = "--yes" in rest
    rest = [a for a in rest if a != "--yes"]
    if rest[:1] == ["pack"] and len(rest) == 4:
        cmd_pack(*rest[1:])
        return 0
    if rest[:1] == ["check"] and len(rest) == 1 or rest[:1] == ["cure"] and len(rest) == 2:
        ports = sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))
        if explicit is None and not ports:
            print("finding: no answer\nNo USB serial port appeared at all. " + ADVICE[NO_ANSWER])
            return 2
        c5flash.PORT = c5flash.resolve_port(explicit)
        return cmd_check() if rest[0] == "check" else cmd_cure(rest[1], yes)
    sys.exit(__doc__)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
