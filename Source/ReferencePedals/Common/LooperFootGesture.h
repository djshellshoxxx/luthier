// OWNER: WP15/WP16
#pragma once
#include "LooperCore.h"
#include <algorithm>
#include <cmath>
namespace luthier::reference::common
{
// Audio-owner, sample-clock gestures. Down acts immediately. A second down
// is measured from the first release; long holds fire at their threshold.
// Wrappers interpret the event against its remembered pre-press transport.
class LooperFootGesture
{
public:
    using State=LooperCore::State;
    enum class Kind { None,Down,DoubleDown,UndoRedoHold,ClearHold };
    struct Event {Kind kind=Kind::None;State began=State::Empty;};
    struct Timing
    {
        double debounceMs=10,tapMaxMs=300,doubleGapMs=350,undoHoldMs=1500,stoppedClearMs=2000,doubleClearMs=1000;
    };
    bool configure(double rate,const Timing& timing) noexcept
    {
        if(!std::isfinite(rate) || rate<1 || rate>192000)return false;
        const auto count=[&](double ms,double fallback)
        {return int64_t(std::ceil(rate*.001*(std::isfinite(ms)?std::clamp(ms,0.,5000.):fallback)));};
        debounce=count(timing.debounceMs,10);tapMax=count(timing.tapMaxMs,300);doubleGap=count(timing.doubleGapMs,350);
        undoHold=count(timing.undoHoldMs,1500);stoppedClear=count(timing.stoppedClearMs,2000);doubleClear=count(timing.doubleClearMs,1000);configured=true;return true;
    }
    void reset() noexcept {held=second=fired=canDouble=hasDown=false;downAt=lastDown=lastUp=0;began=State::Empty;}
    bool edge(bool press,int64_t sample,State state,Event& out) noexcept
    {
        out={};if(!configured || sample<0 || press==held)return false;
        if(press)
        {
            // Always accept release; bounce suppression must not leave a short
            // released press latched as a fictitious held footswitch.
            if(hasDown && sample>=lastDown && sample-lastDown<debounce)return false;
            held=true;fired=false;began=state;downAt=sample;
            second=canDouble && sample>=lastUp && sample-lastUp<=doubleGap;
            canDouble=false;hasDown=true;lastDown=sample;out={second?Kind::DoubleDown:Kind::Down,began};return true;
        }
        // A release may be dispatched at the threshold before this sample's
        // render call. Resolve that crossing while the switch is still held.
        const bool threshold=advance(sample,out);
        held=false;canDouble=!second && !fired && sample>=downAt && sample-downAt<=tapMax;lastUp=sample;return threshold;
    }
    bool advance(int64_t sample,Event& out) noexcept
    {
        out={};if(!configured || !held || fired || sample<downAt)return false;
        int64_t threshold=0;Kind kind=Kind::None;
        if(second){threshold=doubleClear;kind=Kind::ClearHold;}
        else if(began==State::Stopped){threshold=stoppedClear;kind=Kind::ClearHold;}
        else if(began==State::Playing || began==State::Overdubbing){threshold=undoHold;kind=Kind::UndoRedoHold;}
        if(kind==Kind::None || sample-downAt<threshold)return false;
        fired=true;canDouble=false;out={kind,began};return true;
    }
    bool isHeld() const noexcept{return held;}
private:
    bool configured=false,held=false,second=false,fired=false,canDouble=false,hasDown=false;
    int64_t debounce=0,tapMax=0,doubleGap=0,undoHold=0,stoppedClear=0,doubleClear=0,downAt=0,lastDown=0,lastUp=0;
    State began=State::Empty;
};
}
