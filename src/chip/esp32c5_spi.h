// SPDX-License-Identifier: MIT
// ESP32-C5 SPI2 startup helpers shared by board adapters.
#pragma once

namespace Esp32C5 {
namespace Spi2 {

// Restore SPI2 peripheral registers to a known state before any display or SD
// driver claims the bus. This is especially important after a software reset.
void resetPeripheral();

}  // namespace Spi2
}  // namespace Esp32C5
