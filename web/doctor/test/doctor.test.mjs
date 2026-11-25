// SPDX-License-Identifier: MIT
// Host tests for the web doctor's conclusions. Run: node --test web/doctor/test/
import test from 'node:test';
import assert from 'node:assert/strict';
import * as D from '../doctor.mjs';

function entry(label, type, subtype, offset, size) {
  const raw = new Uint8Array(32), view = new DataView(raw.buffer);
  raw[0] = 0xAA; raw[1] = 0x50; raw[2] = type; raw[3] = subtype;
  view.setUint32(4, offset, true); view.setUint32(8, size, true);
  raw.set(new TextEncoder().encode(label), 12);
  return raw;
}
function join(...pieces) {
  const out = new Uint8Array(pieces.reduce((n, p) => n + p.length, 0));
  let at = 0;
  for (const p of pieces) { out.set(p, at); at += p.length; }
  return out;
}
const blank = n => new Uint8Array(n).fill(0xFF);
const table = () => join(entry('nvs', 1, 2, 0x9000, 0x5000), entry('otadata', 1, 0, 0xE000, 0x2000),
  entry('app0', 0, 0x10, 0x10000, 0x600000), entry('app1', 0, 0x11, 0x610000, 0x600000), Uint8Array.of(0xEB, 0xEB), blank(94));
function program(name = 'board_check', version = '1.2.3') {
  const raw = new Uint8Array(0x100); raw[0] = 0xE9;
  raw.set([0x32, 0x54, 0xCD, 0xAB], 0x20);
  raw.set(new TextEncoder().encode(version), 0x30);
  raw.set(new TextEncoder().encode(name), 0x50);
  return raw;
}
const BOOT = join(Uint8Array.of(0xE9), new Uint8Array(15)), BLANK = blank(16);

test('a table is read until its end', () => {
  const parts = D.parseTable(table());
  assert.deepEqual(parts.map(p => p.label), ['nvs', 'otadata', 'app0', 'app1']);
  assert.deepEqual(D.appsIn(parts).map(p => p.offset), [0x10000, 0x610000]);
});

test('blank or short flash is not a table', () => {
  assert.equal(D.parseTable(blank(0xC00)), null);
  assert.equal(D.parseTable(new Uint8Array(0)), null);
  assert.equal(D.parseTable(null), null);
});

test('a program says its name and version', () => {
  const said = D.describeApp(program());
  assert.deepEqual([said.name, said.version], ['board_check', '1.2.3']);
  assert.equal(D.describeApp(blank(0x100)), null);
  assert.equal(D.describeApp(join(Uint8Array.of(0xE9), new Uint8Array(8))).name, '');
});

test('each finding, as the Python doctor concludes it', () => {
  assert.equal(D.diagnose(true, BLANK, BOOT, table(), [program(), BLANK]), D.HEALTHY);
  assert.equal(D.diagnose(false, null, null, null, []), D.NO_ANSWER);
  assert.equal(D.diagnose(true, BLANK, BLANK, table(), [program()]), D.NO_BOOTLOADER);
  assert.equal(D.diagnose(true, BOOT, BLANK, table(), [program()]), D.BOOTLOADER_AT_ZERO);
  assert.equal(D.diagnose(true, BLANK, BOOT, blank(0xC00), []), D.NO_TABLE);
  assert.equal(D.diagnose(true, BLANK, BOOT, table(), [BLANK, BLANK]), D.NO_APP);
  assert.equal(D.diagnose(true, BLANK, null, table(), [program()]), D.NO_BOOTLOADER);
  assert.equal(D.diagnose(true, BLANK, BOOT, table(), [program()], 5), D.RESTARTING);
  assert.equal(D.diagnose(true, BLANK, BOOT, table(), [program()], 1), D.HEALTHY);
  assert.equal(D.diagnose(true, BLANK, BOOT, table(), [program()], 0, 0.14), D.OFF_USB);
  assert.equal(D.diagnose(true, BLANK, BOOT, table(), [program()], 0, 1), D.HEALTHY);
  assert.equal(D.diagnose(true, BLANK, BOOT, table(), [program()], 9, 0.14), D.RESTARTING);
});

test('every finding has advice', () => {
  for (const finding of [D.NO_ANSWER, D.NO_BOOTLOADER, D.BOOTLOADER_AT_ZERO, D.NO_TABLE, D.NO_APP, D.RESTARTING, D.OFF_USB, D.HEALTHY])
    assert.ok(D.ADVICE[finding]);
});

test('the report names what is there and what is not', () => {
  const lines = D.reportLines({ chip: 'ESP32-C5', flash: '32MB', address: 'aa:bb:cc:dd:ee:ff' }, BOOT, D.parseTable(table()),
    [program('arduino-lib-builder', 'x'), BLANK]);
  assert.ok(lines.includes('address:     ends dd:ee:ff'));
  assert.ok(lines.includes('bootloader:  present at 0x2000'));
  assert.ok(lines.some(l => l.includes("'app0' (0x10000): an Arduino program")));
  assert.ok(lines.includes("program:     none in 'app1' (0x610000)"));
});

test('what was heard: restarts and the board check table', () => {
  const heard = 'ESP-ROM:esp32c5\r\njunk\r\n[board_check_28]  PASS  IMU   1 g\r\n[board_check_28] result: PASS\r\n';
  assert.equal(D.countRestarts(heard + heard + heard), 3);
  assert.deepEqual(D.boardCheckLines('E (1082) MSPI Timing: no cacheline for PSR[board_check_28] Waveshare\n'), ['[board_check_28] Waveshare']);
  assert.deepEqual(D.boardCheckLines(heard), ['[board_check_28]  PASS  IMU   1 g', '[board_check_28] result: PASS']);
});

const sha = 'a'.repeat(64), md5 = 'b'.repeat(32);
const plan = () => ({ chip: 'esp32c5', board: 'ws-c5-28', parts: [
  { file: 'bootloader.bin', offset: 0x2000, bytes: 20752, sha256: sha, md5 },
  { file: 'partitions.bin', offset: 0x8000, bytes: 3072, sha256: sha, md5 },
  { file: 'otadata.bin', offset: 0xE000, bytes: 0x2000, sha256: sha, md5 },
  { file: 'firmware.bin', offset: 0x10000, bytes: 511888, sha256: sha, md5 }] });

test('a good plan is accepted and a bad one refused', () => {
  assert.equal(D.validatePlan(plan()).parts.length, 4);
  const cases = [p => { p.chip = 'esp32s3'; }, p => { p.parts[0].offset = 0; }, p => { p.parts[1].offset = 0x9000; },
    p => { p.parts[3].offset = 0xF000; }, p => { p.parts[0].bytes = 0x7000; }, p => { p.parts[3].md5 = 'x'; },
    p => { p.parts.pop(); }, p => { p.parts[2].file = 'nvs.bin'; }, p => { p.parts[3].bytes = 511889; }];
  for (const spoil of cases) { const bad = plan(); spoil(bad); assert.throws(() => D.validatePlan(bad)); }
});

// A pretend flash where reading the block at one address stalls the chip: every
// read then fails until revive() is called, as measured on a 3.5-inch board.
function pretendFlash(stallAt) {
  const flash = Uint8Array.from({ length: 0x400 }, (_, i) => (i * 7 + 3) & 0xFF);
  const state = { stuck: false, revived: 0 };
  const readBlock = async at => {
    if (state.stuck) throw Error('no answer');
    if (at === stallAt) { state.stuck = true; throw Error('stalled'); }
    return flash.slice(at, at + 64);
  };
  const revive = async () => { state.stuck = false; state.revived++; };
  return { flash, state, readBlock, revive };
}

test('reading goes around a block that stalls the chip', async () => {
  const { flash, state, readBlock, revive } = pretendFlash(0x180);
  const got = await D.readInBlocks(readBlock, revive, 0x100, 0x200);
  assert.deepEqual(got, flash.slice(0x100, 0x300));
  assert.equal(state.revived, 1);
});

test('reading an odd range, and nothing stalls', async () => {
  const { flash, state, readBlock, revive } = pretendFlash(-1);
  assert.deepEqual(await D.readInBlocks(readBlock, revive, 0x47, 0x91), flash.slice(0x47, 0xD8));
  assert.equal(state.revived, 0);
});

test('a first block that stalls cannot be read, and says so', async () => {
  const { readBlock, revive } = pretendFlash(0);
  assert.equal(await D.readInBlocks(readBlock, revive, 0, 0x100), null);
});
