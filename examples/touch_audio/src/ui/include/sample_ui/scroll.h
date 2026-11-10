#pragma once
#include "sample_ui/core.h"
namespace sample_ui {
// Release-based row scrolling: no model changes during an outstanding contact.
// Once movement crosses the slop, the contact can never complete a tap.
enum class Swipe { None, Consumed, Forward, Backward };
class VerticalSwipe {
public:
    Swipe dispatch(PointerEvent e,Rect body,uint32_t generation,bool ready) {
        if(!ready || e.generation!=generation || e.phase==PointerPhase::Cancel) {
            active_=dragged_=false;return Swipe::None;
        }
        if(e.phase==PointerPhase::Down) {
            active_=contains(body,e.point.x,e.point.y);dragged_=false;
            start_=e.point;generation_=generation;return Swipe::None;
        }
        if(!active_ || generation_!=generation){active_=false;return Swipe::None;}
        const int64_t dx=int64_t(e.point.x)-start_.x,dy=int64_t(e.point.y)-start_.y;
        const auto ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
        dragged_=dragged_ || ax>=18 || ay>=18;
        if(e.phase==PointerPhase::Up) {
            active_=false;
            if(!dragged_)return Swipe::None;
            if(ay>=24 && ay>ax && contains(body,e.point.x,e.point.y))
                return dy<0?Swipe::Forward:Swipe::Backward;
            return Swipe::Consumed;
        }
        return dragged_?Swipe::Consumed:Swipe::None;
    }
private:
    Point start_{};
    uint32_t generation_=0;
    bool active_=false,dragged_=false;
};
}

namespace sample_ui {
struct DragUpdate {bool consumed=false,changed=false,ended=false;int offset=0;};
// Pixel dragging with locked intent. Offset is proposed; the caller applies it
// only between completed renders. No inertia or allocation; taps retain Input.
class DragScroll {
public:
    bool active() const {return active_;}
    DragUpdate dispatch(PointerEvent e,Rect viewport,int offset,int limit,uint32_t generation,bool ready) {
        auto clamp=[&](int64_t value){return static_cast<int>(value<0?0:value>limit?limit:value);};
        if(e.generation!=generation || (active_ && generation_!=generation)) {active_=false;return {};}
        if(e.phase==PointerPhase::Cancel) {
            const bool ended=active_ && intent_!=0;active_=false;return {ended,false,ended,offset};
        }
        if(e.phase==PointerPhase::Down) {
            active_=ready && contains(viewport,e.point.x,e.point.y);
            intent_=0;start_=e.point;origin_=offset;generation_=generation;return {};
        }
        if(!active_)return {};
        const int64_t dx=int64_t(e.point.x)-start_.x,dy=int64_t(e.point.y)-start_.y;
        const auto ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
        if(!intent_ && (ax>=8 || ay>=8))intent_=ay>ax?1:2;
        const int proposed=intent_==1?clamp(int64_t(origin_)-dy):offset;
        const bool up=e.phase==PointerPhase::Up;
        if(up)active_=false;
        return {intent_!=0,proposed!=offset,up && intent_!=0,proposed};
    }
private:
    Point start_{};int origin_=0,intent_=0;uint32_t generation_=0;bool active_=false;
};
}

namespace sample_ui {
struct ScrollInteraction {Action action=0;bool scrolling=false,finished=false;int offset=0;};
// Shared arbitration for consumers of ListScreen. Applications retain ownership
// of action IDs, proposed viewport application and render scheduling.
class ScrollInput {
public:
    explicit ScrollInput(Input& input):input_(input){}
    ScrollInteraction dispatch(PointerEvent e,const HitList& hits,Rect viewport,int offset,int limit,bool ready) {
        const auto update=drag_.dispatch(e,viewport,offset,limit,input_.generation(),ready);
        if(update.consumed) {
            input_.dispatch({PointerPhase::Cancel,e.point,e.generation},hits,false);
            return {0,update.changed || update.ended,update.ended,update.offset};
        }
        return {input_.dispatch(e,hits,ready),false,false,offset};
    }
private:
    Input& input_;DragScroll drag_;
};
}
