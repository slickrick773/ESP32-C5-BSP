# Startup backlight probe (experimental; both symptoms persist on hardware)

Full dark-boot acceptance remains **FAILED**. This is an opt-in diagnostic
second-stage bootloader for Waveshare ESP32-C5-Touch-LCD-3.5 only. It is not
integrated into the Arduino build or upload path. The scaffold application is
not a replacement for `hello_board` and must not be flashed.

## Current hardware status (2026-11-05)

The experimental bootloader has been removed; a fresh ROM read matched the
verified stock bootloader-region backup byte-for-byte. The controlled probe
investigation is recorded in [HARDWARE-FINDINGS](../../docs/HARDWARE-FINDINGS.md#hide-panel-memory-until-the-first-frame).
The user confirmed PWM-zero alone darkens the panel, esp_restart preserved that
state, physical RST brought the glow back, holding RST kept it dark until release,
and ROM download mode also glowed. ROM-only verification without a reset request
confirmed download access in that last condition. hello_board has now been rebuilt,
flashed app-only and verified; the diagnostic probe is no longer installed.
No default-off firmware change has been made, and full dark-boot acceptance remains
FAILED.

## Earlier experimental bootloader result (2026-11-05)

The user authorized this candidate, and ROM-only c5flash wrote 21,552 bytes at
0x2000 with verification successful. No partition or application images were
written. Serial capture after reset reported `[dark-start] result=0 attempts=1`,
followed by `hello_board` reporting expander up and touch present. A PSRAM
`Failed to allocate dummy cacheline for PSRAM memory barrier!` log was also
captured. The same message appears in the pre-existing 2026-11-04 BLE evidence,
so its presence alone is not evidence of a new bootloader regression.
On hardware (observed 2026-11-05) the acknowledged PWM write eliminated **neither**
the fuzzy cold-boot flash nor the retained previous frame on reset, though the
application boots with normal display and touch. An ACK records only that the
expander answered; it does not establish when or whether the backlight went dark.
This was later characterised: ROM download-mode isolation (no second-stage
bootloader or application running, still glowing) shows the backlight is on at
power-up without any user-flashed ESP write, and the expander's PWM output controls
it (a PWM=0 write darkens the panel). The tested writes and hooks did not eliminate
the observed flash; whether an earlier ROM-level action could pre-empt it is
unresolved, as the first-illumination timing against ROM startup was not measured.
Whether the
expander drives the line high at power-up or it defaults high undriven, and the
flash's exact optical duration, are not established. See the full reassessment in
`docs/HARDWARE-FINDINGS.md`.

## Evidence and limits

The user observes a fuzzy cold-boot flash and retained frame during reset.
Moving PWM suppression into `initVariant()` did not fix cold boot. A diagnostic
video shows illumination before expander initialization and darkness afterward
until UI reveal. Its 30 fps presentation stream and unity-rate edit list do not
prove capture speed or correlate those transitions with boot-stage timestamps.
No measured ROM/application boundary is available.

Vendor sources at revision `ef03a84788c1157836f1e7fc29f5a24f16cf22ff`:

- [Board_IO.cpp](https://github.com/waveshareteam/ESP32-C5-Touch-LCD-3.5/blob/ef03a84788c1157836f1e7fc29f5a24f16cf22ff/examples/arduino/examples/07_lvgl_demo/Board_IO.cpp)
  writes PWM zero during initialization; this is not a specification of the
  CH32 firmware's power-on default.
- [io_extension.h](https://github.com/waveshareteam/ESP32-C5-Touch-LCD-3.5/blob/ef03a84788c1157836f1e7fc29f5a24f16cf22ff/examples/arduino/examples/01_exio/io_extension.h)
  defines address 0x24 and mode/output/PWM registers 0x02/0x03/0x05.
- [Schematic](https://github.com/waveshareteam/ESP32-C5-Touch-LCD-3.5/blob/ef03a84788c1157836f1e7fc29f5a24f16cf22ff/hardware/schematic/ESP32-C5-Touch-LCD-3.5.pdf)
  shows CH32 PC3 as EXIO_PWM, LCD_BL driving Q2 through R40, and the CH32
  PA7/RESET label connected to CHIP_PU. A label is not evidence of the fitted
  board's reset configuration or its firmware behavior. The inspected vendor
  tree contains host-side expander drivers, but no identified CH32 firmware
  establishing its reset defaults. The schematic labels EXIO5 as PA_CTRL and
  EXIO6 as SYS_OUT, while the vendor `Board_IO.h` names EXIO5 BOARD_POWER_EXIO.
  Do not treat that driver name as proof of LCD rail switching. (A later bench
  test, 2026-11-10, showed EXIO5 is the speaker amplifier enable.) The probe
  avoids this ambiguity by leaving all mode/output registers untouched.

## Candidate behavior

`bootloader_after_init()` runs after IDF hardware initialization and before
partition selection/application validation and loading. There is deliberately
no before-init hook: BSS, flash, clock and protection restrictions make that
hook a separate investigation. Neither hook executes during ROM startup.

The probe uses GPIO27 SDA and GPIO26 SCL as open-drain signals and writes only
`0x48, 0x05, 0x00` (address/write, PWM register, off). It checks every ACK, allows
up to 2 ms total clock stretching per transaction, retries only NACK up to
three attempts with 1 ms gaps, and releases output enables on every outcome.
Busy/stuck lines and arbitration loss return without a retry. The delay budget
is below 10 ms for all three attempts, plus instruction overhead; actual board
timing remains unmeasured. It does not configure expander modes, power or reset
outputs, recover the bus with extra clocks, allocate memory, or use FreeRTOS.
A single post-attempt console record reports result (0 OK, 1 busy, 2 timeout,
3 NACK) and attempts. ACK is not proof that the screen went dark.

The build uses installed IDF 5.5.2; the Arduino libraries use IDF 5.5.5. This
version difference is a hardware-validation risk, not a claim of tested runtime
compatibility. Rollback stays enabled, watchdog stays enabled at 9000 ms, image
validation policy stays enabled except the stock deep-sleep exception, and
bootloader/partition offsets remain 0x2000/0x8000. The diagnostic selects USB
Serial/JTAG as its primary console and omits the build timestamp. No eFuse operations or
partition/OTA rewrites are added.

## Build and host test

Activate IDF 5.5.2 with a supported Python environment, then from this folder:

```sh
idf.py -D IDF_TARGET=esp32c5 bootloader
c++ -std=c++17 -Wall -Wextra -Werror test/test_early_i2c.cpp -o /tmp/test_early_i2c
/tmp/test_early_i2c
```

The tested candidate is 21,552 bytes (0x5430), leaving 0xbd0 bytes before 0x8000.
SHA-256: `d0043129372f8d9d6e9d93ddb7f7e579f11913c23db48746497ee94a72a18f8e`.
A new build may differ; inspect its header, size, configuration, symbols and
hash before proposing it for hardware use. Host tests validate transaction bytes,
ACK/NACK handling, stretching, stuck lines and arbitration, not pad waveforms
or optical behavior.

## Recovery and hardware test plan

A custom bootloader flash requires explicit approval. Do not use `idf.py flash`,
which could overwrite the existing application with the scaffold.

Before any bootloader write, preserve the entire 0x2000..0x8000 region, compare
independent reads, and ROM-verify that backup. This was completed for the
current board on 2026-11-05; the image prefix exactly matches its stock
`hello_board` bootloader. Store backups outside the repository. Current backup
region SHA-256:
`21f6bcf1d058f1395c4d1629b90a08f9607785ccab262b87ad5df965b916edbd`.

After approval, with `C5FLASH_ESPTOOL` set to the installed esptool executable,
`PORT` set to the confirmed device and `RECOVERY_DIR` set to the verified backup
folder, run from the repository root:

```sh
python3 tools/c5flash/c5flash.py --port "$PORT" write 0x2000 "$RECOVERY_DIR/candidate-bootloader.bin"
# Recovery, if needed:
python3 tools/c5flash/c5flash.py --port "$PORT" write 0x2000 "$RECOVERY_DIR/original-bootloader-region.bin"
```

Use ROM-only c5flash with an explicit confirmed port and
verification. Write only the candidate at 0x2000. Leave partition table 0x8000,
boot_app0 0xe000 and application 0x10000 untouched. Capture the probe result and
application startup logs; verify display, touch and subsequent resets. Observe
both reset-button boot and a real cold power cycle, with synchronized serial
and video or a logic-analyzer trace of CHIP_PU, I2C, LCD_BL and panel reset.
Do not infer physical darkness from logs alone.

If boot or peripherals regress, hold BOOT, tap RST, release BOOT to enter ROM
download mode. Restore the saved bootloader region at 0x2000 using c5flash and
verify it. The region ends exactly at 0x8000 and does not touch partition data.
Restore even if the probe merely fails to improve the visual symptom. Do not
alter security fuses or boot straps in software to work around recovery errors.

## Validation recorded 2026-11-05

- IDF 5.5.2 bootloader build and image checksum/hash validation passed; the ELF
  contains the strong after-init hook and no before-init hook.
- Host I2C tests passed, including a stuck clock at every transfer edge.
- Expanded BSP host suite and 13 flasher tests passed.
- All ten Arduino examples and both clean file-based consumer snapshots built
  with Python 3.13 and an isolated PlatformIO core. Git-pinned CI was not run.
- Two bootloader-region reads matched, matched the stock image prefix, and
  passed ROM flash verification. Verification reset the board afterward.
- The initial preparation made no flash writes. After explicit authorization,
  the candidate was flashed and verified as recorded above. Optical acceptance
  remains unverified; startup logs confirm the probe and application ran.
