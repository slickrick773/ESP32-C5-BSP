# BLE heap bench evidence — 2026-11-04

Board: user-confirmed Waveshare ESP32-C5-Touch-LCD-3.5, ROM banner
`esp32c5-eco3-20250704`. Stock Arduino 3.3.10 / ESP-IDF v5.5.4.

- `original-serial-excerpt.txt`: selected lines from the existing bridge firmware.
- `build-metadata.json`: exact probe source and app binary SHA-256, core and FQBN.
- `selected-sdkconfig.txt`: relevant definitions from the installed SDK.
- `enable-trials.log`: fresh-boot first-start trials, followed by one fresh-boot
  fragmentation/recovery trial. `cap0` = INTERNAL, `cap1` = INTERNAL|8BIT,
  `cap2` = INTERNAL|8BIT|DMA; sizes are bytes. Pressure allocations are 256 bytes
  each; use logged measurements, not command targets. Advertising payload
  configuration succeeds before pressure is applied.
- `repeat-trials.log`: three additional trials at target 17500 and two at 17000,
  each with roughly four seconds between start and the final heap query.

The fragmentation trial follows the last numbered target trial after another
reboot. It frees alternating pressure blocks, tries advertising once, then frees
the remaining blocks. Its last `a` command retries while advertising is already
active; `rc=2` there is not a failed cold start.

A preliminary probe configured payloads *after* pressure and encountered a
scan-response configuration failure; it was replaced before collecting the
archived enable trials. The archived source/hash correspond to the corrected
probe that isolates advertising enable.

No external BLE scanner/peer was used. GAP active state is not proof of on-air
packets, and the logs deliberately retain allocation failures occurring after a
successful start return. Boot logs also retain the PSRAM cacheline warning.
Firmware backup and build outputs are kept outside the repository.

After testing, the saved original flash region was restored with `c5flash`;
verification returned `OK`, and the original application's I2C activity resumed
over USB serial. No commit or push was performed.
