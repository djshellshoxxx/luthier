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
