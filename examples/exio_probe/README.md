# Expander startup read probe

Diagnostic app for the 3.5-inch board. It does not initialize graphics, so a
blank or retained display is not a rendering test. It selects register pointers
and reads responses by default. In v5, an explicit serial `0` command writes
only the documented PWM register 0x05 to zero, then reads it with lengths one
and two. No mode/output/reset registers are written. It does not establish
optical darkness, undocumented register widths or expander reset coupling.

Use the verified stock bootloader for the untreated baseline. v5 saves its first
sample after Wire initialization and before Serial initialization, and records it in a four-entry checksum-protected RTC-memory history. It reprints
that history every two seconds alongside a fresh sample. RTC retention is tested
per reset type; checksum failure starts a new history. No flash logging is used. `e` replays the
early sample; `r` requests `esp_restart()`. A boot identifier distinguishes runs.
`timer_*_us` uses the ESP timer epoch, not physical power application or ROM entry.
`setup_delta_us` is relative to setup entry. Every read records requested,
reported and actually consumed counts independently; failed pointer selection
skips the read. The PMU response is raw data, not proof of physical supply state.

Build with the pinned platform and supported Python in an isolated PlatformIO
core. Flash only firmware.bin at 0x10000 with ROM-only c5flash and verification.
Never upload the generated factory image for this test. Preserve the partition
table and stock bootloader. The host capture helper is
`tools/startup_probe/host/capture_serial.py`; supply an explicit port and output
path. It reconnects only to that port, sets DTR/RTS false, and can
optionally send software-reset requests. USB resets were nevertheless observed
when opening the serial port; do not assume that capture is nonintrusive. If a hub changes the device path,
identify the board's new port explicitly and restart capture there.

Collect separately:

1. All-power-removed startup, with the physical supply arrangement recorded.
2. Button reset, after the cold sample has been saved.
3. Explicit esp_restart software reset, with several repetitions.

USB removal is insufficient if a battery or another supply remains connected.
Reset reason POWERON is not proof of loss of power to the whole board. Equal
responses across reset types do not prove that the CH32 did or did not reset.
Keep raw response interpretation separate from electrical and optical evidence.

Status: stock bootloader region matched the saved original region exactly.
After a user-reported long unplug/reconnect through a reliable hub, the v3 probe
reported mode/output/input/PWM raw responses FFFF/0000/0000/FFFF. The captured
boot reason was USB(11), so this is not an untouched power-on sample. Three
explicit esp_restart trials reported SW(3) and the same responses. These differ
from the earlier 23FF/2300/3700/00FF readings, but do not establish register width,
power-on transition timing, or optical darkness. v4 adds RTC history to retain
samples that precede a USB-triggered reset. Its app-only flash verified.

RTC history validation passed on hardware: the same saved boot samples survived
one esp_restart and a subsequent serial reopen/USB reset. The retained reset
sequence was POWERON(1), USB(11), SW(3), USB(11), with four distinct boot IDs.
The first POWERON report followed flashing, not a physical cold cycle. A physical
cold cycle with v4 remains required to obtain its first post-power sample.

The v4 physical-cycle capture preserved POWERON(1) and later USB(11) samples,
both returning 02=FFFF, 03=0000, 04=0000, 05=FFFF. The user reported persistent
glow. In v5, the explicitly requested PWM-zero transaction returned status 0;
subsequent reads returned 00 (one byte) and 00FF (two bytes). Mode and output
responses remained unchanged. The user confirmed the glow disappeared and the screen was dark after that
controlled write. Optical transition latency was not measured. v5 has a new RTC-history signature so older firmware's
records are not mislabeled as v5 captures.

Investigation result: user confirmed PWM-zero makes the screen dark. Software
reset preserved the dark state; physical RST returned the glow with response
FFFF. Holding RST kept it dark until release. ROM download mode also glowed,
with no-reset ROM-only access confirmed. hello_board has been restored (app-only
flash verified) and stock bootloader remains installed. The overall startup-flash
fix is still incomplete; see the hardware findings for limits and next steps.
