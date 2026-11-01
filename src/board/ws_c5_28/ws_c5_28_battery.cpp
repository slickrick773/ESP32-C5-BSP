// SPDX-License-Identifier: MIT
// Protocol reference: Waveshare Board_IO.cpp / Board_IO.h, commit
// 0196dcb7e601d6faa1c33785a71f6b0c35b532e6 (see battery_28 README).
#include "ws_c5_28_battery.h"
#include "ws_c5_28_pins.h"
#include "../../chip/esp32c5_i2c_bus.h"
#include <Arduino.h>
#include <Wire.h>

namespace WsC5_28 {
namespace Battery {
static bool initialized = false;
void begin() {
    Esp32C5::I2cBus::begin(I2C_SDA, I2C_SCL, I2C_HZ);
    initialized = true;
}
bool read(Reading& result) {
    if (!initialized) return false;
    uint32_t sum = 0;
    for (unsigned i = 0; i < 8; ++i) {
        {
            Esp32C5::I2cBus::Guard lock(100);
            if (!lock) return false;
            Esp32C5::I2cBus::reopen();
            Esp32C5::I2cBus::TimeoutGuard timeout(30);
            Wire.beginTransmission(EXIO_ADDR);
            Wire.write(uint8_t(0x06));
            if (Wire.endTransmission(false) != 0) return false;
            if (Wire.requestFrom((int)EXIO_ADDR, 2) != 2 || Wire.available() < 2)
                return false;
            const int lo = Wire.read();
            const int hi = Wire.read();
            if (lo < 0 || hi < 0) return false;
            const uint16_t sample = uint16_t(lo) | (uint16_t(hi) << 8);
            if (sample > 1023) return false;
            sum += sample;
        }
        if (i < 7) delay(5);
    }
    const uint16_t raw = (sum + 4) / 8;
    result = {raw, uint16_t((uint32_t(raw) * 9900 + 511) / 1023)};
    return true;
}
}  // namespace Battery
}  // namespace WsC5_28
