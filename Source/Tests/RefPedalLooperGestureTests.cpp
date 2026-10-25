// OWNER: WP15/WP16
#include "TestFramework.h"
#include "../ReferencePedals/Common/LooperFootGesture.h"
using namespace luthier::reference::common;
LUTHIER_TEST(RefPedalLooperGesture, downIsImmediateAndDoubleUsesFirstRelease)
{
    LooperFootGesture gesture;CHECK(gesture.configure(1000,{}));LooperFootGesture::Event event;
    CHECK(!gesture.edge(true,-1,LooperCore::State::Empty,event));CHECK(!gesture.isHeld());
    CHECK(gesture.edge(true,100,LooperCore::State::Empty,event));CHECK(event.kind==LooperFootGesture::Kind::Down);CHECK(event.began==LooperCore::State::Empty);
    CHECK(!gesture.edge(false,200,LooperCore::State::Recording,event));CHECK(gesture.edge(true,550,LooperCore::State::Recording,event));CHECK(event.kind==LooperFootGesture::Kind::DoubleDown);
    CHECK(!gesture.advance(1549,event));CHECK(gesture.advance(1550,event));CHECK(event.kind==LooperFootGesture::Kind::ClearHold);CHECK(!gesture.advance(1551,event));
    gesture.edge(false,1600,LooperCore::State::Empty,event);CHECK(gesture.edge(true,1700,LooperCore::State::Empty,event));CHECK(event.kind==LooperFootGesture::Kind::Down);
}
LUTHIER_TEST(RefPedalLooperGesture, holdsFireAtExactSampleAndRememberPrePressState)
{
    LooperFootGesture gesture;CHECK(gesture.configure(48000,{}));LooperFootGesture::Event event;
    gesture.edge(true,7,LooperCore::State::Playing,event);CHECK(!gesture.advance(72006,event));CHECK(gesture.advance(72007,event));CHECK(event.kind==LooperFootGesture::Kind::UndoRedoHold);CHECK(event.began==LooperCore::State::Playing);CHECK(!gesture.advance(100000,event));
    gesture.edge(false,100001,LooperCore::State::Playing,event);gesture.edge(true,110000,LooperCore::State::Stopped,event);CHECK(!gesture.advance(205999,event));CHECK(gesture.advance(206000,event));CHECK(event.kind==LooperFootGesture::Kind::ClearHold);
    gesture.reset();gesture.edge(true,0,LooperCore::State::Recording,event);CHECK(!gesture.advance(500000,event));
}
LUTHIER_TEST(RefPedalLooperGesture, debounceNeverInventsHeldReleaseAndGapExpires)
{
    LooperFootGesture gesture;CHECK(gesture.configure(1000,{}));LooperFootGesture::Event event;
    CHECK(gesture.edge(true,100,LooperCore::State::Playing,event));CHECK(!gesture.edge(false,101,LooperCore::State::Overdubbing,event));CHECK(!gesture.isHeld());
    CHECK(!gesture.edge(true,105,LooperCore::State::Overdubbing,event));CHECK(!gesture.edge(false,106,LooperCore::State::Overdubbing,event));CHECK(!gesture.isHeld());CHECK(!gesture.advance(5000,event));
    CHECK(gesture.edge(true,500,LooperCore::State::Playing,event));CHECK(event.kind==LooperFootGesture::Kind::Down);CHECK(!gesture.edge(true,501,LooperCore::State::Overdubbing,event));
    gesture.edge(false,900,LooperCore::State::Overdubbing,event);CHECK(gesture.edge(true,901,LooperCore::State::Playing,event));CHECK(event.kind==LooperFootGesture::Kind::Down);
}
LUTHIER_TEST(RefPedalLooperGesture, modelTimingOverridesRemainOnSampleClock)
{
    LooperFootGesture gesture;LooperFootGesture::Timing timing;timing.doubleGapMs=300;timing.undoHoldMs=1000;CHECK(gesture.configure(44100,timing));LooperFootGesture::Event event;
    gesture.edge(true,0,LooperCore::State::Playing,event);CHECK(!gesture.advance(44099,event));CHECK(gesture.advance(44100,event));CHECK(event.kind==LooperFootGesture::Kind::UndoRedoHold);
    gesture.reset();gesture.edge(true,0,LooperCore::State::Playing,event);gesture.edge(false,4410,LooperCore::State::Overdubbing,event);gesture.edge(true,17640,LooperCore::State::Overdubbing,event);CHECK(event.kind==LooperFootGesture::Kind::DoubleDown);
    CHECK(!gesture.configure(std::numeric_limits<double>::quiet_NaN(),timing));
}
#if LUTHIER_ALLOCATION_COUNTER
namespace luthier::tests{long allocationsOnThisThread() noexcept;}
LUTHIER_TEST(RefPedalLooperGesture, gestureEdgesAndSampleThresholdsNeverAllocate)
{
    LooperFootGesture gesture;CHECK(gesture.configure(1000,{}));LooperFootGesture::Event event;const auto before=luthier::tests::allocationsOnThisThread();
    for(int64_t pass=0;pass<1000;++pass){const auto now=pass*10000;gesture.edge(true,now,LooperCore::State::Playing,event);gesture.advance(now+1499,event);gesture.advance(now+1500,event);gesture.edge(false,now+1501,LooperCore::State::Playing,event);}
    CHECK(luthier::tests::allocationsOnThisThread()-before==0);
}
#endif

LUTHIER_TEST(RefPedalLooperGesture, releaseAtThresholdFiresOnceBeforeNextRenderSample)
{
    LooperFootGesture gesture;CHECK(gesture.configure(1000,{}));LooperFootGesture::Event event;
    gesture.edge(true,100,LooperCore::State::Playing,event);CHECK(gesture.edge(false,1600,LooperCore::State::Overdubbing,event));CHECK(event.kind==LooperFootGesture::Kind::UndoRedoHold);CHECK(!gesture.isHeld());CHECK(!gesture.advance(1600,event));
    CHECK(gesture.edge(true,1700,LooperCore::State::Playing,event));CHECK(event.kind==LooperFootGesture::Kind::Down);CHECK(!gesture.edge(false,3199,LooperCore::State::Overdubbing,event));CHECK(!gesture.advance(3200,event));
}
