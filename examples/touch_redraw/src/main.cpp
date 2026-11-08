// SPDX-License-Identifier: MIT
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <cstdio>
#include "boards/ws_c5_35.h"
#include "graphics/tft_espi_display.h"
#include "sample_ui/presentation.h"
#include "sample_ui/scroll.h"
#include "sample_ui/slick.h"

namespace {
using namespace sample_ui;

enum : Action { Toggle = 1, Count = 2 };
constexpr ListRow rows[] = {
    {1, "DISPLAY", "ST7796", 101},
    {2, "TOUCH", "FT6336", 102},
    {3, "PROCESSOR", "ESP32-C5", 103},
    {4, "COLOR", "RGB565", 104},
    {5, "BACKLIGHT", "FIRST FRAME", 105},
    {6, "RENDERING", "STRIP BUFFER", 106},
    {7, "TOUCH INPUT", "DRAG TO SCROLL", 107},
    {8, "REDRAW", "CHANGED REGION", 108},
    {9, "SAMPLE DATA", "STATIC", 109},
    {10, "END OF LIST", "", 110},
};
constexpr auto rowCount = sizeof(rows) / sizeof(rows[0]);

boards::WsC535 board;
TFT_eSPI tft;
graphics::TftEsPiDisplay display(tft);
Input touchInput;
ScrollInput scrollInput(touchInput);
char countLabel[24] = "COUNT 0";
ListModel model{"TOUCH AND REDRAW", rows, rowCount, 0, 0,
                {{{"TOGGLE OFF", Toggle}, {countLabel, Count}}}, 0};
ListScreen screen(model);
uint16_t strip[480 * 8];
Renderer renderer(strip, sizeof(strip) / sizeof(strip[0]));
bool ready = false;
bool revealed = false;
bool enabled = false;
uint32_t count = 0;
int pendingOffset = -1;

void repaint(Rect damage = {}) {
    ready = renderer.begin(screen, board.landscape, slick, 8, damage);
}

void apply(Action action) {
    if (action == Toggle) {
        enabled = !enabled;
        model.footer[0].label = enabled ? "TOGGLE ON" : "TOGGLE OFF";
    } else if (action == Count) {
        ++count;
        std::snprintf(countLabel, sizeof(countLabel), "COUNT %lu",
                      static_cast<unsigned long>(count));
    } else {
        model.selected = static_cast<ItemId>(action - 100);
    }
}
}

void setup() {
    Serial.begin(115200);
    if (!board.begin()) {
        Serial.println("Board initialization failed");
        return;
    }
    display.begin(board.rotation, board.inverted);
    repaint();
}

void loop() {
    if (!ready) {
        delay(10);
        return;
    }

    const auto layout = screen.layout(board.landscape);
    PointerEvent event{};
    if (board.touchEvent(millis(), touchInput.generation(), event)) {
        const auto result = scrollInput.dispatch(
            event, screen.hits(board.landscape), layout.viewport,
            model.scrollOffset, listScrollLimit(board.landscape, rowCount), revealed);
        if (result.scrolling) pendingOffset = result.offset;
        if (result.action == Toggle || result.action == Count ||
            (result.action && !renderer.busy())) {
            const bool interrupted = renderer.busy();
            renderer.cancel();
            apply(result.action);
            const bool footer = result.action == Toggle || result.action == Count;
            repaint(interrupted ? Rect{} : footer ? layout.frame.footer : layout.viewport);
        }
    }

    if (ready && renderer.busy() && renderer.step(display) == Step::Complete && !revealed) {
        board.show();
        revealed = true;
    }
    if (ready && !renderer.busy() && pendingOffset >= 0) {
        const int offset = pendingOffset;
        pendingOffset = -1;
        if (offset != model.scrollOffset) {
            model.scrollOffset = offset;
            repaint(layout.viewport);
        }
    }
    delay(1);
}
