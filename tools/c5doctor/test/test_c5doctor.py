#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Host-only tests for c5doctor: what it concludes from what it reads, and what a
cure writes where. No hardware and no esptool.

Run:  python3 tools/c5doctor/test/test_c5doctor.py
"""
import os, struct, sys, tempfile, unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import c5doctor as d


def entry(label, kind, subtype, offset, size):
    return b"\xAA\x50" + struct.pack("<BBII", kind, subtype, offset, size) + label.encode().ljust(16, b"\0") + b"\0" * 4


def table():
    raw = (entry("nvs", 1, 2, 0x9000, 0x5000) + entry("otadata", 1, 0, 0xE000, 0x2000)
           + entry("app0", 0, 0x10, 0x10000, 0x600000) + entry("app1", 0, 0x11, 0x610000, 0x600000))
    return raw + b"\xEB\xEB" + b"\xFF" * 30 + b"\xFF" * 64


def program(name="board_check", version="1.2.3"):
    raw = bytearray(b"\xE9" + b"\0" * 0xFF)
    raw[0x20:0x24] = d.APP_DESC_MAGIC
    raw[0x30:0x30 + len(version)] = version.encode()
    raw[0x50:0x50 + len(name)] = name.encode()
    return bytes(raw)


BOOT = b"\xE9" + b"\0" * 15
BLANK = b"\xFF" * 16


class Reading(unittest.TestCase):
    def test_table_is_read_until_its_end(self):
        parts = d.parse_table(table())
        self.assertEqual([p["label"] for p in parts], ["nvs", "otadata", "app0", "app1"])
        self.assertEqual([p["offset"] for p in d.apps_in(parts)], [0x10000, 0x610000])
        self.assertEqual(d.otadata_in(parts)["offset"], 0xE000)

    def test_blank_or_short_flash_is_not_a_table(self):
        self.assertIsNone(d.parse_table(b"\xFF" * 0xC00))
        self.assertIsNone(d.parse_table(b""))
        self.assertIsNone(d.parse_table(None))

    def test_a_program_says_its_name_and_version(self):
        said = d.describe_app(program())
        self.assertEqual((said["name"], said["version"]), ("board_check", "1.2.3"))
        self.assertIsNone(d.describe_app(b"\xFF" * 0x100))
        self.assertEqual(d.describe_app(b"\xE9" + b"\0" * 8)["name"], "")    # A program that says nothing is still one.


class Concluding(unittest.TestCase):
    def test_each_finding(self):
        good = (True, BLANK, BOOT, table(), [program(), BLANK])
        self.assertEqual(d.diagnose(*good), d.HEALTHY)
        self.assertEqual(d.diagnose(False, None, None, None, []), d.NO_ANSWER)
        self.assertEqual(d.diagnose(True, BLANK, BLANK, table(), [program()]), d.NO_BOOTLOADER)
        self.assertEqual(d.diagnose(True, BOOT, BLANK, table(), [program()]), d.BOOTLOADER_AT_ZERO)
        self.assertEqual(d.diagnose(True, BLANK, BOOT, b"\xFF" * 0xC00, []), d.NO_TABLE)
        self.assertEqual(d.diagnose(True, BLANK, BOOT, table(), [BLANK, BLANK]), d.NO_APP)
        self.assertEqual(d.diagnose(True, BLANK, None, table(), [program()]), d.NO_BOOTLOADER)   # A read that failed.

    def test_restarting_only_counts_when_listened_to(self):
        good = (True, BLANK, BOOT, table(), [program()])
        self.assertEqual(d.diagnose(*good, restarts=5), d.RESTARTING)
        self.assertEqual(d.diagnose(*good, restarts=1), d.HEALTHY)        # One restart is the doctor's own.
        self.assertEqual(d.diagnose(*good, restarts=None), d.HEALTHY)

    def test_every_finding_has_advice(self):
        for finding in (d.NO_ANSWER, d.NO_BOOTLOADER, d.BOOTLOADER_AT_ZERO, d.NO_TABLE, d.NO_APP, d.RESTARTING, d.HEALTHY):
            self.assertTrue(d.ADVICE[finding])


class Packs(unittest.TestCase):
    def build_dir(self, folder):
        for name, data in (("bootloader.bin", BOOT * 64), ("partitions.bin", table()), ("firmware.bin", program() * 8)):
            with open(os.path.join(folder, name), "wb") as out:
                out.write(data)

    def test_pack_there_and_back_and_the_plan(self):
        with tempfile.TemporaryDirectory() as folder:
            self.build_dir(folder)
            out = os.path.join(folder, "cure.zip")
            with mock.patch("builtins.print"):
                d.cmd_pack(folder, "ws-c5-28", out)
            manifest, parts = d.open_pack(out)
            self.assertEqual((manifest["board"], manifest["program"], manifest["chip"]), ("ws-c5-28", "board_check", "esp32c5"))
            plan = d.cure_plan(parts)
            self.assertEqual([address for address, _ in plan], [0x2000, 0x8000, 0xE000, 0x10000])
            self.assertEqual(plan[2][1], b"\xFF" * 0x2000)                  # The start-up choice is blanked.
            self.assertEqual(plan[3][1], parts["firmware.bin"])

    def test_an_arduino_build_is_named_after_its_folder(self):
        with tempfile.TemporaryDirectory() as top:
            folder = os.path.join(top, "board_check_28"); os.mkdir(folder)
            for name, data in (("bootloader.bin", BOOT * 64), ("partitions.bin", table()), ("firmware.bin", program(d.GENERIC_NAME, "abc123"))):
                with open(os.path.join(folder, name), "wb") as out:
                    out.write(data)
            out = os.path.join(top, "cure.zip")
            with mock.patch("builtins.print"):
                d.cmd_pack(folder, "ws-c5-28", out)
            manifest, _ = d.open_pack(out)
            self.assertEqual((manifest["program"], manifest["version"]), ("board_check_28", ""))

    def test_a_damaged_pack_is_refused(self):
        import json, zipfile
        with tempfile.TemporaryDirectory() as folder:
            self.build_dir(folder)
            out = os.path.join(folder, "cure.zip")
            with mock.patch("builtins.print"):
                d.cmd_pack(folder, "ws-c5-28", out)
            manifest, parts = d.open_pack(out)
            bad = os.path.join(folder, "bad.zip")
            with zipfile.ZipFile(bad, "w") as pack:
                pack.writestr("manifest.json", json.dumps(manifest))
                for name, data in parts.items():
                    pack.writestr(name, data + (b"x" if name == "firmware.bin" else b""))
            with self.assertRaises(SystemExit):
                d.open_pack(bad)

    def test_a_folder_that_is_not_a_build_is_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            for name in d.PACK_FILES:
                with open(os.path.join(folder, name), "wb") as out:
                    out.write(b"\xFF" * 64)
            with self.assertRaises(SystemExit):
                d.cmd_pack(folder, "ws-c5-28", os.path.join(folder, "x.zip"))


class Running(unittest.TestCase):
    def test_no_port_is_said_plainly_and_nothing_is_called(self):
        with mock.patch.object(d.glob, "glob", return_value=[]), mock.patch.object(d.c5flash, "et") as et, \
             mock.patch("builtins.print") as said:
            self.assertEqual(d.main(["check"]), 2)
        et.assert_not_called()
        self.assertIn("No USB serial port", said.call_args[0][0])

    def test_cure_refused_writes_nothing(self):
        with tempfile.TemporaryDirectory() as folder:
            Packs.build_dir(self, folder)
            out = os.path.join(folder, "cure.zip")
            with mock.patch("builtins.print"):
                d.cmd_pack(folder, "ws-c5-28", out)
            with mock.patch.object(d, "chip_facts", return_value=(True, {"chip": "ESP32-C5"})), \
                 mock.patch("builtins.input", return_value="no"), mock.patch("builtins.print"), \
                 mock.patch.object(d.c5flash, "cmd_write") as write, mock.patch.object(d.c5flash, "read_piece") as read:
                self.assertEqual(d.cmd_cure(out, yes=False), 1)
            write.assert_not_called()
            read.assert_not_called()

    def test_cure_stops_if_the_board_cannot_be_saved_first(self):
        with tempfile.TemporaryDirectory() as folder:
            Packs.build_dir(self, folder)
            out = os.path.join(folder, "cure.zip")
            with mock.patch("builtins.print"):
                d.cmd_pack(folder, "ws-c5-28", out)
            with mock.patch.object(d, "chip_facts", return_value=(True, {"chip": "ESP32-C5"})), mock.patch("builtins.print"), \
                 mock.patch.object(d.c5flash, "cmd_write") as write, mock.patch.object(d.c5flash, "read_piece", return_value=None):
                with self.assertRaises(SystemExit):
                    d.cmd_cure(out, yes=True)
            write.assert_not_called()

    def test_another_chip_is_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            Packs.build_dir(self, folder)
            out = os.path.join(folder, "cure.zip")
            with mock.patch("builtins.print"):
                d.cmd_pack(folder, "ws-c5-28", out)
            with mock.patch.object(d, "chip_facts", return_value=(True, {"chip": "ESP32-S3"})), mock.patch("builtins.print"), \
                 mock.patch.object(d.c5flash, "cmd_write") as write:
                with self.assertRaises(SystemExit):
                    d.cmd_cure(out, yes=True)
            write.assert_not_called()


if __name__ == "__main__":
    unittest.main()
