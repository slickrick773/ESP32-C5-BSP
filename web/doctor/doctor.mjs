// SPDX-License-Identifier: MIT
// What the board doctor concludes from what it reads. No browser and no board
// here, so it runs under `node --test`. It must agree with
// tools/c5doctor/c5doctor.py: same findings, same words.

export const BOOTLOADER_AT = 0x2000;   // On the C5 the ROM loads the bootloader from here, not from 0.
export const TABLE_AT = 0x8000;
export const TABLE_SIZE = 0xC00;
export const IMAGE_MAGIC = 0xE9;
export const SAVE_SIZE = 0x10000;
const GENERIC_NAME = 'arduino-lib-builder';   // What every Arduino build calls itself: it tells nothing.

export const NO_ANSWER = 'no_answer', NO_BOOTLOADER = 'no_bootloader', BOOTLOADER_AT_ZERO = 'bootloader_at_zero',
  NO_TABLE = 'no_table', NO_APP = 'no_app', RESTARTING = 'restarting', HEALTHY = 'healthy';

export const ADVICE = {
  [NO_ANSWER]: 'The chip did not answer. Try: a different USB cable (many carry power only), a port on the ' +
    'computer itself, then download mode by hand: hold BOOT, tap RST, release BOOT, and connect again. ' +
    'If it never answers in download mode, no software can fix it.',
  [NO_BOOTLOADER]: 'Nothing the chip can start from is stored at 0x2000. The screen stays dark and the chip ' +
    'restarts over and over. The cure below fixes this.',
  [BOOTLOADER_AT_ZERO]: 'A bootloader is stored at 0x0, where other ESP32 chips keep it. The C5 looks at ' +
    '0x2000, finds none, and restarts over and over. This is the usual way a C5 gets "bricked" by a ' +
    'flashing tool set up for another chip. The cure below fixes this.',
  [NO_TABLE]: 'The bootloader is there but the partition table is missing or damaged, so it cannot find a ' +
    'program to run. The cure below fixes this.',
  [NO_APP]: 'The bootloader and partition table are there, but no program is stored where they point. ' +
    'The cure below fixes this.',
  [RESTARTING]: 'Everything stored looks right, but the board keeps restarting. The program on it is ' +
    'crashing at start-up. The cure below replaces it with a known-good test image.',
  [HEALTHY]: 'The chip answers, and a bootloader, a partition table and a program are all in place. This ' +
    'board is not bricked.',
};

function text(raw, at, n) {
  let out = '';
  for (let i = at; i < at + n && i < raw.length && raw[i] !== 0; i++) out += raw[i] < 0x80 ? String.fromCharCode(raw[i]) : '?';
  return out;
}

// The partitions in a table, or null if this is not a table.
export function parseTable(raw) {
  if (!raw || raw.length < 32 || raw[0] !== 0xAA || raw[1] !== 0x50) return null;
  const view = new DataView(raw.buffer, raw.byteOffset, raw.byteLength), found = [];
  for (let at = 0; at + 32 <= raw.length; at += 32) {
    if (raw[at] !== 0xAA || raw[at + 1] !== 0x50) break;   // The checksum entry or blank flash: the end.
    found.push({ label: text(raw, at + 12, 16), type: raw[at + 2], subtype: raw[at + 3],
      offset: view.getUint32(at + 4, true), size: view.getUint32(at + 8, true) });
  }
  return found.length ? found : null;
}

export const appsIn = table => (table || []).filter(p => p.type === 0);

// What a program says about itself, or null if these bytes are not a program.
export function describeApp(raw) {
  if (!raw || !raw.length || raw[0] !== IMAGE_MAGIC) return null;
  if (raw.length >= 0x90 && raw[0x20] === 0x32 && raw[0x21] === 0x54 && raw[0x22] === 0xCD && raw[0x23] === 0xAB)
    return { version: text(raw, 0x30, 32), name: text(raw, 0x50, 32), built: (text(raw, 0x80, 16) + ' ' + text(raw, 0x70, 16)).trim() };
  return { name: '', version: '', built: '' };
}

// One conclusion from what was read. `appHeads` is the first bytes of each
// program partition, in table order. `restarts` is how many times the board was
// seen to restart while listened to, or null if it was not listened to.
export function diagnose(answers, atZero, atBootloader, tableRaw, appHeads, restarts = null) {
  if (!answers) return NO_ANSWER;
  if (!atBootloader || atBootloader[0] !== IMAGE_MAGIC)
    return atZero && atZero[0] === IMAGE_MAGIC ? BOOTLOADER_AT_ZERO : NO_BOOTLOADER;
  const table = parseTable(tableRaw);
  if (!table || !appsIn(table).length) return NO_TABLE;
  if (!appHeads.some(head => describeApp(head) !== null)) return NO_APP;
  if (restarts !== null && restarts >= 3) return RESTARTING;
  return HEALTHY;
}

// The lines shown for what was read, in the Python doctor's order.
export function reportLines(facts, atBootloader, table, appHeads) {
  const lines = [`chip:        ${facts.chip}`, `flash:       ${facts.flash}`];
  if (facts.address) lines.push(`address:     ends ${facts.address.slice(-8)}`);
  lines.push('bootloader:  ' + (atBootloader && atBootloader[0] === IMAGE_MAGIC ? 'present at 0x2000' : 'MISSING at 0x2000'));
  lines.push('partitions:  ' + (table ? `${table.length} entries` : 'MISSING or damaged'));
  appsIn(table).forEach((part, i) => {
    const said = describeApp(appHeads[i]), where = `'${part.label}' (0x${part.offset.toString(16)})`;
    if (!said) { lines.push(`program:     none in ${where}`); return; }
    const name = said.name && said.name !== GENERIC_NAME ? `${said.name} ${said.version}`.trim() : 'an Arduino program (it does not name itself)';
    lines.push(`program:     ${where}: ${name}${said.built ? ', built ' + said.built : ''}`);
  });
  return lines;
}

// How many times the chip's ROM announced a start in this serial text.
export const countRestarts = heard => (heard.match(/ESP-ROM:/g) || []).length;

// The board check's own lines, if that is what is running.
export const boardCheckLines = heard => heard.split(/\r?\n/).filter(line => line.includes('board_check')).map(line => line.trim());

// A cure plan from the site, checked before anything is trusted: four pieces at
// the addresses a C5 with this layout uses, each named with its size and hashes.
export function validatePlan(plan) {
  const bad = why => { throw Error('This cure is not usable: ' + why); };
  if (!plan || plan.chip !== 'esp32c5' || !Array.isArray(plan.parts) || plan.parts.length !== 4) bad('wrong shape');
  const want = [['bootloader.bin', BOOTLOADER_AT, 0x6000], ['partitions.bin', TABLE_AT, 0x1000], ['otadata.bin', null, 0x2000], ['firmware.bin', null, null]];
  plan.parts.forEach((p, i) => {
    if (p.file !== want[i][0]) bad('unexpected file ' + p.file);
    if (!Number.isInteger(p.offset) || !Number.isInteger(p.bytes) || p.bytes <= 0 || p.bytes % 4) bad('bad size for ' + p.file);
    if (want[i][1] !== null && p.offset !== want[i][1]) bad(p.file + ' is aimed at the wrong address');
    if (want[i][2] !== null && p.bytes > want[i][2]) bad(p.file + ' is too large');
    if (!/^[a-f0-9]{64}$/.test(p.sha256) || !/^[a-f0-9]{32}$/.test(p.md5)) bad('bad checksum for ' + p.file);
  });
  const sorted = [...plan.parts].sort((a, b) => a.offset - b.offset);
  for (let i = 1; i < sorted.length; i++)
    if (sorted[i - 1].offset + sorted[i - 1].bytes > sorted[i].offset) bad('pieces overlap');
  if (sorted[0].offset < BOOTLOADER_AT) bad('a piece is aimed below the bootloader');
  return plan;
}
