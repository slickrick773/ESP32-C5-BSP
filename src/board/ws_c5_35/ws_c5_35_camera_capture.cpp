// SPDX-License-Identifier: MIT
// See ws_c5_35_camera.h. Picture reception through the ESP32-C5 parallel-input
// peripheral (PARLIO RX): the camera's clock samples two data lines while its
// frame signal is low. One picture is larger than the peripheral can count,
// so the end of the frame signal is caught with a pin interrupt and reported
// to the driver, as Espressif's own camera code does.
#if defined(ARDUINO_ARCH_ESP32)
#include "soc/soc_caps.h"
#if SOC_PARLIO_SUPPORTED
#include "ws_c5_35_camera.h"
#include "ws_c5_35_pins.h"
#include <Arduino.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/parlio_rx.h"
#include "esp_private/parlio_rx_private.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace WsC5_35 {
namespace Camera {

static parlio_rx_unit_handle_t      s_unit = nullptr;
static parlio_rx_delimiter_handle_t s_delim = nullptr;
static SemaphoreHandle_t            s_done = nullptr;
static uint8_t*                     s_raw = nullptr;
static size_t                       s_rawSize = 0;
static volatile uint32_t            s_recvBytes = 0;
static CaptureStats                 s_stats = {};
static bool                         s_isrAdded = false;

static bool IRAM_ATTR onDone(parlio_rx_unit_handle_t, const parlio_rx_event_data_t* e, void*) {
    BaseType_t woken = pdFALSE;
    s_recvBytes = (uint32_t)e->recv_bytes;
    xSemaphoreGiveFromISR(s_done, &woken);
    return woken == pdTRUE;
}
static void IRAM_ATTR onFrameEnd(void*) {
    bool yield = false;
    if (s_unit) parlio_rx_unit_trigger_fake_eof(s_unit, &yield);
    if (yield) portYIELD_FROM_ISR();
}

CaptureStats captureStats() { return s_stats; }

void captureEnd() {
    if (s_isrAdded) { gpio_isr_handler_remove((gpio_num_t)CAM_VSYNC); s_isrAdded = false; }
    if (s_unit)  { parlio_rx_unit_disable(s_unit); }
    if (s_delim) { parlio_del_rx_delimiter(s_delim); s_delim = nullptr; }
    if (s_unit)  { parlio_del_rx_unit(s_unit); s_unit = nullptr; }
    if (s_done)  { vSemaphoreDelete(s_done); s_done = nullptr; }
    if (s_raw)   { heap_caps_free(s_raw); s_raw = nullptr; }
}

bool captureBegin() {
    if (s_unit) return true;
    s_stats = {};
    s_rawSize = (RAW_FRAME_BYTES + 63) & ~(size_t)63;
    s_raw = (uint8_t*)heap_caps_aligned_alloc(64, s_rawSize, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!s_raw) s_raw = (uint8_t*)heap_caps_aligned_alloc(64, s_rawSize, MALLOC_CAP_SPIRAM);
    s_done = xSemaphoreCreateBinary();
    if (!s_raw || !s_done) { captureEnd(); return false; }

    gpio_config_t pins = {};
    pins.pin_bit_mask = (1ULL << CAM_D0) | (1ULL << CAM_D1) | (1ULL << CAM_PCLK) | (1ULL << CAM_VSYNC);
    pins.mode = GPIO_MODE_INPUT;
    pins.pull_up_en = GPIO_PULLUP_ENABLE;
    if (gpio_config(&pins) != ESP_OK) { captureEnd(); return false; }
    const esp_err_t isr = gpio_install_isr_service(0);
    if (isr != ESP_OK && isr != ESP_ERR_INVALID_STATE) { captureEnd(); return false; }
    // The frame signal is low while a picture is sent; its rising edge ends it.
    if (gpio_set_intr_type((gpio_num_t)CAM_VSYNC, GPIO_INTR_POSEDGE) != ESP_OK ||
        gpio_isr_handler_add((gpio_num_t)CAM_VSYNC, onFrameEnd, nullptr) != ESP_OK) { captureEnd(); return false; }
    s_isrAdded = true;

    parlio_rx_unit_config_t cfg = {};
    cfg.trans_queue_depth = 1;
    cfg.max_recv_size = s_rawSize;
    cfg.data_width = 2;
    cfg.clk_src = PARLIO_CLK_SRC_EXTERNAL;
    cfg.ext_clk_freq_hz = 24000000;
    cfg.exp_clk_freq_hz = 24000000;
    cfg.clk_in_gpio_num = (gpio_num_t)CAM_PCLK;
    cfg.clk_out_gpio_num = (gpio_num_t)-1;
    cfg.valid_gpio_num = (gpio_num_t)CAM_VSYNC;
    for (int i = 0; i < PARLIO_RX_UNIT_MAX_DATA_WIDTH; ++i) cfg.data_gpio_nums[i] = (gpio_num_t)-1;
    cfg.data_gpio_nums[0] = (gpio_num_t)CAM_D0;
    cfg.data_gpio_nums[1] = (gpio_num_t)CAM_D1;
    if (parlio_new_rx_unit(&cfg, &s_unit) != ESP_OK) { s_unit = nullptr; captureEnd(); return false; }

    parlio_rx_level_delimiter_config_t d = {};
    d.valid_sig_line_id = PARLIO_RX_UNIT_MAX_DATA_WIDTH - 1;
    d.sample_edge = PARLIO_SAMPLE_EDGE_POS;
    d.bit_pack_order = PARLIO_BIT_PACK_ORDER_MSB;
    d.eof_data_len = 0;
    d.timeout_ticks = 0;
    d.flags.active_low_en = 1;
    if (parlio_new_rx_level_delimiter(&d, &s_delim) != ESP_OK) { s_delim = nullptr; captureEnd(); return false; }

    parlio_rx_event_callbacks_t cbs = {};
    cbs.on_receive_done = onDone;
    if (parlio_rx_unit_register_event_callbacks(s_unit, &cbs, nullptr) != ESP_OK ||
        parlio_rx_unit_enable(s_unit, true) != ESP_OK) { captureEnd(); return false; }
    return true;
}

bool captureFrame(uint8_t* out, uint32_t timeoutMs) {
    if (!s_unit || !out) return false;
    const uint32_t start = millis();
    parlio_receive_config_t rc = {};
    rc.delimiter = s_delim;
    for (;;) {
        const uint32_t used = millis() - start;
        if (used >= timeoutMs) return false;
        xSemaphoreTake(s_done, 0);
        if (parlio_rx_unit_receive(s_unit, s_raw, s_rawSize, &rc) != ESP_OK) return false;
        if (xSemaphoreTake(s_done, pdMS_TO_TICKS(timeoutMs - used)) != pdTRUE) return false;
        ++s_stats.received;
        s_stats.lastBytes = s_recvBytes;
        memcpy(s_stats.lastHead, s_raw, sizeof(s_stats.lastHead));
        if (s_recvBytes < RAW_FRAME_BYTES) { ++s_stats.badLength; continue; }
        if (decodeFrame(s_raw, RAW_FRAME_BYTES, out)) return true;
        ++s_stats.badHeader;
    }
}

}  // namespace Camera
}  // namespace WsC5_35
#endif  // SOC_PARLIO_SUPPORTED
#endif  // ARDUINO_ARCH_ESP32
