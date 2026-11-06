// SPDX-License-Identifier: MIT
#include "esp32c5_spi.h"
#include <esp_private/periph_ctrl.h>
#include <hal/spi_ll.h>

namespace Esp32C5 {
namespace Spi2 {

void resetPeripheral() {
    PERIPH_RCC_ATOMIC() {
        spi_ll_enable_bus_clock(SPI2_HOST, true);
        spi_ll_reset_register(SPI2_HOST);
        spi_ll_enable_clock(SPI2_HOST, true);
    }
}

}  // namespace Spi2
}  // namespace Esp32C5
