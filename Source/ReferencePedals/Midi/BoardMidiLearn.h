#pragma once
#include "MidiRouter.h"
namespace luthier::reference::midi
{
// Message-thread timeout/confirmation controller; capture itself uses the RT ring.
class BoardMidiLearn
{
public:
    enum class State { Idle, Armed, Confirm };
    explicit BoardMidiLearn (MidiRouter& r) : router (r) {}
    void arm (LearnTarget t, double nowSeconds) { target = t; began = nowSeconds; state = State::Armed; router.armLearn (t); }
    void cancel() { router.cancelLearn(); state = State::Idle; capture = {}; }
    void poll (double nowSeconds)
    {
        if (state != State::Armed) return;
        if (nowSeconds - began >= 15) { cancel(); return; }
        if (router.pollLearn (capture)) state = State::Confirm;
    }
    State getState() const noexcept { return state; }
    LearnCapture getCapture() const noexcept { return capture; }
    LearnTarget getTarget() const noexcept { return target; }
private:
    MidiRouter& router;
    State state = State::Idle;
    LearnTarget target;
    LearnCapture capture;
    double began = 0;
};
}
