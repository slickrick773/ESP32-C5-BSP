#pragma once
#include "sample_ui/core.h"
#include <TFT_eSPI.h>
namespace sample_ui::graphics {
class TftEsPiDisplay final : public Display {
public:
    explicit TftEsPiDisplay(TFT_eSPI& tft) : tft_(tft) {}
    void begin(int rotation, bool inverted) {
        tft_.init(); tft_.setRotation(rotation); tft_.invertDisplay(inverted);
    }
    bool write(Rect rect, const uint16_t* pixels) override {
        if (rect.w <= 0 || rect.h <= 0 || !pixels) return false;
        // Core pixels are host-endian RGB565; TFT_eSPI needs MSB-first on SPI.
        const bool previous=tft_.getSwapBytes();
        tft_.setSwapBytes(true);
        tft_.pushImage(rect.x,rect.y,rect.w,rect.h,const_cast<uint16_t*>(pixels));
        tft_.setSwapBytes(previous);
        return true;
    }
private:
    TFT_eSPI& tft_;
};
}
