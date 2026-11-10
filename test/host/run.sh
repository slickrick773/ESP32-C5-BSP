#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Build and run the BSP host tests. No hardware, no PlatformIO: the real driver
# sources are compiled against the mocks in ./mock and run on the host.
set -u
cd "$(dirname "$0")"

CXX=${CXX:-g++}
FLAGS="-std=c++17 -Imock"
SRC=../../src
BOARD=$SRC/board/ws_c5_35
BOARD28=$SRC/board/ws_c5_28
BIN=$(mktemp -d)
fail=0

run_one() {
    local name=$1; shift
    if ! $CXX $FLAGS "$@" mock.cpp -o "$BIN/$name" 2> "$BIN/$name.build.log"; then
        echo "BUILD FAILED: $name"
        cat "$BIN/$name.build.log"
        fail=1
        return
    fi
    echo "== $name =="
    if "$BIN/$name"; then :; else fail=1; fi
    echo
}

run_one test_bus     test_bus.cpp     "$SRC/chip/esp32c5_i2c_bus.cpp"
run_one test_clients test_clients.cpp "$BOARD/ws_c5_35_battery.cpp" "$BOARD/ws_c5_35_expander.cpp" "$SRC/chip/esp32c5_i2c_bus.cpp"
run_one test_touch   test_touch.cpp   "$BOARD/ws_c5_35_touch.cpp"   "$BOARD/ws_c5_35_expander.cpp" "$SRC/chip/esp32c5_i2c_bus.cpp"
run_one test_touch_28 test_touch_28.cpp "$BOARD28/ws_c5_28_touch.cpp" "$BOARD28/ws_c5_28_expander.cpp" "$SRC/chip/esp32c5_i2c_bus.cpp"

run_one test_battery_28 test_battery_28.cpp "$BOARD28/ws_c5_28_battery.cpp" "$SRC/chip/esp32c5_i2c_bus.cpp"

run_one test_audio_28 test_audio_28.cpp "$BOARD28/ws_c5_28_audio.cpp" "$BOARD28/ws_c5_28_expander.cpp" "$SRC/chip/esp32c5_i2c_bus.cpp"

run_one test_audio_35 test_audio_35.cpp "$BOARD/ws_c5_35_audio.cpp" "$SRC/chip/esp32c5_i2c_bus.cpp"

run_one test_rtc_35 test_rtc_35.cpp "$SRC/part/pcf85063_rtc.cpp" "$SRC/chip/esp32c5_i2c_bus.cpp"
run_one test_env_35 test_env_35.cpp "$SRC/part/shtc3_env.cpp" "$SRC/chip/esp32c5_i2c_bus.cpp"
run_one test_imu_35 test_imu_35.cpp "$SRC/part/qmi8658_imu.cpp" "$SRC/chip/esp32c5_i2c_bus.cpp"

run_one test_audio_shutdown -I"$SRC" test_audio_shutdown.cpp "$BOARD28/ws_c5_28_audio.cpp" "$BOARD28/ws_c5_28_expander.cpp" "$SRC/chip/esp32c5_i2c_bus.cpp"

run_one test_startup test_startup.cpp "$BOARD/ws_c5_35_board.cpp" "$BOARD/ws_c5_35_expander.cpp" "$SRC/chip/esp32c5_i2c_bus.cpp"
run_one test_startup_28 -DBOARD_28 test_startup.cpp "$BOARD28/ws_c5_28_board.cpp" "$BOARD28/ws_c5_28_expander.cpp" "$SRC/chip/esp32c5_i2c_bus.cpp"
for name in test_startup test_startup_28; do
    if [[ -x "$BIN/$name" ]]; then "$BIN/$name" default || fail=1; fi
done

# Startup probe: the bit-banged early I2C write used by the experimental
# bootloader hook (tools/startup_probe). Self-contained; no mocks.
echo "== test_early_i2c =="
if $CXX -std=c++17 -I ../../tools/startup_probe/bootloader_components/dark_start \
        ../../tools/startup_probe/test/test_early_i2c.cpp -o "$BIN/test_early_i2c" 2> "$BIN/tei.log" \
        && "$BIN/test_early_i2c"; then :; else cat "$BIN/tei.log"; fail=1; fi
echo

rm -rf "$BIN"
if [ "$fail" -ne 0 ]; then
    echo "RESULT: SOME HOST TESTS FAILED"
    exit 1
fi
echo "RESULT: ALL HOST TESTS PASSED"
