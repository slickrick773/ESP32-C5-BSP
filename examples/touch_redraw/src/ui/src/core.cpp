#include "sample_ui/core.h"
#include "font5x7.h"
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstring>

namespace sample_ui {
bool contains(Rect r, int x, int y) {
    return r.w > 0 && r.h > 0 && x >= r.x && y >= r.y &&
        int64_t(x) < int64_t(r.x) + r.w && int64_t(y) < int64_t(r.y) + r.h;
}
Rect intersect(Rect a, Rect b) {
    const int x = std::max(a.x, b.x), y = std::max(a.y, b.y);
    const int64_t right = std::min(int64_t(a.x) + std::max(0, a.w), int64_t(b.x) + std::max(0, b.w));
    const int64_t bottom = std::min(int64_t(a.y) + std::max(0, a.h), int64_t(b.y) + std::max(0, b.h));
    return {x, y, static_cast<int>(std::max<int64_t>(0, right-x)), static_cast<int>(std::max<int64_t>(0, bottom-y))};
}
bool supported(Size s) { return s.width >= 120 && s.height >= 120 && s.width <= 4096 && s.height <= 4096; }
Chrome chrome(Size s) {
    if (!supported(s)) return {};
    const int gap = std::max(4, std::min(s.width, s.height) / 40);
    const int band = std::max(22, std::min(44, s.height / 5));
    return {{0,0,s.width,band}, {gap,band+gap,s.width-2*gap,s.height-2*band-2*gap},
            {gap,s.height-band,s.width-2*gap,band-gap}, gap};
}
Rect cell(Rect a, int index, int cols, int rows, int gap) {
    if (cols <= 0 || rows <= 0 || index < 0 || int64_t(index) >= int64_t(cols)*rows || gap < 0 ||
        a.w <= 0 || a.h <= 0 || int64_t(gap)*(cols-1) >= a.w || int64_t(gap)*(rows-1) >= a.h) return {};
    const int c = index % cols, r = index / cols;
    const int64_t w = int64_t(a.w)-int64_t(gap)*(cols-1), h = int64_t(a.h)-int64_t(gap)*(rows-1);
    const int x = static_cast<int>(c*w/cols), y = static_cast<int>(r*h/rows);
    return {a.x+x+c*gap, a.y+y+r*gap, static_cast<int>((c+1)*w/cols)-x, static_cast<int>((r+1)*h/rows)-y};
}
void Painter::outline(Rect r, uint16_t c) {
    if (r.w <= 0 || r.h <= 0) return;
    fill({r.x,r.y,r.w,1},c); fill({r.x,r.y+r.h-1,r.w,1},c);
    fill({r.x,r.y,1,r.h},c); fill({r.x+r.w-1,r.y,1,r.h},c);
}
void Painter::panel(Rect r, const Theme& t) { fill(r,t.panel); outline(r,t.border); }
void Painter::button(Rect r, const char* label, const Theme& t, bool selected, Availability state) {
    if (state == Availability::Hidden) return;
    const bool active = state == Availability::Available;
    const uint16_t foreground = !active ? (state == Availability::Disabled ? t.disabled : t.unavailable)
                                       : (selected ? t.onAccent : t.bright);
    fill(r,active && selected ? t.accent : t.panel);
    outline(r,active && selected ? t.accentHigh : t.border);
    text({r.x+3,r.y,r.w-6,r.h},label,foreground);
}
void Control::paint(Painter& p, const Theme& t) const { p.button(rect,label,t,selected,state); }
bool Control::addHit(HitList& hits) const {
    return state == Availability::Hidden || hits.add(rect,action,state == Availability::Available,identity);
}
uint16_t color(const Theme& t, Role r) {
    switch (r) {
    case Role::Background:return t.background; case Role::Panel:return t.panel;
    case Role::Border:return t.border; case Role::Text:return t.text;
    case Role::Accent:return t.accent; case Role::Muted:return t.muted;
    case Role::Bright:return t.bright; case Role::AccentHigh:return t.accentHigh;
    case Role::Success:return t.success; case Role::Warning:return t.warning;
    case Role::Danger:return t.danger; case Role::OnAccent:return t.onAccent;
    case Role::Disabled:return t.disabled; case Role::Unavailable:return t.unavailable;
    case Role::Count:break;
    }
    return t.background;
}
namespace {
double luminance(uint16_t c) {
    auto linear=[](double x){return x<=0.04045 ? x/12.92 : std::pow((x+0.055)/1.055,2.4);};
    return .2126*linear(((c>>11)&31)/31.0)+.7152*linear(((c>>5)&63)/63.0)+.0722*linear((c&31)/31.0);
}
bool contrast(uint16_t a,uint16_t b,double minimum) {
    const double x=luminance(a),y=luminance(b);
    return (std::max(x,y)+.05)/(std::min(x,y)+.05)>=minimum;
}
}
bool validateTheme(const Theme& t) {
    return contrast(t.text,t.background,4.5) && contrast(t.text,t.panel,4.5) &&
           contrast(t.bright,t.panel,4.5) && contrast(t.onAccent,t.accent,4.5) &&
           contrast(t.muted,t.panel,3) && contrast(t.disabled,t.panel,3) &&
           contrast(t.unavailable,t.panel,3) && contrast(t.success,t.panel,3) &&
           contrast(t.warning,t.panel,3) && contrast(t.danger,t.panel,3);
}
Theme modalBackdropTheme(const Theme& t) {
    // Fade each semantic role toward the active background without a framebuffer
    // alpha pass. Retain one third of the original channel difference.
    auto fade=[&](uint16_t c) {
        const int r=((t.background>>11)&31)+(((c>>11)&31)-int((t.background>>11)&31))/3;
        const int g=((t.background>>5)&63)+(((c>>5)&63)-int((t.background>>5)&63))/3;
        const int b=(t.background&31)+((c&31)-int(t.background&31))/3;
        return static_cast<uint16_t>((r<<11)|(g<<5)|b);
    };
    return {fade(t.background),fade(t.panel),fade(t.border),fade(t.text),fade(t.accent),
            fade(t.muted),fade(t.bright),fade(t.accentHigh),fade(t.success),fade(t.warning),
            fade(t.danger),fade(t.onAccent),fade(t.disabled),fade(t.unavailable)};
}
Availability availability(Capabilities supplied,Capabilities required,bool enabled,bool hide) {
    if ((supplied & required) != required) return hide ? Availability::Hidden : Availability::Unavailable;
    return enabled ? Availability::Available : Availability::Disabled;
}
RasterPainter::RasterPainter(uint16_t* p, int width, int top, int rows) : pixels_(p), clip_{0,top,width,rows} {}
void RasterPainter::fill(Rect r, uint16_t c) {
    if (!pixels_) return;
    r = intersect(r,clip_);
    if (r.w <= 0 || r.h <= 0) return;
    for (int y=r.y; y<r.y+r.h; ++y)
        std::fill_n(pixels_+(y-clip_.y)*clip_.w+r.x,r.w,c);
}
void Painter::text(Rect box, const char* label, uint16_t color, int scale) {
    if (!label || scale < 1 || scale > 8 || box.w <= 0 || box.h < 7*scale) return;
    // Fixed-height text never wraps; truncate with dots inside its own box.
    const int fit = box.w/(6*scale);
    if (fit <= 0) return;
    int length = 0;
    while (length <= fit && label[length]) ++length;
    const bool truncated = length > fit;
    const int count = std::min(length,fit);
    const int x = box.x+(box.w-(count*6-1)*scale)/2;
    const int y = box.y+(box.h-7*scale)/2;
    for (int i=0; i<count; ++i) {
        const char ch = truncated && i >= std::max(0,count-3) ? '.' : label[i];
        for (int row=0; row<7; ++row) for (int col=0; col<5; ++col)
            if (Font::row(ch,row) & (1 << (4-col)))
                fill(intersect({x+i*6*scale+col*scale,y+row*scale,scale,scale},box),color);
    }
}
void RasterPainter::text(Rect box,const char* label,uint16_t c,int scale) {Painter::text(box,label,c,scale);}
bool HitList::add(Rect r, Action a, bool enabled, ItemId identity) {
    if (count_ == capacity || r.w <= 0 || r.h <= 0 || !a) return false;
    hits_[count_++] = {r,a,enabled,identity}; return true;
}
const Hit* HitList::find(Point p) const {
    for (std::size_t i=count_; i>0; --i)
        if (contains(hits_[i-1].rect,p.x,p.y)) return &hits_[i-1];
    return nullptr;
}
Action HitList::at(Point p) const {
    const Hit* hit=find(p);
    return hit && hit->enabled ? hit->action : 0;
}
void Input::reset() { ++generation_; captured_=0; }
Action Input::dispatch(PointerEvent e, const HitList& hits, bool ready) {
    if (e.generation != generation_ || !ready || e.phase == PointerPhase::Cancel) { captured_=0; return 0; }
    const Action action = hits.at(e.point);
    const Hit* target=hits.find(e.point);
    const bool sameItem=target && target->identity==capturedIdentity_;
    if (e.phase == PointerPhase::Down) {
        captured_=action;
        if (const Hit* h=hits.find(e.point)) {capturedRect_=h->rect;capturedIdentity_=h->identity;}
    }
    else if (e.phase == PointerPhase::Move && (!sameItem || action != captured_ || !contains(capturedRect_,e.point.x,e.point.y))) captured_=0;
    else if (e.phase == PointerPhase::Up) {
        const Action result = sameItem && action == captured_ && contains(capturedRect_,e.point.x,e.point.y) ? action : 0;
        captured_=0; return result;
    }
    return 0;
}
bool Viewport::map(Point p, Point& out) const {
    if (!supported(logical) || !contains(physical,p.x,p.y)) return false;
    out = {static_cast<int>((int64_t(p.x)-physical.x)*logical.width/physical.w),
           static_cast<int>((int64_t(p.y)-physical.y)*logical.height/physical.h)};
    return true;
}
bool Renderer::begin(const Screen& screen, Size size, const Theme& theme, int rows, Rect damage) {
    // Failed replacement also discards pending outgoing paint.
    cancel();
    if (!buffer_ || !supported(size) || rows <= 0 || capacity_ < static_cast<std::size_t>(size.width)) return false;
    damage_ = damage.w > 0 && damage.h > 0 ? intersect(damage,{0,0,size.width,size.height}) : Rect{0,0,size.width,size.height};
    if (damage_.w <= 0 || damage_.h <= 0) return false;
    screen_=&screen; size_=size; theme_=theme; next_=damage_.y;
    rows_=static_cast<int>(std::min<std::size_t>(std::min(rows,size.height),capacity_/size.width));
    return true;
}
Step Renderer::step(Display& display) {
    if (!screen_) return Step::Idle;
    const int rows = std::min(rows_,damage_.y+damage_.h-next_);
    RasterPainter painter(buffer_,size_.width,next_,rows);
    painter.fill({0,next_,size_.width,rows},theme_.background);
    screen_->paint(painter,size_,theme_);
    for (int y=0; y<rows; ++y)
        std::memmove(buffer_+y*damage_.w,buffer_+y*size_.width+damage_.x,
                     static_cast<std::size_t>(damage_.w)*sizeof(uint16_t));
    if (!display.write({damage_.x,next_,damage_.w,rows},buffer_)) return Step::Retry;
    next_+=rows;
    if (next_ == damage_.y+damage_.h) { cancel(); return Step::Complete; }
    return Step::More;
}
void Renderer::cancel() { screen_=nullptr; next_=0; rows_=0; }
}
