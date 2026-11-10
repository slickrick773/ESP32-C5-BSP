# Bench logs, 2026-11-10

Raw serial logs behind the 2026-11-10 results in the board notes and the
verification ledger. Each line is prefixed with the seconds since the board
was reset (`tools/bench/serial_log.py`). GPS positions are replaced with
`<position withheld>`; nothing else is edited.

| File | Board | What it shows |
|---|---|---|
| `boardcheck28.log` | 2.8-inch | Whole-board check. The first run shows the IMU line failing before the check was changed to wait for the sensor's first real sample; the later runs pass |
| `selftest28.log` | 2.8-inch | Audio self-test, three runs |
| `sd28.log` | 2.8-inch | microSD test |
| `ta28.log` | 2.8-inch | Combined screen demo: two tones and a quiet microphone reading |
| `gps28_baseline.log` | 2.8-inch | Touch baseline with no GPS fitted |
| `gps28_gps.log` | 2.8-inch | First run with the GPS fitted, taps counted alongside |
| `gps28_soak2.log` | 2.8-inch | Three-minute GPS run to a fix |
| `boardcheck35.log` | 3.5-inch (LiPo board) | Whole-board check, two runs |
| `gps35_regress.log` | 3.5-inch (LiPo board) | GPS re-run after the driver moved to shared code |
| `ta35_final.log` | 3.5-inch (LiPo board) | Touch demo with the 1 kHz tone: one tone, one microphone reading |

What the logs do not contain: anything a person heard or saw. Those
judgements (a clean tone, a counter that kept running, a correct layout) are
recorded in the board notes as the owner's observations.
