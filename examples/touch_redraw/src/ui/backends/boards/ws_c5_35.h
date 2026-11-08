#pragma once
#include "sample_ui/core.h"
#include "input/contact_tracker.h"
#include <Arduino.h>
#include <board/ws_c5_35/ws_c5_35_board.h>
// Opt in only with a dependency snapshot providing begin(initialBacklight).
// The Waveshare example enables this for its published BSP pin.
// Leave it off only for consumers still using the legacy API.
#ifndef SAMPLE_UI_BSP_STARTUP_API
#define SAMPLE_UI_BSP_STARTUP_API 0
#endif
#ifndef SAMPLE_UI_BSP_CLASSIFIED_TOUCH
#define SAMPLE_UI_BSP_CLASSIFIED_TOUCH 0
#endif
#if !SAMPLE_UI_BSP_STARTUP_API
#include <esp_private/periph_ctrl.h>
#include <hal/spi_ll.h>
#endif
namespace sample_ui::boards {
class WsC535 {
public:
    static constexpr Size landscape{480,320};
    static constexpr int rotation = 1;
    static constexpr int touchInterrupt = 3;
    static constexpr bool inverted = WsC5_35::PANEL_NEEDS_INVERSION;
    bool begin() {
#if SAMPLE_UI_BSP_STARTUP_API
        // The BSP owns one-time SPI2 reset, deselection and dark initialization.
        // Do not prepare/reset SPI again after a graphics driver claims it.
        if (!WsC5_35::begin(0)) return false;
#else
        // C5 Arduino does not reset SPI2 after a soft reset. Display and SD share it.
        PERIPH_RCC_ATOMIC() {
            spi_ll_enable_bus_clock(SPI2_HOST,true);
            spi_ll_reset_register(SPI2_HOST);
            spi_ll_enable_clock(SPI2_HOST,true);
        }
        // Deselect both devices before the graphics library starts shared SPI.
        pinMode(WsC5_35::LCD_CS,OUTPUT); digitalWrite(WsC5_35::LCD_CS,HIGH);
        pinMode(WsC5_35::SD_CS,OUTPUT); digitalWrite(WsC5_35::SD_CS,HIGH);
        // Mask panel memory while touch and graphics are still initializing.
        // BSP expander startup briefly enables PWM before returning; dim it
        // immediately, before the longer touch probe or panel init starts.
        if (!WsC5_35::Expander::begin()) return false;
        WsC5_35::Expander::backlight(0);
        WsC5_35::Touch::begin();
#endif
        WsC5_35::Touch::beginInt(touchInterrupt);
        return true;
    }
    void show() { WsC5_35::Expander::backlight(255); }
#if SAMPLE_UI_BSP_CLASSIFIED_TOUCH
    static constexpr uint32_t sampleIntervalMs=12;
    bool touchEvent(uint32_t now,uint32_t generation,PointerEvent& event) {
        using input::ContactStatus;
        auto status=ContactStatus::Unavailable;
        Point point{};
        if (!sampled_ || uint32_t(now-lastSampleAt_)>=sampleIntervalMs) {
            sampled_=true;lastSampleAt_=now;
            const auto sample=WsC5_35::Touch::readSample();
            switch(sample.status) {
            case WsC5_35::Touch::SampleStatus::Contact:
                point={static_cast<int>(sample.y),WsC5_35::PANEL_W-1-static_cast<int>(sample.x)};
                status=contains({0,0,landscape.width,landscape.height},point.x,point.y)
                    ? ContactStatus::Contact : ContactStatus::Error;
                break;
            case WsC5_35::Touch::SampleStatus::Released:status=ContactStatus::Released;break;
            case WsC5_35::Touch::SampleStatus::Unavailable:break;
            case WsC5_35::Touch::SampleStatus::Error:status=ContactStatus::Error;break;
            default:status=ContactStatus::Error;break;
            }
        }
        // Advance generation and stale-contact handling even between bus reads.
        return contact_.update(status,point,now,generation,event);
    }
#endif
    // Native touch is portrait. This mapping matches display rotation 1.
    bool touch(Point& p) {
        uint16_t x=0,y=0;
        if (!WsC5_35::Touch::poll(x,y)) return false;
        p={static_cast<int>(y),WsC5_35::PANEL_W-1-static_cast<int>(x)};
        return contains({0,0,landscape.width,landscape.height},p.x,p.y);
    }
private:
#if SAMPLE_UI_BSP_CLASSIFIED_TOUCH
    input::ContactTracker contact_;
    bool sampled_=false;
    uint32_t lastSampleAt_=0;
#endif
};
}
