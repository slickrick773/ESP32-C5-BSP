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

rm -rf "$BIN"
if [ "$fail" -ne 0 ]; then
    echo "RESULT: SOME HOST TESTS FAILED"
    exit 1
fi
echo "RESULT: ALL HOST TESTS PASSED"
