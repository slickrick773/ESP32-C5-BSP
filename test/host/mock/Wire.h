// SPDX-License-Identifier: MIT
// Host-test mock of the Arduino TwoWire surface used by the BSP. Read responses
// and the endTransmission status are test-controlled; calls are counted so a
// test can assert that no bus op happened when a lock was not acquired.
#pragma once
#include <stdint.h>
#include <stddef.h>

// Test-controlled read response (defined in mock.cpp).
extern int     g_wireReqReturn;        // value requestFrom() returns
extern uint8_t g_wireReadBuf[64];      // bytes served by read()
extern int     g_wireReadLen;          // number of valid bytes in g_wireReadBuf
extern int     g_wireReadPos;          // read cursor (reset each requestFrom)
extern uint8_t g_wireEndTransStatus;   // value endTransmission() returns (0 = ACK)

// Call counters (defined in mock.cpp).
extern int g_wireBeginCount;   // Wire.begin()
extern int g_wireEndCount;     // Wire.end()
extern int g_wireWriteCount;   // Wire.write()

struct MockWire {
    void     begin(int sda, int scl, uint32_t hz);
    void     begin(int sda, int scl);
    void     begin();
    void     end();
    void     setBufferSize(size_t n);
    uint16_t getTimeOut();
    void     setTimeOut(uint16_t ms);
    void     beginTransmission(uint8_t addr);
    size_t   write(uint8_t b);
    size_t   write(const uint8_t* b, size_t n);
    uint8_t  endTransmission(bool stop = true);
    int      requestFrom(int addr, int qty);
    int      available();
    int      read();
};
extern MockWire Wire;

// Set the next read response: requestFrom() returns `reqRet`, read()/available()
// serve `n` bytes from `bytes`.
void mockWireSetResponse(int reqRet, const uint8_t* bytes, int n);

// Optional register-map mode for codec/expander tests; off for packet tests.
extern bool g_wireRegisterMode;
extern uint8_t g_wireRegisters[128][256];
extern int g_wireFailReg;  // NACK writes to this register; -1 disables
extern bool g_wireCommandDevice; // register mode: two-byte writes are commands, not register writes
extern int g_wireShortRead; // register mode: multi-byte reads return one byte short

struct MockWireWrite { uint8_t addr, reg, value; uint8_t value2 = 0; uint8_t bytes = 0; };  // bytes counts reg + data
extern MockWireWrite g_wireWrites[256];
extern unsigned g_wireWritesLen;
extern unsigned g_wireRequestCount;
