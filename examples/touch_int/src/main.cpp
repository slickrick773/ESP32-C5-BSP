// SPDX-License-Identifier: MIT
// touch_int — prove the optional INT-gated read on WsC5_35::Touch.
//
// With beginInt(TP_INT) the driver skips the I2C read whenever the FT6336 INT
// line says no finger is down, so "no touch" reports no touch instead of a
// stale/phantom point. This test shows, once a second: how many polls saw the
// INT idle (no finger) vs active (finger), and how many reads actually returned
// a point. Untouched: finger=0, reads=0, no coord lines (no phantom). Touched:
// finger>0 and coord lines appear.
//
// Console: USB-Serial-JTAG at 115200.
#include <Arduino.h>
#include <board/ws_c5_35/ws_c5_35_board.h>

using namespace WsC5_35;

static const int TP_INT = 3;   // FT6336 INT -> GPIO3 (schematic-confirmed)

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.printf("[tint] %s -- INT-gated touch\n", boardName());

    begin();                   // expander + touch on the shared bus
    Touch::beginInt(TP_INT);   // opt into the INT gate
    Serial.println("[tint] leave untouched: expect finger=0, reads=0, no coords (no phantom)");
}

void loop() {
    static uint32_t last = 0, winStart = 0;
    static uint32_t idle = 0, finger = 0, reads = 0;
    const uint32_t now = millis();
    if (now - last < 15) return;   // pace the loop
    last = now;

    if (digitalRead(TP_INT) == LOW) finger++; else idle++;

    uint16_t x, y;
    if (Touch::read(x, y)) {       // returns false immediately when INT is high
        reads++;
        Serial.printf("[tint] touch %u,%u\n", x, y);
    }

    if (now - winStart >= 1000) {
        Serial.printf("[tint] idle(no finger) %lu, finger %lu, reads %lu\n",
                      (unsigned long)idle, (unsigned long)finger, (unsigned long)reads);
        winStart = now; idle = finger = reads = 0;
    }
}
