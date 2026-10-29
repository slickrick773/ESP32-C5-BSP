#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Host-only regression tests for c5flash. No hardware, no real esptool: the
serial-port enumeration and the esptool transport are mocked.

Run:  python3 tools/c5flash/test/test_c5flash.py
"""
import os, sys, tempfile, types, unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import c5flash


def et_stalls_above(limit=0x1000):
    """Fake c5flash.et modelling the real C5 bus: a read-flash of <= limit bytes
    succeeds and writes exactly that many bytes; a larger read 'stalls' (fails,
    transfers nothing). This forces read_piece onto its 4 KB fallback path."""
    def _et(*args, **kw):
        if args and args[0] == "read-flash":
            size, path = int(args[2]), args[3]
            if size <= limit:
                with open(path, "wb") as f:
                    f.write(b"\xA5" * size)
                return True, ""
            open(path, "wb").close()          # stalled: nothing transferred
            return False, "stall"
        return True, ""
    return _et


class TestRead(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def test_partial_final_chunk_exact(self):
        # 0x1800 (6144) is not a multiple of the 0x1000 fallback chunk. The
        # fallback must return EXACTLY 0x1800 bytes -- the old code returned
        # 0x2000 (8192) because the final piece was never clamped.
        with mock.patch.object(c5flash, "et", et_stalls_above()):
            data = c5flash.read_piece(0, 0x1800)
        self.assertIsNotNone(data)
        self.assertEqual(len(data), 0x1800,
                         f"expected 6144 bytes, got {len(data)} (over-read)")

    def test_cmd_read_unaligned_end_exact(self):
        # End to end: a read whose length is not 64 KB-aligned writes exactly the
        # requested byte count to the output file.
        out = os.path.join(self.tmp, "out.bin")
        with mock.patch.object(c5flash, "et", et_stalls_above()):
            c5flash.cmd_read("0x0", "0x1800", out)
        self.assertEqual(os.path.getsize(out), 0x1800)

    def test_short_response_rejected(self):
        # et "succeeds" but returns one byte short -> never accepted; the read
        # fails (None) rather than returning padded/truncated data.
        def short_et(*args, **kw):
            if args and args[0] == "read-flash":
                size, path = int(args[2]), args[3]
                with open(path, "wb") as f:
                    f.write(b"\x00" * max(0, size - 1))
                return True, ""
            return True, ""
        with mock.patch.object(c5flash, "et", short_et):
            self.assertIsNone(c5flash.read_piece(0, 0x800))

    def test_failed_response_exits(self):
        # Every read fails -> cmd_read exits non-zero, writes no file.
        def fail_et(*args, **kw):
            if args and args[0] == "read-flash":
                open(args[3], "wb").close()
                return False, "fail"
            return True, ""
        out = os.path.join(self.tmp, "x.bin")
        with mock.patch.object(c5flash, "et", fail_et):
            with self.assertRaises(SystemExit):
                c5flash.cmd_read("0x0", "0x800", out)
        self.assertFalse(os.path.exists(out))


class TestPort(unittest.TestCase):
    def tearDown(self):
        c5flash.PORT = None

    def test_explicit_port_wins(self):
        self.assertEqual(c5flash.resolve_port("/dev/ttyUSB9"), "/dev/ttyUSB9")

    def test_single_candidate(self):
        with mock.patch("glob.glob", side_effect=[["/dev/cu.usbmodemAAA"], []]):
            self.assertEqual(c5flash.resolve_port(None), "/dev/cu.usbmodemAAA")

    def test_no_candidate_exits(self):
        with mock.patch("glob.glob", return_value=[]):
            with self.assertRaises(SystemExit):
                c5flash.resolve_port(None)

    def test_ambiguous_rejected(self):
        with mock.patch("glob.glob", side_effect=[["/dev/cu.usbmodemAAA"], ["/dev/ttyACM0"]]):
            with self.assertRaises(SystemExit) as cm:
                c5flash.resolve_port(None)
        self.assertIn("--port", str(cm.exception))

    def test_port_retained_across_calls(self):
        # Resolve once to AAA, then let the device re-enumerate to BBB. Every
        # esptool invocation must still target AAA (resolved once, reused).
        with mock.patch("glob.glob", side_effect=[["/dev/cu.usbmodemAAA"], []]):
            c5flash.PORT = c5flash.resolve_port(None)
        calls = []
        def fake_run(cmd, **kw):
            calls.append(cmd)
            return types.SimpleNamespace(returncode=0, stdout="", stderr="")
        with mock.patch.object(c5flash, "ESPTOOL", "esptool"), \
             mock.patch("glob.glob", return_value=["/dev/cu.usbmodemBBB"]), \
             mock.patch.object(c5flash.subprocess, "run", fake_run):
            c5flash.et("chip-id")
            c5flash.et("read-flash", "0x0", "16", os.path.join(tempfile.gettempdir(), "ignored"))
        self.assertEqual(len(calls), 2)
        for cmd in calls:
            self.assertIn("/dev/cu.usbmodemAAA", cmd)
            self.assertNotIn("/dev/cu.usbmodemBBB", cmd)


if __name__ == "__main__":
    unittest.main(verbosity=2)
