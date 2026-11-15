// SPDX-License-Identifier: MIT
// Talking to the ESP32-C5's built-in loader (the ROM) from a browser, through
// the pinned esptool-js bundle. Nothing is uploaded to the chip's memory: every
// command here is one the ROM itself understands, as in tools/c5flash.
//
// esptool-js 0.7.0 builds some packets in the shorter form its own RAM helper
// takes. The C5's ROM wants what Python esptool sends, so four things differ:
//   - blocks of 0x400 bytes when writing, not 0x4000;
//   - one more word (encryption, zero) on the compressed-write start command;
//   - one more word (reserved, zero) on the flash attach command;
//   - the flash chip's size has to be told to the ROM (command 0x0B) before a
//     large region is written, or the write is refused.
// And one thing is missing from the bundle: reading flash through the ROM
// (command 0x0E, 64 bytes at a time).
import { ESPLoader } from './vendor/esptool-js-0.7.0.js';
import { readInBlocks } from './doctor.mjs';

const SET_FLASH_SIZE = 0x0B, READ_SLOW = 0x0E, READ_BLOCK = 64;

function words(...values) {
  const out = new Uint8Array(values.length * 4), view = new DataView(out.buffer);
  values.forEach((v, i) => view.setUint32(i * 4, v >>> 0, true));
  return out;
}

export class C5Rom extends ESPLoader {
  constructor(options) {
    super(options);
    this.FLASH_WRITE_SIZE = 0x400;
  }

  async checkCommand(description, op, data, ...rest) {
    if (this.chip?.CHIP_NAME === 'ESP32-C5' && this.IS_STUB === false) {
      const wanted = op === this.ESP_FLASH_DEFL_BEGIN && data?.length === 16 ? 20 : op === this.ESP_SPI_ATTACH && data?.length === 4 ? 8 : 0;
      if (wanted) { const longer = new Uint8Array(wanted); longer.set(data); data = longer; }
    }
    return super.checkCommand(description, op, data, ...rest);
  }

  // id, total size, block 64 KB, sector 4 KB, page 256, status mask.
  async tellFlashSize(bytes) {
    await this.checkCommand('set the flash size', SET_FLASH_SIZE, words(0, bytes, 0x10000, 0x1000, 0x100, 0xFFFF));
  }

  // `length` bytes of flash from `address`, or null if the ROM would not give them.
  readFlashSlow(address, length, progress) {
    return readInBlocks(at => this.checkCommand('read flash', READ_SLOW, words(at, READ_BLOCK), 0, READ_BLOCK),
      () => this.revive(), address, length, progress);
  }

  // Reach the chip again on the same port after it stopped answering.
  async revive() {
    try { await this.transport.disconnect(); } catch { /* already closed */ }
    await this.detectChip('default_reset', 3);
    if (this.chip.postConnect) await this.chip.postConnect(this);
    await this.flashSpiAttach(0);
    if (this.flashBytes) await this.tellFlashSize(this.flashBytes);
  }
}

// Reach the ROM and learn what the chip is. Throws if it is not a C5 that can be written.
export async function reach(loader, attempts = 7) {
  await loader.detectChip('default_reset', attempts);   // Never main(): that would upload a RAM helper.
  if (!loader.chip) throw Error('The chip did not say what it is.');
  if (loader.chip.CHIP_NAME !== 'ESP32-C5') throw Error(`This is an ${loader.chip.CHIP_NAME}, not an ESP32-C5. Nothing was written.`);
  if (loader.secureDownloadMode) throw Error('This chip is locked (secure download mode). The doctor cannot read or write it.');
  if (loader.chip.postConnect) await loader.chip.postConnect(loader);
  await loader.flashSpiAttach(0);
  const flash = await loader.detectFlashSize();
  const megabytes = /^(\d+)MB$/.exec(flash || '');
  loader.flashBytes = megabytes ? Number(megabytes[1]) * 1024 * 1024 : 0;
  if (loader.flashBytes) await loader.tellFlashSize(loader.flashBytes);
  return { chip: loader.chip.CHIP_NAME, flash: flash || 'size not read', flashBytes: megabytes ? Number(megabytes[1]) * 1024 * 1024 : 0,
    address: await loader.chip.readMac(loader) };
}

// The ROM's MD5 of a region, as 32 lowercase hex characters. esptool-js gives
// the ROM's 32 text characters hex-encoded again; undo that.
export async function flashMd5(loader, address, length) {
  let said = await loader.flashMd5sum(address, length);
  if (/^[a-f0-9]{64}$/i.test(said)) said = String.fromCharCode(...said.match(/../g).map(pair => parseInt(pair, 16)));
  return said.toLowerCase();
}

// Write the pieces of a cure and check each one on the chip afterwards.
export async function writeAndVerify(loader, pieces, progress) {
  await loader.writeFlash({ fileArray: pieces.map(p => ({ data: p.data, address: p.offset })), flashSize: 'keep', flashMode: 'keep',
    flashFreq: 'keep', eraseAll: false, compress: true, reportProgress: progress });
  for (const p of pieces)
    if (await flashMd5(loader, p.offset, p.bytes) !== p.md5) throw Error(`${p.file} did not read back as written. Keep the board connected and try the cure again.`);
}
