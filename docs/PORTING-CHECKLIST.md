# Porting checklist: firmware onto the Waveshare ESP32-C5-Touch-LCD-3.5

A one-page list of what every firmware port to this board has had to get right.
Each item links to the finding behind it, so you can check the evidence instead
of taking the list on trust. You do not need to depend on this package to use
it: most ports copy the few lines they need.

It is written for the 3.5-inch board. The 2.8-inch board differs in a few
places, listed [at the end](#the-28-inch-board-differs-here).

## Build and flash

- [ ] **Route `Serial` to the USB port.** Build with
  `-DARDUINO_USB_CDC_ON_BOOT=1` (Arduino CLI: `CDCOnBoot=cdc`). Without it your
  own log lines go to a header pin and the USB port looks silent.
  [Finding](HARDWARE-FINDINGS.md#console-is-on-usb-serial-jtag-not-a-uart)
- [ ] **Keep every UART off GPIO13 and GPIO14.** They are the USB data lines. A
  UART there drops the board off USB at boot and breaks hands-free flashing.
  [Finding](HARDWARE-FINDINGS.md#console-is-on-usb-serial-jtag-not-a-uart)
- [ ] **Flash the bootloader at `0x2000`, and set the flash mode.** The C5 loads
  its bootloader from `0x2000`, not `0x0`, and a bootloader written with the
  flash mode left on "keep" will not boot. Both mistakes look like a dead, dark
  screen.
  [Offset](HARDWARE-FINDINGS.md#bootloader-offset-is-0x2000-on-the-c5-not-0x0),
  [flash mode](HARDWARE-FINDINGS.md#flashing-must-set-the-flash-mode-or-the-c5-wont-boot-and-the-screen-stays-dark)
- [ ] **Expect large USB transfers to stall now and then.** Flash in chunks and
  verify; [`tools/c5flash`](../tools/c5flash) does this.
  [Finding](HARDWARE-FINDINGS.md#the-usb-link-stalls-on-some-data-patterns-during-large-transfers)
- [ ] **Pin tasks for one core.** The C5 has a single core, so code copied from a
  dual-core chip that pins to core 1 needs a chip-aware choice.
  [Finding](HARDWARE-FINDINGS.md#the-c5-is-single-core)

## Start-up order

- [ ] **Bring up the I/O expander before the display and touch.** The expander
  at `0x24` owns the display reset (EXIO1), the touch reset (EXIO0) and the
  backlight. Use the verified sequence: drive EXIO0, EXIO1 and EXIO5 high
  together, then pulse each reset. Do it once: a second display reset after the
  panel is initialised blanks the screen.
  EXIO5 is the speaker amplifier enable according to the vendor's schematic and
  current example code; older comments, including in this package, call it
  panel power. That has not been bench-tested here, and the sequence works
  either way, so do not build power-saving or muting on it until it is.
  [Board notes](BOARD-WS-C5-35.md#board-specific-notes),
  [the sequence in code](../src/board/ws_c5_35/ws_c5_35_expander.cpp)
- [ ] **Set `TFT_RST` and `TFT_BL` to `-1`.** There is no reset pin and no
  backlight pin. Brightness is a write to the expander, so `ledcWrite()` on a
  pin does nothing here.
  [Finding](HARDWARE-FINDINGS.md#backlight-dimming-is-through-the-expander-not-a-tft_bl-pin)
- [ ] **Start dark.** Leave the backlight off in the expander start-up, draw the
  complete first frame, wait about 150 ms, then turn the backlight on. Lit any
  earlier, the panel shows its own memory: static after power-on, the previous
  picture after a reset.
  [Clean startup](BOARD-WS-C5-35.md#clean-startup)
- [ ] **Check nothing else turns the backlight on early.** Brightness-restore
  code that runs before the first frame undoes the step above. Search the
  start-up path for every backlight write, not only the first.

## Buses

- [ ] **Treat SDA 27 / SCL 26 as one shared bus.** Touch, expander, power chip,
  audio codec and GPS are all on it. Re-open the bus once per touch read, read
  the codec with a STOP rather than a repeated start, and never re-open in the
  middle of a GPS transaction.
  [Finding](HARDWARE-FINDINGS.md#shared-ic-bus-discipline)
- [ ] **Turn the NeoPixel off.** Configurations inherited from other boards
  drive a status LED on GPIO27, which is this board's SDA line. Every LED update
  corrupts the bus, and it can leave the panel black after a reset.
  [Finding](HARDWARE-FINDINGS.md#neopixel-is-on-gpio27--which-is-the-ic-sda-line)
- [ ] **Share one SPI bus between the display and the microSD card.** Keep one
  live SPI object, give each device its own transaction and chip select
  (display CS 8, card CS 9), and do not tear the bus down because the card
  failed to mount.
  [Board notes](BOARD-WS-C5-35.md#board-specific-notes)

## Touch

- [ ] **Gate touch reads on the interrupt line (GPIO3).** Polling the FT6336
  with no finger down can return a stale point, which shows up as phantom taps.
  [Finding](HARDWARE-FINDINGS.md#the-touch-break-story-why-ft6336-feels-the-way-it-does)
- [ ] **Validate what a read returns.** Accept one or two points with on-panel
  coordinates, and retry a couple of times before reporting "no touch".
  [Finding](HARDWARE-FINDINGS.md#shared-ic-bus-discipline)

## GPS

- [ ] **Use the I²C route.** On this board as shipped, the header's UART pins do
  not reach the chip; only SDA, SCL, 3V3 and GND do. A UART GPS wired to the
  header is never heard.
  [Finding](HARDWARE-FINDINGS.md#the-j3-header-uart-is-not-bonded-to-the-chip-as-shipped),
  [GPS guide](GPS.md)
- [ ] **Remove any inherited UART GPS pins.** A leftover `GPS_RX 13` /
  `GPS_TX 14` is the most common way ports lose their USB port.

## Power and audio

- [ ] **Read the battery from the power chip on the shared bus.** The AXP2101
  reports level, voltage and charge state, and still answers with no cell
  fitted.
  [Finding](HARDWARE-FINDINGS.md#battery-the-axp2101-fuel-gauge-reads-over-plain-ic-no-xpowerslib-needed)
- [ ] **Clock the audio codec from BCLK.** There is no MCLK connection to the
  ES8311. The microphone arrives on the left I2S slot.
  [Speaker](BOARD-WS-C5-35.md#speaker-playback),
  [microphone](BOARD-WS-C5-35.md#microphone-capture)
- [ ] **Do not rely on the clock across a power loss.** The real-time clock has
  no backup cell on this board.
  [Real-time clock](BOARD-WS-C5-35.md#real-time-clock)

## Radio

- [ ] **Call `set_band_mode()` after `esp_wifi_start()`.**
  [Finding](HARDWARE-FINDINGS.md#dual-band-radio-set_band_mode-must-come-after-esp_wifi_start)
- [ ] **Ask the driver which 5 GHz channels exist.** Do not use a fixed list.
  [Finding](HARDWARE-FINDINGS.md#5-ghz-channel-enumeration-is-driver-gated-not-fixed)

## Before you call it done

- [ ] **Film the boot in slow motion, after RST and after a cold power-on.**
  Static and stale frames last a few hundredths of a second and are easy to miss
  by eye. Time from the reset to the first frame, not from dark to light, if the
  firmware mounts a card before lighting the screen.
- [ ] **Test with the RST button and a re-plug, not only a reset sent over
  USB.** Some firmware images crash once in start-up after a USB reset and then
  boot normally; the button and power-on did not show it.
  [Finding](HARDWARE-FINDINGS.md#a-reset-sent-over-usb-can-crash-psram-start-up-on-some-images)
- [ ] **Run the whole-board check** to confirm the board itself is healthy
  before blaming the port:
  [`board_check_35`](../examples/board_check_35/README.md).
- [ ] **Say what you tested.** In the pull request, list what ran on a real
  board and what only compiled.

## The 2.8-inch board differs here

- The display is an ST7789 and the touch chip is a CST3530 at `0x58`, not an
  FT6336. [Display](BOARD-WS-C5-28.md#display-the-st7789-byte-order-fix),
  [touch](BOARD-WS-C5-28.md#touch-cst3530-not-ft6336)
- The shared I²C bus is on SDA 0 / SCL 1, and the microSD chip select is GPIO 23.
  [Pin map](BOARD-WS-C5-28.md#pin-map-hardware-verified-on-the-real-28-board-2026-10-31)
- The GPS module's wake address is also `0x58`, the same as the touch chip, so
  start the GPS driver without the wake address. [GPS](BOARD-WS-C5-28.md#gps)
- There is no pin header. Add-ons connect through the board's sockets.
  [Connectors](BOARD-WS-C5-28.md#connectors)
