#pragma once
#include "sample_ui/core.h"
namespace sample_ui::input {
enum class ContactStatus { Contact, Released, Unavailable, Error };
// Single-contact policy. Timing retains the previous BSP poll grace budget,
// but faults/expiry cancel rather than synthesizing a release. Single caller.
class ContactTracker {
public:
    static constexpr uint32_t staleAfterMs=90;
    bool update(ContactStatus status,Point point,uint32_t now,uint32_t generation,PointerEvent& out) {
        if (!initialized_) {generation_=generation;initialized_=true;}
        if (generation!=generation_) {
            generation_=generation;
            if (down_) {
                down_=false;blocked_=status!=ContactStatus::Released;
                out={PointerPhase::Cancel,last_,generation_};return true;
            }
        }
        // Even a late Contact or Released cannot revive an expired gesture.
        if (down_ && uint32_t(now-lastAt_)>=staleAfterMs) {
            down_=false;blocked_=status!=ContactStatus::Released;
            out={PointerPhase::Cancel,last_,generation_};return true;
        }
        if (status==ContactStatus::Error) {
            blocked_=true;
            if (!down_)return false;
            down_=false;out={PointerPhase::Cancel,last_,generation_};return true;
        }
        if (status==ContactStatus::Unavailable)return false;
        if (status==ContactStatus::Released) {
            blocked_=false;
            if (!down_)return false;
            down_=false;out={PointerPhase::Up,last_,generation_};return true;
        }
        if (blocked_)return false;
        out={down_?PointerPhase::Move:PointerPhase::Down,point,generation_};
        down_=true;last_=point;lastAt_=now;return true;
    }
private:
    bool initialized_=false,down_=false,blocked_=false;
    uint32_t generation_=0,lastAt_=0;
    Point last_{};
};
}
