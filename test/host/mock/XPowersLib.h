// SPDX-License-Identifier: MIT
// Host-test stub of the XPowersLib AXP2101 surface the battery driver uses.
// Every read method bumps g_pmuReadCalls so a test can assert the driver made
// no PMU reads when the bus lock was not acquired.
#pragma once
#include <stdint.h>
#include "Wire.h"

#define AXP2101_SLAVE_ADDRESS 0x34

extern int g_pmuBeginResult;   // value begin() returns (nonzero = present)
extern int g_pmuReadCalls;     // count of read-method calls

struct XPowersPMU {
    bool begin(MockWire&, uint8_t, int, int) { return g_pmuBeginResult != 0; }
    void enableBattDetection()        {}
    void enableBattVoltageMeasure()   {}
    void enableVbusVoltageMeasure()   {}
    void enableSystemVoltageMeasure() {}
    bool     isBatteryConnect() { g_pmuReadCalls++; return true;  }
    int      getBatteryPercent(){ g_pmuReadCalls++; return 77;    }
    uint16_t getBattVoltage()   { g_pmuReadCalls++; return 4100;  }
    bool     isCharging()       { g_pmuReadCalls++; return true;  }
    bool     isVbusIn()         { g_pmuReadCalls++; return true;  }
    uint16_t getVbusVoltage()   { g_pmuReadCalls++; return 5000;  }
};
