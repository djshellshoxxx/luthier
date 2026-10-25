#include "TestFramework.h"
#include "../Support/PreviewCommandQueue.h"
#include <array>
#include <atomic>
#include <limits>
#include <thread>
#include <type_traits>
using namespace luthier::preview;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
LUTHIER_TEST(PreviewCommands, fixedPodAndOrderedCrossLaneDrain)
{
    static_assert(std::is_trivially_copyable_v<Command>);
    static_assert(sizeof(Command)==32);
    CommandQueue queue;
    CHECK(queue.postNoteOn(60,.75f,2)); CHECK(queue.postStringOn(3,7.25,.8f));
    CHECK(queue.postNoteOff(60,2)); CHECK(queue.postStringOff(3));
    Command out[8]; const auto before=luthier::tests::allocationsOnThisThread();
    const int n=queue.drain(out,8); const auto after=luthier::tests::allocationsOnThisThread();
    CHECK(before==after); CHECK(n==4);
    if(n==4)
    {
        CHECK(out[0].kind==CommandKind::NoteOn && out[0].note==60 && out[0].channel==2 && out[0].velocity==.75f);
        CHECK(out[1].kind==CommandKind::StringOn && out[1].string==3 && out[1].fret==7.25);
        CHECK(out[2].kind==CommandKind::NoteOff && out[3].kind==CommandKind::StringOff);
        CHECK(out[0].order<out[1].order && out[1].order<out[2].order && out[2].order<out[3].order);
    }
    CHECK(queue.drain(out,8)==0);
}
LUTHIER_TEST(PreviewCommands, releaseLaneSurvivesFullNoteOnLaneWithoutReordering)
{
    CommandQueue queue; bool accepted=true;
    for(int i=0;i<CommandQueue::laneCapacity;++i) accepted &= queue.postNoteOn(i%128,.7f);
    CHECK(accepted); CHECK(!queue.postNoteOn(100,.7f));
    accepted=true; for(int i=0;i<128;++i) accepted &= queue.postNoteOff(i);
    CHECK(accepted); CHECK(queue.droppedCount()==1);
    std::array<Command,CommandQueue::maxDrain> out;
    const int n=queue.drain(out.data(),(int)out.size()); CHECK(n==CommandQueue::laneCapacity+128);
    bool ordered=true,release=true;
    for(int i=1;i<n;++i) ordered &= out[(size_t)i-1].order<out[(size_t)i].order;
    for(int i=CommandQueue::laneCapacity;i<n;++i) release &= out[(size_t)i].kind==CommandKind::NoteOff;
    CHECK(ordered); CHECK(release);
}
LUTHIER_TEST(PreviewCommands, releaseOverflowPublishesPanicAndOldEpochCannotResurrectNotes)
{
    CommandQueue queue; Command out[CommandQueue::maxDrain];
    CHECK(queue.postNoteOn(60,.7f)); CHECK(queue.drain(out,1)==1);
    bool accepted=true; for(int i=0;i<CommandQueue::laneCapacity;++i) accepted &= queue.postNoteOff(i%128);
    CHECK(accepted); CHECK(!queue.postNoteOff(60)); CHECK(queue.droppedCount()==1);
    CHECK(queue.postNoteOn(61,.8f));
    const int n=queue.drain(out,CommandQueue::maxDrain); CHECK(n==2);
    if(n==2)
    {
        CHECK(out[0].kind==CommandKind::Panic && out[0].epoch>1);
        CHECK(out[1].kind==CommandKind::NoteOn && out[1].note==61 && out[1].epoch==out[0].epoch);
    }
}
LUTHIER_TEST(PreviewCommands, panicInvalidatesCachedCommandsWithoutControlThreadQueueReset)
{
    CommandQueue queue; Command out[8];
    CHECK(queue.postNoteOn(60,.7f)); CHECK(queue.postNoteOff(60)); CHECK(queue.drain(out,1)==1);
    queue.requestPanic(); CHECK(queue.postStringOn(2,5.5,.6f));
    CHECK(queue.drain(nullptr,8)==0); CHECK(queue.drain(out,0)==0);
    const int n=queue.drain(out,8); CHECK(n==2);
    if(n==2) CHECK(out[0].kind==CommandKind::Panic && out[1].kind==CommandKind::StringOn && out[0].epoch==out[1].epoch);
    CHECK(queue.drain(out,8)==0);
}
LUTHIER_TEST(PreviewCommands, boundedChordExpansionAndMalformedInput)
{
    CommandQueue queue; Command out[CommandQueue::maxDrain]; const int notes[]{60,64,67};
    CHECK(queue.postChord(notes,3,.9f)==3); CHECK(queue.postChord(notes,3,.9f,true)==3);
    CHECK(queue.postChord(notes,129,.9f)==0); CHECK(queue.postChord(nullptr,3,.9f)==0);
    CHECK(!queue.postNoteOn(-1,.7f)); CHECK(!queue.postNoteOff(128)); CHECK(!queue.postNoteOn(60,.7f,17));
    CHECK(!queue.postStringOn(16,0,.7f)); CHECK(!queue.postStringOff(-1));
    CHECK(!queue.postStringOn(0,std::numeric_limits<double>::infinity(),.7f));
    CHECK(!queue.postNoteOn(60,std::numeric_limits<float>::quiet_NaN()));
    const int n=queue.drain(out,CommandQueue::maxDrain); CHECK(n==6);
    if(n==6) CHECK(out[0].note==60 && out[1].note==64 && out[2].note==67 && out[3].kind==CommandKind::NoteOff);
    CHECK(detail::nextEpoch(UINT64_MAX)==0); // exhaustion fails closed; not a reusable incarnation
}
LUTHIER_TEST(PreviewCommands, concurrentControlProducersAndAudioDrainRemainCoherent)
{
    CommandQueue queue; std::atomic<bool> begin{false},done{false},valid{true},noAlloc{true};
    std::atomic<int> panics{0},events{0};
    std::array<uint64_t,5> firstInvalid{}; // audio-owned until join; diagnostic is formatted off audio
    std::thread audio([&]
    {
        while(!begin.load(std::memory_order_acquire)) {}
        uint64_t epoch=1,lastOrder=0; std::array<std::array<bool,128>,16> held{};
        Command out[64]; const auto before=luthier::tests::allocationsOnThisThread();
        auto consume=[&]
        {
            const int n=queue.drain(out,64); bool okay=n>=0 && n<=64;
            for(int i=0;i<n;++i)
            {
                const auto& c=out[i]; ++events;
                if(c.kind==CommandKind::Panic)
                { okay &= c.epoch>epoch; epoch=c.epoch; lastOrder=0; held={}; ++panics; }
                else
                {
                    const bool coherent=c.epoch==epoch && c.order>lastOrder && c.channel>=1 && c.channel<=16 && c.note<128;
                    if(!coherent && firstInvalid[0]==0) firstInvalid={1,epoch,lastOrder,c.epoch,c.order};
                    okay &= coherent;
                    lastOrder=c.order;
                    if(c.kind==CommandKind::NoteOn) held[c.channel-1][c.note]=true;
                    if(c.kind==CommandKind::NoteOff) held[c.channel-1][c.note]=false;
                }
            }
            if(!okay) valid.store(false,std::memory_order_release);
        };
        while(!done.load(std::memory_order_acquire)) consume();
        for(int i=0;i<32;++i) consume();
        bool allReleased=true; for(auto& channel:held) for(bool note:channel) allReleased &= !note;
        if(!allReleased) valid.store(false,std::memory_order_release);
        noAlloc.store(before==luthier::tests::allocationsOnThisThread(),std::memory_order_release);
    });
    std::array<std::thread,4> producers;
    for(int p=0;p<4;++p) producers[(size_t)p]=std::thread([&,p]
    {
        while(!begin.load(std::memory_order_acquire)) {}
        for(int i=0;i<10000;++i)
        {
            queue.postNoteOn(36+i%48,.7f,p+1); queue.postNoteOff(36+i%48,p+1);
            if(i%347==0) queue.requestPanic();
        }
    });
    begin.store(true,std::memory_order_release); for(auto& producer:producers) producer.join();
    queue.requestPanic(); done.store(true,std::memory_order_release); audio.join();
    CHECK_MSG(valid.load(), "first invalid epoch/order: expected epoch="+juce::String((juce::int64)firstInvalid[1])+" after order="+juce::String((juce::int64)firstInvalid[2])+" received epoch="+juce::String((juce::int64)firstInvalid[3])+" order="+juce::String((juce::int64)firstInvalid[4]));
    CHECK(noAlloc.load()); CHECK(panics.load()>0); CHECK(events.load()>0);
}
LUTHIER_TEST(PreviewCommands, stoppedConsumerResumesWithPanicAndFreshCommandsWithoutReinitialisingTheQueue)
{
    CommandQueue queue; std::array<Command,CommandQueue::maxDrain> out;
    bool accepted=true;
    for(int i=0;i<CommandQueue::laneCapacity;++i) accepted &= queue.postNoteOn(i%128,.7f);
    CHECK(accepted); CHECK(queue.postNoteOff(60));
    queue.requestPanic(); // audio is stopped; producer never clears either ring
    CHECK(queue.drain(out.data(),1)==1 && out[0].kind==CommandKind::Panic);
    CHECK(queue.drain(out.data(),(int)out.size())==0); // all queued old-epoch work is discarded
    bool lifecycle=true; uint64_t epoch=out[0].epoch;
    for(int i=0;i<1000;++i)
    {
        lifecycle &= queue.postStringOn(2,5.5,.7f) && queue.postStringOff(2);
        lifecycle &= queue.drain(out.data(),1)==1 && out[0].kind==CommandKind::StringOn;
        queue.requestPanic(); // stop/reprepare invalidates the cached release head too
        lifecycle &= queue.postNoteOn(61,.8f) && queue.postNoteOff(61);
        const int n=queue.drain(out.data(),(int)out.size());
        lifecycle &= n==3 && out[0].kind==CommandKind::Panic && out[0].epoch>epoch;
        lifecycle &= out[1].kind==CommandKind::NoteOn && out[2].kind==CommandKind::NoteOff;
        lifecycle &= out[1].epoch==out[0].epoch && out[2].epoch==out[0].epoch;
        epoch=out[0].epoch;
    }
    CHECK(lifecycle); CHECK(queue.drain(out.data(),(int)out.size())==0);
}
