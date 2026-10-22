# Examples

One runnable sketch per phase, each with its own `platformio.ini` pinned to the
`pioarduino` platform the C5 needs. Examples are added as each phase is verified
on hardware — an example is not committed here until its board capability has
been observed working on a real board.

Planned:

- `hello_board/` — display init + touch read + print board name (Phase 1)
- `touch_solid/` — shared-bus touch with the re-open + retry discipline (Phase 2)
- `gps_i2c/` — LC76G over I²C, print fix + satellite count (Phase 3)
- `battery/` — AXP2101 readout via the expander (Phase 4)
- `reliability_reference/` — Wi-Fi + GPS + touch at once (capstone)
