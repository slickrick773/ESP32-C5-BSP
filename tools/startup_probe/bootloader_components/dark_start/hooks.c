// SPDX-License-Identifier: MIT
#include "early_i2c.h"
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"
#include "soc/gpio_sig_map.h"

#if !CONFIG_IDF_TARGET_ESP32C5
#error This diagnostic bootloader is only for ESP32-C5.
#endif
#if CONFIG_BOOTLOADER_OFFSET_IN_FLASH != 0x2000 || CONFIG_PARTITION_TABLE_OFFSET != 0x8000
#error Preserve the installed flash layout.
#endif
#if !CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE || !CONFIG_BOOTLOADER_WDT_ENABLE
#error Preserve rollback and bootloader watchdog protection.
#endif
#define SDA 27
#define SCL 26

static void sda(bool release) { gpio_ll_set_level(&GPIO, SDA, release); }
static void scl(bool release) { gpio_ll_set_level(&GPIO, SCL, release); }
static bool read_sda(void) { return gpio_ll_get_level(&GPIO, SDA); }
static bool read_scl(void) { return gpio_ll_get_level(&GPIO, SCL); }

static void prepare_pin(unsigned pin) {
    gpio_ll_output_disable(&GPIO, pin);
    gpio_ll_set_level(&GPIO, pin, 1);
    gpio_ll_od_enable(&GPIO, pin);
    esp_rom_gpio_pad_select_gpio(pin);
    gpio_ll_input_enable(&GPIO, pin);
    esp_rom_gpio_pad_pullup_only(pin);
    esp_rom_gpio_connect_out_signal(pin, SIG_GPIO_OUT_IDX, false, false);
}

void bootloader_hooks_include(void) {}

// AFTER init deliberately: BSS, clocks, memory protection and console are ready.
// No before-init hook. Do not change watchdog, partitions, OTA state or eFuses.
void bootloader_after_init(void) {
    const early_i2c_io io = {sda, scl, read_sda, read_scl, esp_rom_delay_us};
    prepare_pin(SDA);
    prepare_pin(SCL);
    enum early_i2c_result result = EARLY_NACK;
    unsigned attempts = 0;
    for (; attempts < 3; ) {
        ++attempts;
        result = early_pwm_off(&io);
        if (result != EARLY_NACK) break;
        if (attempts < 3) esp_rom_delay_us(1000);
    }
    // Release pads even on failure; Arduino later selects its own I2C matrix.
    gpio_ll_output_disable(&GPIO, SDA);
    gpio_ll_output_disable(&GPIO, SCL);
    esp_rom_printf("[dark-start] result=%u attempts=%u\n", result, attempts);
}
