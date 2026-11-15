// SPDX-License-Identifier: MIT
// Host test for the 3.5-inch camera control driver (identity and registers).
#include <vector>
#include <Wire.h>
#include <freertos/semphr.h>
#include "testutil.h"
#include "../../src/board/ws_c5_35/ws_c5_35_camera.h"
#include "../../src/chip/esp32c5_i2c_bus.h"
namespace Cam = WsC5_35::Camera;
static uint8_t* reg = g_wireRegisters[0x6E];
int main() {
    Esp32C5::I2cBus::begin(27, 26, 100000);
    g_wireRegisterMode = true;
    uint8_t v = 0x55;

    CHECK(!Cam::begin() && !Cam::present(), "blank identity rejected");
    CHECK(!Cam::readReg(0x12, v) && v == 0x55, "read rejected before setup, output unchanged");
    CHECK(!Cam::writeReg(0x12, 0x01), "write rejected before setup");
    reg[0xFC] = 0x39; reg[0xFD] = 0x02;
    CHECK(!Cam::begin(), "wrong identity rejected");
    reg[0xFD] = 0x01;                            // 39 01, as read on hardware
    g_wireEndTransStatus = 2;
    CHECK(!Cam::begin(), "no answer on the bus rejected");
    g_wireEndTransStatus = 0;
    g_takeResult = pdFALSE;
    CHECK(!Cam::begin(), "begin reports lock failure");
    g_takeResult = pdTRUE;
    CHECK(Cam::begin() && Cam::present(), "camera detected");

    CHECK(Cam::writeReg(0x12, 0x80) && reg[0x12] == 0x80, "register written");
    CHECK(Cam::readReg(0x12, v) && v == 0x80, "register read back");
    const Cam::RegValue table[] = {{0x20, 0x11}, {0x21, 0x22}, {0x22, 0x33}};
    CHECK(Cam::writeRegs(table, 3) && reg[0x20] == 0x11 && reg[0x22] == 0x33, "table written in order");
    g_wireFailReg = 0x21;
    const Cam::RegValue table2[] = {{0x20, 0xAA}, {0x21, 0xBB}, {0x22, 0xCC}};
    CHECK(!Cam::writeRegs(table2, 3) && reg[0x20] == 0xAA && reg[0x22] == 0x33, "table stops at the first failure");
    g_wireFailReg = -1;
    CHECK(!Cam::writeRegs(nullptr, 1), "null table rejected");

    g_takeResult = pdFALSE;
    const unsigned reqBefore = g_wireRequestCount, wrBefore = g_wireWritesLen;
    v = 0x55;
    CHECK(!Cam::readReg(0x12, v) && v == 0x55 && g_wireRequestCount == reqBefore, "lock failure performs no read");
    CHECK(!Cam::writeReg(0x12, 0x01) && g_wireWritesLen == wrBefore, "lock failure performs no write");
    g_takeResult = pdTRUE;

    // A pause entry is not sent to the camera.
    const unsigned wr0 = g_wireWritesLen;
    const Cam::RegValue table3[] = {{0x30, 0x01}, {Cam::REG_DELAY, 0x05}, {0x31, 0x02}};
    CHECK(Cam::writeRegs(table3, 3) && g_wireWritesLen == wr0 + 2 && reg[0xFE] == 0, "pause entry is not written");
    const unsigned wr1 = g_wireWritesLen;
    CHECK(Cam::configure() && g_wireWritesLen > wr1 && g_wireWrites[wr1].reg == 0x09 && g_wireWrites[wr1].value == 0x13,
          "start-up settings begin by idling the camera");
    CHECK(reg[0x12] == 0x04 && reg[0x3A] == 0x05, "start-up settings select RGB565");
    CHECK(Cam::stream(true) && reg[0x09] == 0x03 && Cam::stream(false) && reg[0x09] == 0x13, "stream on and off");

    CHECK(reg[0x1E] == 0x39 && Cam::setMirrorFlip(false, false) && reg[0x1E] == 0x09, "mirror and flip off keeps the other bits");
    CHECK(Cam::setMirrorFlip(true, false) && reg[0x1E] == 0x29 && Cam::setMirrorFlip(false, true) && reg[0x1E] == 0x19, "mirror and flip set separately");

    // Picture decoding: headers checked and removed, pixels kept in order.
    CHECK(Cam::RAW_FRAME_BYTES == 155524 && Cam::FRAME_BYTES == 153600, "picture sizes");
    std::vector<uint8_t> raw(Cam::RAW_FRAME_BYTES), out(Cam::FRAME_BYTES, 0);
    auto build = [&]() {
        size_t at = 0;
        raw[at++] = 0xFF; raw[at++] = 0xFF; raw[at++] = 0xFF; raw[at++] = 0x00;
        for (int y = 0; y < Cam::HEIGHT; ++y) {
            raw[at++] = 0xFF; raw[at++] = 0xFF; raw[at++] = 0xFF; raw[at++] = 0x40;
            raw[at++] = (uint8_t)y; raw[at++] = (uint8_t)(y >> 8);
            for (int x = 0; x < Cam::WIDTH * 2; ++x) raw[at++] = (uint8_t)(y * 7 + x);
        }
    };
    auto pixelsOk = [&](const uint8_t* p) {
        for (int y = 0; y < Cam::HEIGHT; ++y)
            for (int x = 0; x < Cam::WIDTH * 2; ++x)
                if (p[(size_t)y * Cam::WIDTH * 2 + x] != (uint8_t)(y * 7 + x)) return false;
        return true;
    };
    build();
    CHECK(Cam::decodeFrame(raw.data(), raw.size(), out.data()) && pixelsOk(out.data()), "picture decoded");
    CHECK(!Cam::decodeFrame(raw.data(), raw.size() - 1, out.data()), "short picture rejected");
    CHECK(!Cam::decodeFrame(nullptr, raw.size(), out.data()) && !Cam::decodeFrame(raw.data(), raw.size(), nullptr), "null buffers rejected");
    raw[3] = 0x40;
    CHECK(!Cam::decodeFrame(raw.data(), raw.size(), out.data()), "bad picture header rejected");
    build(); raw[4 + 100 * 486 + 3] = 0x00;
    CHECK(!Cam::decodeFrame(raw.data(), raw.size(), out.data()), "bad line header rejected");
    build();
    CHECK(Cam::decodeFrame(raw.data(), raw.size(), raw.data()) && pixelsOk(raw.data()), "picture decoded in place");
    return g_failures ? 1 : 0;
}
