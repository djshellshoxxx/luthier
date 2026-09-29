/*  SPEC-SWEEP rhythm-engine checks (rhythm-engine.md): the audio-thread hand-off
    of patterns (RE-2) and the scheduler behaviours the Phase-1 audit found
    untested. Kept apart from RhythmSchedulerTests.cpp so the sweep's additions
    stay easy to find; the fixture mirrors that file's.
*/

#include "TestFramework.h"

#include "../Rhythm/RhythmEngine.h"
#include "../Rhythm/Patterns.h"
#include "../Rhythm/ChordDetector.h"

#include <atomic>
#include <thread>
#include <limits>
#include <set>
#include <vector>
#include <algorithm>

#if defined (LUTHIER_ALLOCATION_COUNTER)
namespace luthier::tests
{
    long allocationsOnThisThread() noexcept;
}
#endif

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    struct SweepRhythmFixture
    {
        SweepRhythmFixture()
        {
            tuning.prepare (kSr);
            tuning.setNumStrings (6);
            tuning.setTuningPreset (TuningPreset::Standard);

            voicer.prepare (&tuning, 6);

            engine.prepare (kSr, kBlock, &tuning, &voicer);
            engine.setNumStrings (6);
            engine.setEnabled (true);

            RhythmHumanise flat;
            flat.timingMs = 0.0;
            flat.velocityPercent = 0.0;
            flat.missPercent = 0.0;
            flat.ghostPercent = 0.0;
            flat.amount = 0.0;
            engine.setHumanise (flat);
            engine.setStrumDurationMs (1.0);
        }

        void holdNotes (std::initializer_list<int> notes)
        {
            juce::MidiBuffer midi;

            for (int note : notes)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);

            engine.handleMidi (midi, 0);
        }

        void holdChord() { holdNotes ({ 40, 47, 52, 56, 59, 64 }); }

        RhythmTransport transportAt (int block, double bpm = 120.0) const
        {
            RhythmTransport t;
            t.bpm = bpm;
            t.isPlaying = true;
            t.ppqPosition = (double) (block * kBlock) / (60.0 / bpm * kSr);
            return t;
        }

        TuningEngine tuning;
        RubricVoicer voicer;
        RhythmEngine engine;
    };

    RhythmPattern taggedPattern (int variant)
    {
        RhythmPattern p;
        p.setName ("Sweep pattern " + juce::String (variant));
        p.setTags ({ "sweep", "tagged", "rock", "variant-" + juce::String (variant) });
        p.setKind (variant % 2 == 0 ? RhythmPattern::Kind::strum : RhythmPattern::Kind::fingerpick);
        p.setSubdivision (Subdivision::sixteenth);
        p.setLength (16);

        for (int i = 0; i < 16; ++i)
        {
            StrumStep step;
            step.type = (i % 2 == 0) ? StrumType::down : StrumType::up;
            step.dynamic = 0.8;
            step.stringMask = 0x0FFF;
            p.setStrumStep (i, step);

            FingerpickStep f;
            f.active = true;
            f.finger = (Finger) (i % 4);
            f.dynamic = 0.7;
            p.setFingerpickStep (i, f);
        }

        return p;
    }
}

//==============================================================================
/*  RE-2 (engine.md 0): processBlock reads the live pattern by reference. It used
    to copy a whole RhythmPattern (its name and its StringArray of tags) every
    block and every fingerpick step; a copy is an allocation, and the last
    reference dropping on the audio thread is a free. Here a second thread keeps
    publishing tagged patterns while this thread renders: nothing allocates on
    the render thread and every block still plays. */
LUTHIER_TEST (RhythmPatterns, processBlockDoesNotAllocate)
{
    SweepRhythmFixture fixture;
    fixture.holdChord();
    fixture.engine.setPattern (taggedPattern (0));

    PlayEventQueue out;

    // Warm up: the first blocks voice the chord (which may size things once).
    for (int block = 0; block < 8; ++block)
    {
        out.clear();
        fixture.engine.processBlock (kBlock, fixture.transportAt (block), out);
    }

    std::atomic<bool> stop { false };
    std::atomic<int> published { 0 };

    std::thread writer ([&]
    {
        int variant = 1;

        while (! stop.load())
        {
            fixture.engine.setPattern (taggedPattern (variant++));
            published.fetch_add (1);
            std::this_thread::yield();
        }
    });

   #if defined (LUTHIER_ALLOCATION_COUNTER)
    const auto before = allocationsOnThisThread();
   #endif

    int noteOns = 0;

    for (int block = 8; block < 3000; ++block)
    {
        out.clear();
        fixture.engine.processBlock (kBlock, fixture.transportAt (block), out);
        noteOns += out.getNumNoteOns();
    }

   #if defined (LUTHIER_ALLOCATION_COUNTER)
    const auto after = allocationsOnThisThread();
   #endif

    stop.store (true);
    writer.join();

   #if defined (LUTHIER_ALLOCATION_COUNTER)
    CHECK_MSG (after == before, juce::String (after - before) + " allocations on the render thread");
   #endif

    CHECK_MSG (published.load() > 10, "the writer thread only published " + juce::String (published.load()));
    CHECK_MSG (noteOns > 100, "only " + juce::String (noteOns) + " note-ons while patterns changed");

    // The message-thread getter sees the last pattern published, tags and all.
    const auto last = fixture.engine.getPattern();
    CHECK (last.getTags().contains ("sweep"));
}

//==============================================================================
namespace
{
    RhythmPattern everyStep (StrumType type, double swing = 0.5)
    {
        RhythmPattern p;
        p.setName ("Sweep every step");
        p.setKind (RhythmPattern::Kind::strum);
        p.setSubdivision (Subdivision::sixteenth);
        p.setLength (16);
        p.setSwing (swing);

        for (int i = 0; i < 16; ++i)
        {
            StrumStep step;
            step.type = type;
            step.dynamic = 0.9;
            step.stringMask = 0x0FFF;
            p.setStrumStep (i, step);
        }

        return p;
    }

    /** Renders @p blocks and returns every note-on with its absolute sample. */
    std::vector<std::pair<int64_t, NoteOnEvent>> collectNoteOns (SweepRhythmFixture& f, int blocks)
    {
        std::vector<std::pair<int64_t, NoteOnEvent>> all;
        PlayEventQueue out;

        for (int block = 0; block < blocks; ++block)
        {
            out.clear();
            f.engine.processBlock (kBlock, f.transportAt (block), out);

            for (int i = 0; i < out.getNumNoteOns(); ++i)
                all.push_back ({ (int64_t) block * kBlock + out.getNoteOn (i).sampleOffset, out.getNoteOn (i) });
        }

        return all;
    }
}

/*  RE-35 (rhythm-engine 5): swing 0.66 is a triplet feel - the offbeat 16th
    lands two thirds of the way through its pair. */
LUTHIER_TEST (RhythmPatterns, swingDelaysTheOffbeats)
{
    const double perBeat = 60.0 / 120.0 * kSr;   // 24 000 samples
    const double stepSamples = perBeat / 4.0;

    auto firstOffbeat = [&] (double swing)
    {
        SweepRhythmFixture f;
        f.holdChord();
        f.engine.setPattern (everyStep (StrumType::down, swing));

        const auto notes = collectNoteOns (f, 20);
        int64_t earliest = std::numeric_limits<int64_t>::max();

        // The first strum after the downbeat's.
        for (const auto& n : notes)
            if (n.first > (int64_t) (stepSamples * 0.5))
                earliest = std::min (earliest, n.first);

        return earliest;
    };

    const auto straight = firstOffbeat (0.5);
    const auto swung = firstOffbeat (0.66);

    CHECK_MSG (std::llabs (straight - (int64_t) stepSamples) <= 2,
               "straight offbeat at " + juce::String (straight));

    const double expectedDelay = (0.66 - 0.5) * 2.0 * stepSamples;   // 1920 samples
    CHECK_MSG (std::abs ((double) (swung - straight) - expectedDelay) <= 2.0,
               "swing 0.66 moved the offbeat by " + juce::String (swung - straight)
                 + " samples, expected " + juce::String (expectedDelay));
}

/*  RE-18 (rhythm-engine 2): a rake drags muted across the strings and lands on
    an open target at full strength. */
LUTHIER_TEST (RhythmPatterns, rakeEndsOnAnUnmutedTarget)
{
    SweepRhythmFixture f;
    f.holdChord();

    auto pattern = everyStep (StrumType::rest);
    StrumStep rake;
    rake.type = StrumType::rake;
    rake.dynamic = 0.9;
    pattern.setStrumStep (0, rake);
    f.engine.setPattern (pattern);

    auto notes = collectNoteOns (f, 8);   // well inside the first beat
    CHECK (notes.size() >= 3);

    if (notes.size() < 3)
        return;

    std::stable_sort (notes.begin(), notes.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });

    for (size_t i = 0; i + 1 < notes.size(); ++i)
        CHECK_MSG (notes[i].second.technique == Technique::PalmMute,
                   "rake stroke " + juce::String ((int) i) + " was not muted");

    const auto& target = notes.back().second;
    CHECK_MSG (target.technique == Technique::Pluck, "the rake's target string was muted");

    for (size_t i = 0; i + 1 < notes.size(); ++i)
        CHECK (target.velocity >= notes[i].second.velocity - 1.0e-9);
}

/*  RE-14 (rhythm-engine 3): density caps how many strings a voicing sounds. */
LUTHIER_TEST (RhythmPatterns, densityCapsTheStringsSounded)
{
    auto stringsStruck = [] (double density)
    {
        SweepRhythmFixture f;
        f.engine.setVoicingDensity (density);
        f.holdNotes ({ 48, 52, 55, 58, 62 });   // C9
        f.engine.setPattern (everyStep (StrumType::down));

        std::set<int> strings;
        PlayEventQueue out;
        f.engine.processBlock (kBlock, f.transportAt (0), out);

        for (int i = 0; i < out.getNumNoteOns(); ++i)
            strings.insert (out.getNoteOn (i).stringIndex);

        return (int) strings.size();
    };

    const int full = stringsStruck (100.0);
    const int some = stringsStruck (40.0);
    const int few = stringsStruck (20.0);

    CHECK_MSG (full >= 4, "a C9 at full density struck " + juce::String (full) + " strings");
    CHECK_MSG (some <= 3 && some >= 1, "density 40% struck " + juce::String (some) + " of 6");
    CHECK_MSG (few <= 2 && few >= 1, "density 20% struck " + juce::String (few) + " of 6");
    CHECK (full > some && some >= few);
}

/*  RE-12 (rhythm-engine 3): the hand span reaches the voicer and travels with
    the rhythm state. */
LUTHIER_TEST (RhythmPatterns, handSpanIsASettingThatTravels)
{
    SweepRhythmFixture f;
    CHECK (f.engine.getHandSpan() == 5);

    f.engine.setHandSpan (3);
    CHECK (f.engine.getHandSpan() == 3);
    f.engine.setHandSpan (12);
    CHECK (f.engine.getHandSpan() == 7);

    const auto state = f.engine.toVar();

    SweepRhythmFixture g;
    g.engine.fromVar (state);
    CHECK (g.engine.getHandSpan() == 7);

    // A span of three still voices an open E major.
    f.engine.setHandSpan (3);
    f.holdChord();
    f.engine.setPattern (everyStep (StrumType::down));
    PlayEventQueue out;
    f.engine.processBlock (kBlock, f.transportAt (0), out);
    CHECK (out.getNumNoteOns() >= 3);
}

/*  RE-22 (rhythm-engine 2): triplet and dotted grids put each step on its own
    sample - a sixteenth triplet every 1/6 beat, a dotted eighth every 3/4. */
LUTHIER_TEST (RhythmPatterns, tripletAndDottedGridsLandOnTheirSamples)
{
    const double perBeat = 60.0 / 120.0 * kSr;

    for (auto sub : { Subdivision::sixteenthTriplet, Subdivision::eighthDotted, Subdivision::sixteenthDotted })
    {
        SweepRhythmFixture f;
        f.holdChord();

        auto pattern = everyStep (StrumType::down);
        pattern.setSubdivision (sub);
        f.engine.setPattern (pattern);

        auto notes = collectNoteOns (f, 200);
        std::set<int64_t> starts;

        for (const auto& n : notes)
            starts.insert (n.first);

        // One strum per step: the earliest note of each stroke.
        std::vector<int64_t> strokes;

        for (auto t : starts)
            if (strokes.empty() || t - strokes.back() > 200)
                strokes.push_back (t);

        CHECK (strokes.size() >= 6);

        const double stepSamples = perBeat / subdivisionsPerBeat (sub);
        int worst = 0;

        for (size_t i = 0; i < strokes.size(); ++i)
        {
            const double expected = (double) std::llround ((double) strokes[i] / stepSamples) * stepSamples;
            worst = std::max (worst, (int) std::llabs (strokes[i] - (int64_t) std::llround (expected)));
        }

        CHECK_MSG (worst <= 1, juce::String (getSubdivisionName (sub)) + ": worst step error " + juce::String (worst));

        if (strokes.size() >= 2)
            CHECK_NEAR ((double) (strokes[1] - strokes[0]), stepSamples, 1.5);
    }

    CHECK_NEAR (subdivisionsPerBeat (Subdivision::eighthDotted), 4.0 / 3.0, 1.0e-12);
    CHECK (juce::String (getSubdivisionName (Subdivision::sixteenthDotted)) == "16.");
}
