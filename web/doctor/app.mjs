// SPDX-License-Identifier: MIT
// The page: connect, check, and cure. Everything happens between this browser
// and the board; nothing read from the board is sent anywhere.
import { Transport } from './vendor/esptool-js-0.7.0.js';
import { C5Rom, reach, writeAndVerify } from './rom.mjs';
import * as D from './doctor.mjs';

const $ = id => document.getElementById(id);
const pause = ms => new Promise(done => setTimeout(done, ms));
let port = null, transport = null, loader = null, facts = null, busy = false, remembered = false;
// Whether a board was ever picked on this page: the browser then lets the page see it without the list.
try { remembered = localStorage.getItem('c5doctor-picked') === '1'; } catch { /* storage is off: only this visit counts */ }

function say(text) { $('status').textContent = text; }
function show(id, text) { $(id).textContent = text; $(id).hidden = !text; }
function log(text) { $('log').textContent = ($('log').textContent + text + '\n').slice(-20000); }
function bar(done, total) { $('progress').value = total ? Math.round(100 * done / total) : 0; }

function controls() {
  const able = 'serial' in navigator && window.isSecureContext;
  $('connect').disabled = !able || busy;
  $('cure').disabled = !able || busy || !loader || !$('board').value || !$('agree').checked;
  $('board').disabled = $('agree').disabled = busy;
  $('listen').disabled = !able || busy || !port;
}

async function letGo() {
  if (transport) { try { await transport.disconnect(); } catch { /* already gone */ } }
  transport = loader = null;
}

async function examine() {
  const atZero = await loader.readFlashSlow(0, 64);
  const atBootloader = await loader.readFlashSlow(D.BOOTLOADER_AT, 64);
  const tableRaw = await loader.readFlashSlow(D.TABLE_AT, D.TABLE_SIZE, bar);
  const table = D.parseTable(tableRaw), heads = [];
  for (const part of D.appsIn(table)) heads.push(await loader.readFlashSlow(part.offset, 0x100));
  return { finding: D.diagnose(true, atZero, atBootloader, tableRaw, heads), lines: D.reportLines(facts, atBootloader, table, heads) };
}

function conclude(finding, lines, heading) {
  show('report', lines.join('\n'));
  $('finding').className = finding === D.HEALTHY ? 'good' : 'bad';
  show('finding', `${heading}: ${finding.replaceAll('_', ' ')}`);
  show('advice', D.ADVICE[finding]);
}

const ESPRESSIF = 0x303A;   // The USB maker number of the chip's own USB port.
const granted = async () => (await navigator.serial.getPorts()).filter(p => p.getInfo().usbVendorId === ESPRESSIF);

// Reach the chip, trying again for a while. A board that restarts over and over
// is only on USB for a moment at a time: each time it is there, grab it and ask
// the chip to stop in its loader. `first` is the port the user just picked, if
// any; after that, any board this page was allowed to use before will do.
async function catchChip(first, seconds = 30) {
  const until = Date.now() + seconds * 1000;
  let candidate = first, problem = Error('no board appeared');
  while (Date.now() < until) {
    if (!candidate) { candidate = (await granted())[0]; if (!candidate) { await pause(50); continue; } }
    try {
      port = candidate;
      transport = new Transport(port);
      loader = new C5Rom({ transport, baudrate: 115200, romBaudrate: 115200, terminal: { clean() {}, write: log, writeLine: log } });
      facts = await reach(loader, 2);
      return;
    } catch (why) {
      problem = why;
      log(String(why?.message || why));
      await letGo();
      if (/not an ESP32-C5|locked/.test(why?.message || '')) throw why;
      candidate = null;
      await pause(100);
      say('The board comes and goes. Still trying to catch it…');
    }
  }
  throw problem;
}

$('connect').onclick = async () => {
  busy = true; controls(); $('log').textContent = ''; show('report', ''); show('finding', ''); show('advice', ''); show('heard', ''); bar(0, 1);
  try {
    await letGo();
    say('Choose the board in the list the browser shows. It is called "USB JTAG/serial debug unit". If it flickers in and out of the list, click it the moment it shows: once is enough.');
    let picked = null;
    try {
      picked = await navigator.serial.requestPort({ filters: [{ usbVendorId: ESPRESSIF }] });
    } catch (problem) {
      if (problem?.name !== 'NotFoundError') throw problem;
      // Nothing picked. A board picked on an earlier try is remembered, so wait for it.
      if (!remembered && !(await granted()).length) {
        say('No board was chosen. If it flickers in and out of the list it is restarting over and over: see "If the board flickers" below.');
        return;
      }
    }
    if (picked) { remembered = true; try { localStorage.setItem('c5doctor-picked', '1'); } catch { /* storage is off */ } }
    say(picked ? 'Asking the chip…' : 'Waiting for the board you chose before to show up…');
    try {
      await catchChip(picked);
    } catch (problem) {
      log(String(problem?.stack || problem));
      await letGo();
      if (/not an ESP32-C5|locked/.test(problem.message)) { say(problem.message); return; }
      conclude(D.NO_ANSWER, [], 'Finding');
      say('The chip did not answer.');
      return;
    }
    say('Reading what is stored on the board…');
    const { finding, lines } = await examine();
    conclude(finding, lines, 'Finding');
    say(finding === D.HEALTHY ? 'Checked. Nothing stored is wrong. If the screen is still dark, the cure below puts a test image on it.'
                              : 'Checked. Choose your board below to cure it.');
  } catch (problem) {
    await letGo();
    say(`Stopped: ${problem.message}`);
    log(String(problem?.stack || problem));
  } finally { busy = false; controls(); }
};

async function fetchPlan(board) {
  const base = new URL(`packs/${board}/`, location.href);
  const answer = await fetch(new URL('plan.json', base));
  if (!answer.ok) throw Error('The cure for that board could not be downloaded.');
  const plan = D.validatePlan(await answer.json());
  if (plan.board !== board) throw Error('The cure downloaded is for another board.');
  for (const p of plan.parts) {
    const got = await fetch(new URL(p.file, base));
    if (!got.ok) throw Error(`${p.file} could not be downloaded.`);
    p.data = new Uint8Array(await got.arrayBuffer());
    const digest = new Uint8Array(await crypto.subtle.digest('SHA-256', p.data));
    const hex = Array.from(digest, b => b.toString(16).padStart(2, '0')).join('');
    if (p.data.length !== p.bytes || hex !== p.sha256) throw Error(`${p.file} arrived damaged. Nothing was written.`);
  }
  return plan;
}

function saveFile(name, bytes) {
  const link = document.createElement('a');
  link.href = URL.createObjectURL(new Blob([bytes], { type: 'application/octet-stream' }));
  link.download = name;
  link.click();
  setTimeout(() => URL.revokeObjectURL(link.href), 60000);
}

$('cure').onclick = async () => {
  busy = true; controls(); show('heard', '');
  try {
    say('Downloading the cure and checking it…');
    const plan = await fetchPlan($('board').value);
    say('Saving the board\'s first 64 KB as it is now…');
    const before = await loader.readFlashSlow(0, D.SAVE_SIZE, bar);
    if (!before) throw Error('The board\'s first 64 KB could not be read to save it. Nothing was written.');
    saveFile(`${plan.board}-before-${new Date().toISOString().replace(/[-:]/g, '').slice(0, 15)}.bin`, before);
    say('Writing. Keep the board connected.');
    await writeAndVerify(loader, plan.parts, (index, written, total) => {
      bar(index + written / total, plan.parts.length);
      say(`Writing piece ${index + 1} of ${plan.parts.length}: ${Math.floor(100 * written / total)} %`);
    });
    bar(1, 1);
    say('Written and read back correctly. Checking again…');
    const { finding, lines } = await examine();
    conclude(finding, lines, 'Finding after the cure');
    await letGo();
    say('Cured. Now press the RST button on the board once (do not hold BOOT). Its screen should light up with a PASS/FAIL table after a few seconds. '
      + 'Then press "Listen to the board" to see the same table here.');
  } catch (problem) {
    say(`The cure stopped: ${problem.message}`);
    log(String(problem?.stack || problem));
  } finally { busy = false; controls(); }
};

// Open the board's serial port as plain text and collect what it prints. The
// port goes away for a moment each time the chip restarts, so keep reopening.
async function hear(seconds) {
  const until = Date.now() + seconds * 1000, decoder = new TextDecoder();
  let heard = '';
  while (Date.now() < until) {
    let reader = null;
    try {
      const known = await navigator.serial.getPorts();
      if (!known.includes(port)) {
        const same = port.getInfo();
        port = known.find(p => p.getInfo().usbVendorId === same.usbVendorId && p.getInfo().usbProductId === same.usbProductId) || port;
      }
      await port.open({ baudRate: 115200 });
      // Without these two lines raised the board's program keeps its text to itself
      // (measured: 0 of 14 board check lines arrive without them, 14 of 14 with).
      await port.setSignals({ dataTerminalReady: true, requestToSend: true });
      reader = port.readable.getReader();
      const mine = reader;
      pause(until - Date.now()).then(() => mine.cancel().catch(() => {}));   // Stop waiting when the time is up.
      while (Date.now() < until) {
        const { value, done } = await reader.read();
        if (done) break;
        heard += decoder.decode(value, { stream: true });
        show('heard', D.boardCheckLines(heard).join('\n') || 'Listening…');
      }
    } catch { await pause(300); }
    finally {
      try { reader?.releaseLock(); } catch { /* lost with the port */ }
      try { await port.close(); } catch { /* already closed */ }
    }
  }
  return heard;
}

$('listen').onclick = async () => {
  busy = true; controls(); bar(0, 1);
  try {
    await letGo();
    say('Listening for 20 seconds. Press RST on the board now if you have not yet.');
    show('heard', 'Listening…');
    const heard = await hear(20), table = D.boardCheckLines(heard), restarts = D.countRestarts(heard);
    if (restarts >= 3) {
      $('finding').className = 'bad';
      show('finding', `Finding: restarting (${restarts} times in 20 seconds)`);
      show('advice', D.ADVICE[D.RESTARTING]);
      say('The board keeps restarting.');
    } else if (table.length) {
      show('heard', table.join('\n'));
      say(table.some(line => /result: PASS/.test(line)) ? 'The board check ran and every part passed.'
        : 'The board check ran. A FAIL line is about that part of the board (or something not plugged in, like a speaker), not about how it was flashed.');
    } else {
      show('heard', '');
      say('Nothing was heard. Press RST once and listen again. A board running its own firmware may simply print nothing.');
    }
  } catch (problem) {
    say(`Listening stopped: ${problem.message}`);
  } finally { busy = false; controls(); }
};

$('board').onchange = $('agree').onchange = controls;
if (!('serial' in navigator) || !window.isSecureContext)
  say('This page needs desktop Chrome or Edge. Safari, Firefox and phones cannot talk to a board over USB.');
controls();
