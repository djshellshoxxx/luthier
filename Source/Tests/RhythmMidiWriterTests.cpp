#include "TestFramework.h"
#include "../Rhythm/RhythmMidiWriter.h"
#include "../Routing/MidiOutRouter.h"
#include <tuple>

using namespace luthier;
using namespace luthier::tests;

namespace
{
void strike (PlayEventQueue& queue, int string, int pitch, int offset)
{
    NoteOnEvent event;
    event.stringIndex = string;
    event.midiNote = pitch;
    event.sampleOffset = offset;
    event.velocity = 1.0;
    queue.addNoteOn (event);
}
void releaseString (PlayEventQueue& queue, int string, int offset)
{
    NoteOffEvent event; // Default pitch 60 must not replace the actual held pitch.
    event.stringIndex = string;
    event.sampleOffset = offset;
    queue.addNoteOff (event);
}
using Event = std::tuple<int, int, int, int>;
std::vector<Event> collect (const juce::MidiBuffer& midi, int origin = 0)
{
    std::vector<Event> result;
    for (const auto e : midi)
        result.emplace_back (origin + e.samplePosition, e.data[0], e.data[1], e.data[2]);
    return result;
}
MidiOutConfig rhythmConfig()
{
    MidiOutConfig config;
    config.enabled = config.rhythmEngine = true;
    config.passThrough = false;
    return config;
}
}

LUTHIER_TEST (RhythmMidiWriter, exactChronologicalStreamAcrossBlockPartitions)
{
    std::vector<Event> expected { {0, 0x90, 48, 127}, {40, 0x91, 55, 127},
                                {80, 0x80, 48, 0}, {80, 0x90, 50, 127},
                                {128, 0x81, 55, 0}, {160, 0x80, 50, 0} };
    for (int size : {1, 37, 512, 2048})
    {
        RhythmMidiWriter writer;
        PlayEventQueue strokes;
        // Deliberately enqueue attacks out of time order, with offs in a separate list.
        strike (strokes, 0, 50, 80);
        strike (strokes, 1, 55, 40);
        strike (strokes, 0, 48, 0);
        releaseString (strokes, 1, 128);
        releaseString (strokes, 0, 160);
        juce::MidiBuffer output;
        output.ensureSize (BoundedMidi::kReserveBytes);
        std::vector<Event> actual;
        for (int origin = 0; origin < 4096; origin += size)
        {
            output.clear();
            writer.process (strokes, true, &output, size, 0);
            auto part = collect (output, origin);
            actual.insert (actual.end(), part.begin(), part.end());
            strokes.clear();
        }
        CHECK (actual == expected);
    }
}

LUTHIER_TEST (RhythmMidiWriter, stoppingCancelsFutureAttacksAndClosesActualPitches)
{
    RhythmMidiWriter writer;
    PlayEventQueue strokes;
    strike (strokes, 0, 43, 0);
    strike (strokes, 1, 58, 90);
    juce::MidiBuffer output;
    output.ensureSize (BoundedMidi::kReserveBytes);
    writer.process (strokes, true, &output, 64, 11);
    CHECK ((collect (output) == std::vector<Event>{{11, 0x90, 43, 127}}));
    output.clear();
    strokes.clear();
    writer.process (strokes, false, &output, 64, 7);
    CHECK ((collect (output) == std::vector<Event>{{7, 0x80, 43, 0}}));
    output.clear();
    writer.process (strokes, true, &output, 1024, 0);
    CHECK (output.isEmpty());
}

LUTHIER_TEST (RhythmMidiWriter, resetReleaseDoesNotConsumeTheNextAttackBlock)
{
    for (bool previouslyHeld : {false, true})
    {
        RhythmMidiWriter writer;
        PlayEventQueue strokes;
        juce::MidiBuffer output;
        output.ensureSize (BoundedMidi::kReserveBytes);
        if (previouslyHeld)
        {
            strike (strokes, 0, 43, 0);
            writer.process (strokes, true, &output, 64, 0);
        }
        writer.cancel(); // engine prepare/reset or transport restart
        strokes.clear(); output.clear();
        strike (strokes, 0, 48, 0);
        writer.process (strokes, true, &output, 64, 0);
        std::vector<Event> expected;
        if (previouslyHeld) expected.emplace_back (0, 0x80, 43, 0);
        expected.emplace_back (0, 0x90, 48, 127);
        CHECK (collect (output) == expected);
    }
}

LUTHIER_TEST (RhythmMidiWriter, overflowReleasesHeldNotesAndDropsFutureAttacks)
{
    RhythmMidiWriter writer;
    PlayEventQueue strokes;
    strike (strokes, 2, 67, 0);
    juce::MidiBuffer output;
    output.ensureSize (BoundedMidi::kReserveBytes);
    writer.process (strokes, true, &output, 64, 0);
    strokes.clear();
    output.clear();
    for (int i = 0; i <= RhythmMidiWriter::kCapacity; ++i)
        strike (strokes, i % kMaxStrings, 60, 1000 + i);
    writer.process (strokes, true, &output, 64, 0);
    CHECK ((collect (output) == std::vector<Event>{{0, 0x82, 67, 0}}));
    strokes.clear();
    output.clear();
    writer.process (strokes, true, &output, 2048, 0);
    CHECK (output.isEmpty());
}

LUTHIER_TEST (RhythmMidiWriter, blockedReleaseRetriesBeforeAnyNewAttack)
{
    RhythmMidiWriter writer;
    PlayEventQueue strokes;
    strike (strokes, 0, 45, 0);
    juce::MidiBuffer output;
    output.ensureSize (BoundedMidi::kReserveBytes);
    writer.process (strokes, true, &output, 64, 0);
    output.clear();
    const juce::uint8 off[] {0x8f, 1, 0};
    while (BoundedMidi::add (output, off, 3, 0)) {}
    strokes.clear();
    strike (strokes, 0, 50, 0);
    writer.process (strokes, true, &output, 64, 0);
    output.clear();
    strokes.clear();
    writer.process (strokes, true, &output, 64, 0);
    CHECK ((collect (output) == std::vector<Event>{{0, 0x80, 45, 0}}));
    output.clear();
    writer.process (strokes, true, &output, 64, 0);
    CHECK (output.isEmpty());
}

LUTHIER_TEST (RhythmMidiWriter, invalidAndDeadStrikesDoNotEmitNotes)
{
    RhythmMidiWriter writer;
    PlayEventQueue strokes;
    strike (strokes, -1, 60, 0);
    strike (strokes, kMaxStrings, 60, 0);
    NoteOnEvent dead;
    dead.deadStrike = true;
    strokes.addNoteOn (dead);
    releaseString (strokes, 0, 0);
    juce::MidiBuffer output;
    output.ensureSize (BoundedMidi::kReserveBytes);
    writer.process (strokes, true, &output, 64, 0);
    CHECK (output.isEmpty());
}

LUTHIER_TEST (RhythmMidiRouter, disablingSourceOrGlobalOutputClosesNotesExactlyOnce)
{
    for (bool global : {false, true})
    {
        MidiOutRouter router;
        router.prepare (48000, 64);
        auto config = rhythmConfig();
        auto& source = router.getRhythmBuffer();
        source.addEvent (juce::MidiMessage::noteOn (3, 67, (juce::uint8) 100), 13);
        juce::MidiBuffer output;
        output.ensureSize (BoundedMidi::kReserveBytes);
        StringActivityQueue empty;
        router.emitBounded (output, config, empty, 64, BoundedMidi::kReserveBytes);
        CHECK ((collect (output) == std::vector<Event>{{13, 0x92, 67, 100}}));
        if (global) config.enabled = false;
        else config.rhythmEngine = false;
        source.addEvent (juce::MidiMessage::noteOn (4, 70, (juce::uint8) 100), 15);
        router.emitBounded (output, config, empty, 64, BoundedMidi::kReserveBytes);
        CHECK ((collect (output) == std::vector<Event>{{0, 0x82, 67, 0}}));
        router.emitBounded (output, config, empty, 64, BoundedMidi::kReserveBytes);
        CHECK (output.isEmpty());
        config = rhythmConfig();
        router.emitBounded (output, config, empty, 64, BoundedMidi::kReserveBytes);
        CHECK (output.isEmpty());
    }
}

LUTHIER_TEST (RhythmMidiRouter, tinyDestinationAndZeroFramesRetainReleases)
{
    MidiOutRouter router;
    router.prepare (48000, 64);
    auto config = rhythmConfig();
    for (int note : {48, 50})
        router.getRhythmBuffer().addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
    juce::MidiBuffer output;
    output.ensureSize (BoundedMidi::kReserveBytes);
    StringActivityQueue empty;
    router.emitBounded (output, config, empty, 64, BoundedMidi::kReserveBytes);
    config.enabled = false;
    router.emitBounded (output, config, empty, 0, 9);
    CHECK (output.isEmpty());
    router.emitBounded (output, config, empty, 64, 9);
    CHECK ((collect (output) == std::vector<Event>{{0, 0x80, 48, 0}}));
    config.enabled = true; // Retry must survive re-enabling the source.
    router.emitBounded (output, config, empty, 64, 9);
    CHECK ((collect (output) == std::vector<Event>{{0, 0x80, 50, 0}}));
    router.emitBounded (output, config, empty, 64, 9);
    CHECK (output.isEmpty());
}

LUTHIER_TEST (RhythmMidiRouter, droppedReleaseRetriesBeforePassThrough)
{
    MidiOutRouter router;
    router.prepare (48000, 64);
    auto config = rhythmConfig();
    router.getRhythmBuffer().addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
    juce::MidiBuffer output, incoming;
    output.ensureSize (BoundedMidi::kReserveBytes);
    StringActivityQueue empty;
    router.emitBounded (output, config, empty, 64, 64);
    for (int i = 0; i < 7; ++i)
        incoming.addEvent (juce::MidiMessage::noteOff (16, i), 0);
    config.passThrough = true;
    router.captureInput (incoming);
    router.getRhythmBuffer().addEvent (juce::MidiMessage::noteOff (1, 45), 1);
    router.emitBounded (output, config, empty, 64, 64);
    CHECK (output.getNumEvents() == 7);
    incoming.clear();
    router.captureInput (incoming);
    router.emitBounded (output, config, empty, 64, 64);
    CHECK ((collect (output) == std::vector<Event>{{0, 0x80, 45, 0}}));
}

LUTHIER_TEST (RhythmMidiRouter, seededSwitchAndCapacityStressLeavesNoHeldNotes)
{
    juce::Random random (0x4d494449);
    bool duplicate = false, outOfBlock = false, stuck = false;
    for (int trial = 0; trial < 16; ++trial)
    {
        RhythmMidiWriter writer;
        MidiOutRouter router;
        router.prepare (48000, 64);
        auto config = rhythmConfig();
        std::array<std::array<int, 128>, 16> held {};
        juce::MidiBuffer output;
        output.ensureSize (BoundedMidi::kReserveBytes);
        StringActivityQueue empty;
        auto inspect = [&]
        {
            for (const auto event : output)
            {
                outOfBlock |= event.samplePosition < 0 || event.samplePosition >= 64;
                const auto message = event.getMessage();
                auto& count = held[(size_t) message.getChannel() - 1][(size_t) message.getNoteNumber()];
                if (message.isNoteOn()) { duplicate |= count != 0; ++count; }
                else if (message.isNoteOff()) count = juce::jmax (0, count - 1);
            }
        };
        for (int block = 0; block < 256; ++block)
        {
            PlayEventQueue strokes;
            const int attacks = random.nextInt (18);
            for (int i = 0; i < attacks; ++i)
                strike (strokes, random.nextInt (kMaxStrings), 36 + random.nextInt (48), random.nextInt (512));
            config.enabled = random.nextInt (8) != 0;
            config.rhythmEngine = random.nextInt (8) != 0;
            writer.process (strokes, random.nextInt (8) != 0,
                            config.enabled && config.rhythmEngine ? &router.getRhythmBuffer() : nullptr, 64, 0);
            const int capacities[] {0, 9, 64, 256, BoundedMidi::kReserveBytes};
            router.emitBounded (output, config, empty, 64, capacities[random.nextInt (5)]);
            inspect();
        }
        config.enabled = false;
        router.emitBounded (output, config, empty, 64, BoundedMidi::kReserveBytes);
        inspect();
        for (const auto& channel : held)
            for (int count : channel) stuck |= count != 0;
    }
    CHECK (! duplicate);
    CHECK (! outOfBlock);
    CHECK (! stuck);
}

#if LUTHIER_ALLOCATION_COUNTER
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
LUTHIER_TEST (RhythmMidiRouter, writerAndRouterAllocateNothingAfterPreparation)
{
    RhythmMidiWriter writer;
    MidiOutRouter router;
    router.prepare (48000, 64);
    auto config = rhythmConfig();
    PlayEventQueue strokes;
    strike (strokes, 0, 48, 0);
    juce::MidiBuffer output;
    output.ensureSize (BoundedMidi::kReserveBytes);
    StringActivityQueue empty;
    const auto before = allocationsOnThisThread();
    writer.process (strokes, true, &router.getRhythmBuffer(), 64, 0);
    router.emitBounded (output, config, empty, 64, BoundedMidi::kReserveBytes);
    config.enabled = false;
    router.emitBounded (output, config, empty, 64, BoundedMidi::kReserveBytes);
    const auto after = allocationsOnThisThread();
    CHECK (after == before);
}
#endif
