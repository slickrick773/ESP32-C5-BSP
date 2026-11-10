#pragma once
#include <cstddef>
#include <cstdint>

namespace sample_ui {
struct Size { int width, height; };
struct Point { int x, y; };
struct Rect { int x = 0, y = 0, w = 0, h = 0; };
bool contains(Rect r, int x, int y);
Rect intersect(Rect a, Rect b);
// Supported logical dimensions: 120..4096 on each axis.
bool supported(Size size);
struct Chrome { Rect header, body, footer; int gap; };
Chrome chrome(Size size);
Rect cell(Rect area, int index, int columns, int rows, int gap);

constexpr uint16_t rgb565(uint32_t rgb) {
    return static_cast<uint16_t>(((rgb >> 8) & 0xf800) | ((rgb >> 5) & 0x07e0) | ((rgb >> 3) & 0x001f));
}
struct Theme {
    uint16_t background, panel, border, text, accent;
    uint16_t muted, bright, accentHigh, success, warning, danger;
    uint16_t onAccent = 0, disabled = 0, unavailable = 0;
};

enum class Role { Background, Panel, Border, Text, Accent, Muted, Bright,
                  AccentHigh, Success, Warning, Danger, OnAccent, Disabled, Unavailable, Count };
uint16_t color(const Theme& theme, Role role);
// Contrast gates: body 4.5:1, muted/disabled/unavailable 3:1, accent label 4.5:1.
bool validateTheme(const Theme& theme);
// Derived inactive render state, not a separately bundled/registered theme.
Theme modalBackdropTheme(const Theme& theme);
enum class Availability { Available, Disabled, Unavailable, Hidden };
using Capabilities = uint32_t; // Application-defined bits; core assigns no product meaning.
Availability availability(Capabilities supplied, Capabilities required,
                          bool enabled = true, bool hideWhenMissing = false);

class Painter {
public:
    virtual ~Painter() = default;
    virtual void fill(Rect rect, uint16_t color) = 0;
    virtual void text(Rect rect, const char* label, uint16_t color, int scale = 1);
    void outline(Rect rect, uint16_t color);
    void panel(Rect rect, const Theme& theme);
    void button(Rect rect, const char* label, const Theme& theme, bool selected = false, Availability state = Availability::Available);
};
// Composition clip. Text uses the same built-in bitmap layout as RasterPainter;
// clipping crops glyph pixels rather than recentering text in a smaller box.
class ClippedPainter final : public Painter {
public:
    ClippedPainter(Painter& target,Rect clip):target_(target),clip_(clip){}
    void fill(Rect r,uint16_t color) override {target_.fill(intersect(r,clip_),color);}
private:
    Painter& target_;Rect clip_;
};
// No allocation. Coordinates are global; writes are clipped to this strip.
class RasterPainter final : public Painter {
public:
    RasterPainter(uint16_t* pixels, int width, int top, int rows);
    void fill(Rect rect, uint16_t color) override;
    void text(Rect rect, const char* label, uint16_t color, int scale = 1) override;
private:
    uint16_t* pixels_;
    Rect clip_;
};

using ItemId = uint32_t; // 0 means no keyed item; identities belong to the application.
using Action = uint32_t; // 0 means no action; IDs belong to the application.
struct Hit { Rect rect; Action action = 0; bool enabled = true; ItemId identity = 0; };
class HitList {
public:
    static constexpr std::size_t capacity = 24;
    bool add(Rect rect, Action action, bool enabled = true, ItemId identity = 0);
    const Hit* find(Point point) const;
    Action at(Point point) const; // top hit wins; disabled hits block controls below
private:
    Hit hits_[capacity]{};
    std::size_t count_ = 0;
};
// One descriptor supplies both drawing and hit geometry. Hidden draws nothing;
// disabled and unavailable draw feedback but block input to lower controls.
struct Control {
    Rect rect; const char* label; Action action;
    Availability state = Availability::Available;
    bool selected = false;
    ItemId identity = 0;
    void paint(Painter& painter, const Theme& theme) const;
    bool addHit(HitList& hits) const;
};
enum class PointerPhase { Down, Move, Up, Cancel };
struct PointerEvent { PointerPhase phase; Point point; uint32_t generation; };
class Input {
public:
    void reset(); // screen, layout, theme or active-hit changes cancel gestures
    uint32_t generation() const { return generation_; }
    Action dispatch(PointerEvent event, const HitList& hits, bool ready);
private:
    uint32_t generation_ = 0;
    Action captured_ = 0;
    Rect capturedRect_{};
    ItemId capturedIdentity_ = 0;
};
// Physical viewport and logical size share one axis-aligned scaling transform.
// Rotation/calibration belong to the hardware adapter, before this mapping.
struct Viewport {
    Rect physical;
    Size logical;
    bool map(Point physicalPoint, Point& logicalPoint) const;
};
class InputSource {
public:
    virtual ~InputSource() = default;
    virtual bool poll(PointerEvent& event) = 0;
};
class Display {
public:
    virtual ~Display() = default;
    // Synchronous: consume/copy packed host-endian RGB565 before returning.
    // False leaves this slice pending for retry. Adapter owns bus/byte order.
    virtual bool write(Rect rect, const uint16_t* pixels) = 0;
};
class Screen {
public:
    virtual ~Screen() = default;
    virtual void paint(Painter& painter, Size size, const Theme& theme) const = 0;
    virtual HitList hits(Size size) const = 0;
};
// Caller owns both screens and keeps their models stable for a render. Modal
// paint subdues the base and overlays a normal modal; only modal hits are exposed.
// Opening/closing changes the full viewport, requiring full invalidation and
// Input::reset().
class ModalScreen final : public Screen {
public:
    ModalScreen(const Screen& base, const Screen& modal) : base_(base), modal_(modal) {}
    void paint(Painter& p, Size s, const Theme& t) const override {
        const Theme backdrop=modalBackdropTheme(t);
        base_.paint(p,s,backdrop);modal_.paint(p,s,t);
    }
    HitList hits(Size s) const override { return modal_.hits(s); }
private:
    const Screen& base_;
    const Screen& modal_;
};
enum class Step { Idle, More, Complete, Retry };
// Caller-owned strip memory, no heap or global state. Paint is pure and must
// fully compose a scene; screen and model stay immutable until complete.
class Renderer {
public:
    Renderer(uint16_t* buffer, std::size_t pixels) : buffer_(buffer), capacity_(pixels) {}
    bool begin(const Screen& screen, Size size, const Theme& theme, int rowsPerStep = 8, Rect damage = {});
    Step step(Display& display);
    void cancel();
    bool busy() const { return screen_ != nullptr; }
private:
    uint16_t* buffer_;
    std::size_t capacity_;
    const Screen* screen_ = nullptr;
    Size size_{};
    Theme theme_{};
    Rect damage_{};
    int next_ = 0, rows_ = 0;
};
}
