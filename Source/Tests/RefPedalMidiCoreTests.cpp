#include "TestFramework.h"
#include "../ReferencePedals/Midi/MidiRouter.h"
#include "../ReferencePedals/Midi/MidiMap.h"
#include "../ReferencePedals/Midi/ClockSync.h"
#include <memory>
using namespace luthier::reference;
using namespace luthier::reference::midi;
LUTHIER_TEST (RefPedalMidiCore, mappedOffsetsAndReservedGuard)
{
    MidiRouter router; router.prepare (48000, 512);
    auto map = std::make_unique<MapSnap>();
    Binding b; b.srcNum = 20; b.target = 3; b.smoothMs = 0;
    CHECK (map->add (b));
    b.srcNum = 123; CHECK (! map->add (b));
    map->compile(); router.publishMap (std::move (map));
    juce::MidiBuffer in; in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 100);
    RouteOut out; router.process (in, {}, 0, 512, out);
    CHECK (out.nWrites == 1); CHECK (out.writes[0].offset == 100);
    CHECK (out.writes[0].paramIdx == 3); CHECK (out.writes[0].norm == 1.f);
}
LUTHIER_TEST (RefPedalMidiCore, clockStartStopLoss)
{
    ClockSync clock; clock.prepare (48000);
    RouteOut out; clock.begin ({}, TempoSource::MidiClock, 120, 0, out);
    clock.event (0xfa, 0, 0, 0, out);
    for (int i = 0; i < 48; ++i) clock.event (0xf8, 0, 0, i * 1000, out);
    clock.finish (48000, out);
    CHECK (std::abs (out.time.bpm - 120.) < 0.1); CHECK (out.time.playing);
    clock.event (0xfc, 0, 0, 48000, out); CHECK (! clock.isRunning());
    clock.event (0xfb, 0, 0, 48000, out); CHECK (clock.isRunning());
    clock.finish (73000, out); CHECK (clock.isLost()); CHECK (! clock.isRunning());
}
LUTHIER_TEST (RefPedalMidiCore, boundedFloodProtectsClock)
{
    MidiRouter router; router.prepare (48000, 512);
    juce::MidiBuffer in;
    for (int i = 0; i < 800; ++i) in.addEvent (juce::MidiMessage::controllerEvent (1, 20, i % 128), 0);
    in.addEvent (juce::MidiMessage::midiStart(), 1);
    in.addEvent (juce::MidiMessage::midiClock(), 2);
    RouteOut out; router.process (in, {}, 0, 512, out);
    CHECK (router.droppedCount() > 0); CHECK (out.nTr >= 1);
}
LUTHIER_TEST (RefPedalMidiCore, learnNeedsDistinctValuesAndSuppresses)
{
    MidiRouter router; router.prepare (48000, 512); router.armLearn ({ 0, 0, true });
    juce::MidiBuffer in; in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 10), 0);
    RouteOut out; router.process (in, {}, 0, 512, out); LearnCapture c;
    CHECK (! router.pollLearn (c));
    in.clear(); in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 80), 4);
    router.process (in, {}, 512, 512, out); CHECK (router.pollLearn (c));
    CHECK (c.valid); CHECK (c.num == 20); CHECK (c.chan == 1);
}

#include <thread>
#include <atomic>
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
LUTHIER_TEST (RefPedalMidiCore, noAllocationsDuringRouting)
{
    MidiRouter router; router.prepare (48000, 512);
    auto map = std::make_unique<MapSnap>(); Binding b; b.srcNum = 20; CHECK (map->add (b));
    router.publishMap (std::move (map));
    juce::MidiBuffer in; in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 37);
    RouteOut out;
    const auto before = luthier::tests::allocationsOnThisThread();
    router.process (in, {}, 0, 512, out);
    const auto after = luthier::tests::allocationsOnThisThread();
    CHECK (after == before); CHECK (out.nWrites == 1);
}
LUTHIER_TEST (RefPedalMidiCore, concurrentPublicationAndReclamation)
{
    MidiRouter router; router.prepare (48000, 64);
    std::atomic<bool> done { false }, valid { true };
    std::thread audio ([&]
    {
        juce::MidiBuffer in; in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 37);
        RouteOut out; int64_t sample = 0;
        while (! done.load (std::memory_order_acquire))
        {
            router.process (in, {}, sample, 64, out); sample += 64;
            if (out.nWrites > 1 || (out.nWrites == 1 && (out.writes[0].norm < 0 || out.writes[0].norm > 1))) valid.store (false);
        }
    });
    for (int i = 0; i < 200; ++i)
    {
        auto map = std::make_unique<MapSnap>(); Binding b; b.srcNum = 20; b.target = (uint16_t) (i % 32); map->add (b);
        router.publishMap (std::move (map)); router.reclaim();
    }
    done.store (true, std::memory_order_release); audio.join();
    CHECK (valid.load());
}
LUTHIER_TEST (RefPedalMidiCore, boardTargetsRetainOffsets)
{
    MidiRouter router; router.prepare (48000, 512);
    auto map = std::make_unique<MapSnap>();
    Binding b; b.srcNum = 20; b.target = kTargetBoardBase + (uint16_t) BoardTarget::Mix; CHECK (map->add (b));
    router.publishMap (std::move (map));
    juce::MidiBuffer in; in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 37);
    RouteOut out; router.process (in, {}, 0, 512, out);
    CHECK (out.nWrites == 1); CHECK (out.writes[0].slot == kBoardTargetSlot);
    CHECK (out.writes[0].paramIdx == (uint16_t) BoardTarget::Mix); CHECK (out.writes[0].offset == 37);
}
LUTHIER_TEST (RefPedalMidiCore, softTakeoverAndExactChannelPrecedence)
{
    MidiRouter router; router.prepare (48000, 128);
    auto map = std::make_unique<MapSnap>(); Binding b; b.srcNum = 20; b.flags |= BF_SoftTakeover;
    CHECK (map->add (b)); map->initialValues[0] = .5f;
    router.publishMap (std::move (map));
    juce::MidiBuffer in; RouteOut out;
    in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 10), 0);
    router.process (in, {}, 0, 128, out); CHECK (out.nWrites == 0);
    in.clear(); in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 100), 0);
    router.process (in, {}, 128, 128, out); CHECK (out.nWrites == 1);
    auto exact = std::make_unique<MapSnap>(); b.flags = BF_Enabled; b.target = 1; exact->add (b);
    b.chan = 2; b.target = 2; exact->add (b); router.publishMap (std::move (exact));
    in.clear(); in.addEvent (juce::MidiMessage::controllerEvent (2, 20, 100), 0);
    router.process (in, {}, 256, 128, out); CHECK (out.nWrites == 1); CHECK (out.writes[0].paramIdx == 2);
}
LUTHIER_TEST (RefPedalMidiCore, heldNotesReleaseOriginalSlotAcrossReorder)
{
    MidiRouter router; router.prepare (48000, 128);
    auto map = std::make_unique<MapSnap>(); map->noteToSlot[36] = 5; router.publishMap (std::move (map));
    juce::MidiBuffer in; in.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 20), 37);
    RouteOut out; router.process (in, {}, 0, 128, out);
    CHECK (out.nFoot == 1); CHECK (out.footSlot[0] == 5); CHECK (out.foot[0].press);
    auto moved = std::make_unique<MapSnap>(); moved->noteToSlot[36] = 7; router.publishMap (std::move (moved));
    in.clear(); in.addEvent (juce::MidiMessage::noteOff (1, 36), 44);
    router.process (in, {}, 128, 128, out);
    CHECK (out.nFoot == 1); CHECK (out.footSlot[0] == 5); CHECK (! out.foot[0].press); CHECK (out.foot[0].offset == 44);
}
LUTHIER_TEST (RefPedalMidiCore, disabledBoardOnlyAllowsEnableTarget)
{
    MidiRouter router; router.prepare (48000, 128);
    auto map = std::make_unique<MapSnap>(); map->boardEnabled = false; map->noteToSlot[36] = 0;
    Binding b; b.srcNum = 20; b.target = 1; map->add (b);
    b.srcNum = 21; b.target = kTargetBoardBase + (uint16_t) BoardTarget::Enabled; map->add (b);
    router.publishMap (std::move (map));
    juce::MidiBuffer in; in.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 80), 0);
    in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 1);
    in.addEvent (juce::MidiMessage::controllerEvent (1, 21, 127), 37);
    in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 38);
    RouteOut out; router.process (in, {}, 0, 128, out);
    CHECK (out.nFoot == 0); CHECK (out.nWrites == 2);
    CHECK (out.writes[0].slot == kBoardTargetSlot); CHECK (out.writes[0].offset == 37);
    CHECK (out.writes[1].slot == 0); CHECK (out.writes[1].offset == 38);
}
