// SPDX-License-Identifier: MIT
#include <Wire.h>
#include <freertos/semphr.h>
#include "testutil.h"
#include "../../src/board/ws_c5_28/ws_c5_28_battery.h"
namespace B = WsC5_28::Battery;
int main() {
    B::Reading r{123,456};
    CHECK(!B::read(r), "read before begin rejected");
    B::begin();
    const uint8_t value[] = {0x9C, 0x01}; // 412 -> 3987 mV
    mockWireSetResponse(2,value,2);
    g_takeResult=pdFALSE;
    const int before=g_wireWriteCount;
    CHECK(!B::read(r) && g_wireWriteCount==before, "lock failure performs no transaction");
    g_takeResult=pdTRUE;
    g_wireEndTransStatus=2;
    CHECK(!B::read(r), "register-select NACK rejected");
    CHECK(r.raw==123 && r.voltageMv==456, "failure preserves caller reading");
    g_wireEndTransStatus=0;
    mockWireSetResponse(1,value,1);
    CHECK(!B::read(r), "short I2C response rejected");
    mockWireSetResponse(2,value,1);
    CHECK(!B::read(r), "missing buffered byte rejected");
    const uint8_t bad[]={0,4};
    mockWireSetResponse(2,bad,2);
    CHECK(!B::read(r), "out-of-range ADC rejected");
    mockWireSetResponse(2,value,2);
    const int locks=g_giveCount, reopens=g_wireEndCount, writes=g_wireWriteCount;
    CHECK(B::read(r) && r.raw==412 && r.voltageMv==3987, "little-endian nominal voltage conversion");
    CHECK(g_giveCount-locks==8 && g_wireEndCount-reopens==8, "each sample locks/releases and reopens bus");
    CHECK(g_wireWriteCount-writes==8, "only register-selection bytes written; no output writes");
    const uint8_t zero[]={0,0};mockWireSetResponse(2,zero,2);
    CHECK(B::read(r) && r.raw==0 && r.voltageMv==0, "zero is a valid ADC reading, not proof of absent battery");
    const uint8_t max[]={255,3};mockWireSetResponse(2,max,2);
    CHECK(B::read(r) && r.raw==1023 && r.voltageMv==9900, "full scale does not overflow");
    return g_failures ? 1 : 0;
}
