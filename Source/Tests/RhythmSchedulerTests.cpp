/*  Pattern library and scheduler tests (rhythm-engine.md section 10).

    Separate from RhythmTests.cpp, which covers chord detection: these need a
    prepared engine with a chord held and a transport running, and that fixture
    is worth keeping away from the detector's much simpler setup.
*/

#include "TestFramework.h"

#include "../Rhythm/RhythmEngine.h"
#include "../Rhythm/Patterns.h"
#include "../Rhythm/ChordDetector.h"
#include "../Model/Guitar/GuitarLibrary.h"

#include <limits>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    int buildChordNotes (int root, int templateIndex, int* dest, int maxNotes)
    {
        const auto& t = getChordTemplate (templateIndex);

        int count = 0;

        for (int interval = 0; interval < 12 && count < maxNotes; ++interval)
            if ((t.intervalMask & (uint16_t) (1u << interval)) != 0)
                dest[count++] = 48 + root + interval;

        return count;
    }

    /** An engine with a chord held, a known pattern, and humanisation off, so
        the grid is exactly the grid. */
    struct RhythmFixture
    {
        RhythmFixture()
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

            // A near-instant strum, so a chord's six notes land on one sample and
            // the scheduling test measures the grid rather than the spread.
            engine.setStrumDurationMs (1.0);
        }

        void holdChord()
        {
            juce::MidiBuffer midi;

            for (int note : { 40, 47, 52, 56, 59, 64 })
                midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);

            engine.handleMidi (midi, 0);
        }

        TuningEngine tuning;
        RubricVoicer voicer;
        RhythmEngine engine;
    };

    /** Down-strums on the quarter notes of a 16-step sixteenth-note grid. */
    RhythmPattern quarterNoteDowns()
    {
        RhythmPattern p;
        p.setName ("Test Quarters");
        p.setKind (RhythmPattern::Kind::strum);
        p.setSubdivision (Subdivision::sixteenth);
        p.setLength (16);
        p.setSwing (0.5);

        for (int i = 0; i < 16; ++i)
        {
            StrumStep step;
            step.type = (i % 4 == 0) ? StrumType::down : StrumType::rest;
            step.dynamic = 1.0;
            step.stringMask = 0x0FFF;
            p.setStrumStep (i, step);
        }

        return p;
    }

    double samplesPerBeatAt (double bpm)
    {
        return 60.0 / bpm * kSr;
    }
}

//==============================================================================
/*  Every factory pattern must be well formed: named, non-empty, inside its
    declared length, and tagged so the browser can filter it. */
LUTHIER_TEST (RhythmPatterns, factoryPatternsAreWellFormed)
{
    PatternLibrary library;

    CHECK_MSG (library.getNumPatterns() >= 16,
               "only " + juce::String (library.getNumPatterns()) + " factory patterns");

    int strums = 0, fingerpicks = 0;

    for (int i = 0; i < library.getNumPatterns(); ++i)
    {
        const auto& p = library.getPattern (i);

        CHECK_MSG (p.getName().isNotEmpty(), "pattern " + juce::String (i) + " has no name");
        CHECK_MSG (! p.isEmpty(), p.getName() + " has no events in it");
        CHECK_MSG (p.getLength() > 0 && p.getLength() <= RhythmPattern::kMaxSteps,
                   p.getName() + " has an out-of-range length");
        CHECK_MSG (p.getTags().size() > 0, p.getName() + " carries no tags");
        CHECK (p.getSwing() >= 0.5 && p.getSwing() <= 0.75);

        if (p.getKind() == RhythmPattern::Kind::strum)
            ++strums;
        else
            ++fingerpicks;
    }

    // rhythm-engine 5 names seven fingerpick patterns as the minimum ship set.
    CHECK_MSG (fingerpicks >= 7,
               "only " + juce::String (fingerpicks) + " fingerpick patterns, spec asks for 7");

    CHECK (strums >= 8);

    for (const char* required : { "Travis Picking", "Classical Ascending", "Classical Return",
                                  "Boom Chick", "Piedmont Blues", "Modern Folk",
                                  "Fingerstyle Folk" })
    {
        CHECK_MSG (library.indexOf (required) >= 0,
                   juce::String ("factory pattern '") + required + "' is missing");
    }

    // Tag search has to actually find things, or the browser's filter is a lie.
    CHECK (library.findByTag ("acoustic").size() > 0);
    CHECK (library.findByKind (RhythmPattern::Kind::fingerpick).size() == fingerpicks);
}

//==============================================================================
/*  Patterns round trip through their JSON form unchanged. */
LUTHIER_TEST (RhythmPatterns, patternsRoundTripThroughJson)
{
    PatternLibrary library;

    for (int i = 0; i < library.getNumPatterns(); ++i)
    {
        const auto& original = library.getPattern (i);

        const auto json = juce::JSON::toString (original.toVar(), false);
        const auto reloaded = RhythmPattern::fromVar (juce::JSON::parse (json));

        CHECK (reloaded.getName() == original.getName());
        CHECK (reloaded.getKind() == original.getKind());
        CHECK (reloaded.getLength() == original.getLength());
        CHECK (reloaded.getSubdivision() == original.getSubdivision());
        CHECK_NEAR (reloaded.getSwing(), original.getSwing(), 1.0e-9);
        CHECK (reloaded.getTags().size() == original.getTags().size());

        for (int step = 0; step < original.getLength(); ++step)
        {
            if (original.getKind() == RhythmPattern::Kind::strum)
            {
                const auto a = original.getStrumStep (step);
                const auto b = reloaded.getStrumStep (step);

                CHECK_MSG (a.type == b.type,
                           original.getName() + " step " + juce::String (step) + " changed type");
                CHECK_NEAR (a.dynamic, b.dynamic, 1.0e-9);
                CHECK (a.stringMask == b.stringMask);
            }
            else
            {
                const auto a = original.getFingerpickStep (step);
                const auto b = reloaded.getFingerpickStep (step);

                CHECK (a.active == b.active);

                if (a.active)
                {
                    CHECK (a.finger == b.finger);
                    CHECK_NEAR (a.dynamic, b.dynamic, 1.0e-9);
                }
            }
        }
    }
}

//==============================================================================
/*  Scheduling accuracy: at 120 bpm on a 16th grid, quarter-note strums land
    within one sample of where the transport says they should. */
LUTHIER_TEST (RhythmPatterns, strumSchedulingIsSampleAccurate)
{
    RhythmFixture fixture;
    fixture.holdChord();
    fixture.engine.setPattern (quarterNoteDowns());

    const double bpm = 120.0;
    const double perBeat = samplesPerBeatAt (bpm);

    juce::Array<int64_t> hits;
    juce::SortedSet<int> beatsSeen;

    for (int block = 0; block < 400; ++block)
    {
        PlayEventQueue out;
        out.clear();

        RhythmTransport transport;
        transport.bpm = bpm;
        transport.isPlaying = true;
        transport.ppqPosition = (double) (block * kBlock) / perBeat;

        fixture.engine.processBlock (kBlock, transport, out);

        if (out.getNumNoteOns() == 0)
            continue;

        int earliest = std::numeric_limits<int>::max();

        for (int i = 0; i < out.getNumNoteOns(); ++i)
            earliest = juce::jmin (earliest, out.getNoteOn (i).sampleOffset);

        const int64_t absolute = (int64_t) block * kBlock + earliest;
        const int beat = (int) std::llround ((double) absolute / perBeat);

        if (! beatsSeen.contains (beat))
        {
            beatsSeen.add (beat);
            hits.add (absolute);
        }
    }

    CHECK_MSG (hits.size() >= 6,
               "only " + juce::String (hits.size()) + " strums were scheduled");

    int worstError = 0;

    for (int i = 0; i < hits.size(); ++i)
    {
        const double expected = (double) beatsSeen[i] * perBeat;
        const auto error = (int) std::llabs (hits[i] - (int64_t) std::llround (expected));
        worstError = juce::jmax (worstError, error);
    }

    CHECK_MSG (worstError <= 1,
               "worst scheduling error was " + juce::String (worstError)
                 + " samples, expected at most 1");
}

//==============================================================================
/*  With the transport stopped the engine is silent, unless free-run is on. */
LUTHIER_TEST (RhythmPatterns, silentWhenStoppedUnlessFreeRunning)
{
    RhythmFixture fixture;
    fixture.holdChord();
    fixture.engine.setPattern (quarterNoteDowns());
    fixture.engine.setFreeRun (false);

    int notesWhileStopped = 0;

    for (int block = 0; block < 200; ++block)
    {
        PlayEventQueue out;
        out.clear();

        RhythmTransport transport;
        transport.bpm = 120.0;
        transport.isPlaying = false;

        fixture.engine.processBlock (kBlock, transport, out);
        notesWhileStopped += out.getNumNoteOns();
    }

    CHECK_MSG (notesWhileStopped == 0,
               "the engine played " + juce::String (notesWhileStopped)
                 + " notes with the transport stopped");

    CHECK (! fixture.engine.isDriving());

    // Free-run advances the pattern on its own clock.
    fixture.engine.setFreeRun (true);

    int notesFreeRunning = 0;

    for (int block = 0; block < 200; ++block)
    {
        PlayEventQueue out;
        out.clear();

        RhythmTransport transport;
        transport.bpm = 120.0;
        transport.isPlaying = false;

        fixture.engine.processBlock (kBlock, transport, out);
        notesFreeRunning += out.getNumNoteOns();
    }

    CHECK_MSG (notesFreeRunning > 0, "free-run produced no notes at all");
    CHECK (fixture.engine.isDriving());

    // 200 blocks of 512 at 48 kHz is 2.13 s; at 120 bpm that is four quarter-note
    // strums, each hitting up to six strings.
    CHECK_MSG (notesFreeRunning >= 4 && notesFreeRunning <= 6 * 8,
               "free-run produced " + juce::String (notesFreeRunning)
                 + " notes, which is not the four-ish strums expected");
}

//==============================================================================
/*  Bypass takes effect within one block and releases what was ringing. */
LUTHIER_TEST (RhythmPatterns, bypassIsCleanAndImmediate)
{
    RhythmFixture fixture;
    fixture.holdChord();
    fixture.engine.setPattern (quarterNoteDowns());

    const double perBeat = samplesPerBeatAt (120.0);

    for (int block = 0; block < 60; ++block)
    {
        PlayEventQueue out;
        out.clear();

        RhythmTransport transport;
        transport.bpm = 120.0;
        transport.isPlaying = true;
        transport.ppqPosition = (double) (block * kBlock) / perBeat;

        fixture.engine.processBlock (kBlock, transport, out);
    }

    fixture.engine.setEnabled (false);

    PlayEventQueue out;
    out.clear();

    RhythmTransport transport;
    transport.bpm = 120.0;
    transport.isPlaying = true;
    transport.ppqPosition = 10.0;

    const int emitted = fixture.engine.processBlock (kBlock, transport, out);

    CHECK_MSG (emitted == 0, "a bypassed engine still scheduled notes");
    CHECK_MSG (out.getNumNoteOns() == 0, "a bypassed engine still emitted note-ons");
    CHECK_MSG (out.getNumNoteOffs() > 0,
               "bypassing left the ringing strings without a note-off");
    CHECK (! fixture.engine.isDriving());

    for (int block = 0; block < 20; ++block)
    {
        PlayEventQueue quiet;
        quiet.clear();
        fixture.engine.processBlock (kBlock, transport, quiet);
        CHECK (quiet.getNumNoteOns() == 0);
    }
}

//==============================================================================
/*  Humanisation is deterministic: the same seed gives the same performance. */
LUTHIER_TEST (RhythmPatterns, humanisationIsDeterministic)
{
    auto run = []
    {
        RhythmFixture fixture;
        fixture.holdChord();
        fixture.engine.setPattern (quarterNoteDowns());
        fixture.engine.setSeed (0xC0FFEEull);

        RhythmHumanise h;
        h.timingMs = 12.0;
        h.velocityPercent = 20.0;
        h.missPercent = 10.0;
        h.ghostPercent = 25.0;
        h.amount = 1.0;
        fixture.engine.setHumanise (h);

        const double perBeat = samplesPerBeatAt (120.0);

        std::vector<std::pair<int64_t, double>> events;

        for (int block = 0; block < 200; ++block)
        {
            PlayEventQueue out;
            out.clear();

            RhythmTransport transport;
            transport.bpm = 120.0;
            transport.isPlaying = true;
            transport.ppqPosition = (double) (block * kBlock) / perBeat;

            fixture.engine.processBlock (kBlock, transport, out);

            for (int i = 0; i < out.getNumNoteOns(); ++i)
            {
                const auto& e = out.getNoteOn (i);
                events.emplace_back ((int64_t) block * kBlock + e.sampleOffset, e.velocity);
            }
        }

        return events;
    };

    const auto first = run();
    const auto second = run();

    CHECK_MSG (! first.empty(), "the humanised run produced no events to compare");
    CHECK (first.size() == second.size());

    int differences = 0;

    for (size_t i = 0; i < juce::jmin (first.size(), second.size()); ++i)
        if (first[i].first != second[i].first || first[i].second != second[i].second)
            ++differences;

    CHECK_MSG (differences == 0,
               juce::String (differences) + " of " + juce::String ((int) first.size())
                 + " humanised events differed between two identically seeded runs");
}

//==============================================================================
/*  The voicer places every chord in the vocabulary on every ship guitar, or says
    it cannot - but never produces a physically impossible fingering. */
LUTHIER_TEST (RhythmPatterns, voicerHandlesEveryChordOnEveryGuitar)
{
    const int numTemplates = getNumChordTemplates();

    int cases = 0;
    int voiced = 0;
    juce::StringArray failures;

    for (int guitarIndex = 0; guitarIndex < (int) GuitarType::NumTypes; ++guitarIndex)
    {
        const auto spec = GuitarLibrary::get ((GuitarType) guitarIndex);

        TuningEngine tuning;
        tuning.prepare (kSr);
        tuning.setNumStrings (spec.numStrings);
        tuning.setTuningPreset (spec.tuning);

        for (int s = 0; s < spec.numStrings; ++s)
            tuning.setMaxFrets (s, spec.maxFrets);

        ChordVoicer voicer;
        voicer.prepare (&tuning, spec.numStrings);
        voicer.setMaxFret (spec.maxFrets);

        for (int t = 0; t < numTemplates; ++t)
        {
            for (int root = 0; root < 12; ++root)
            {
                int notes[12];
                const int count = buildChordNotes (root, t, notes, 12);

                const auto voicing = voicer.voice (notes, nullptr, count);
                ++cases;

                uint16_t used = 0;

                for (int i = 0; i < voicing.numNotes; ++i)
                {
                    const auto& note = voicing.notes[(size_t) i];

                    if (! note.valid)
                        continue;

                    if (! juce::isPositiveAndBelow (note.stringIndex, spec.numStrings))
                    {
                        failures.add (juce::String (spec.name) + ": note on string "
                                        + juce::String (note.stringIndex));
                        continue;
                    }

                    if (note.fretPosition < 0.0 || note.fretPosition > (double) spec.maxFrets)
                    {
                        failures.add (juce::String (spec.name) + ": note at fret "
                                        + juce::String (note.fretPosition, 1));
                        continue;
                    }

                    const auto bit = (uint16_t) (1u << note.stringIndex);

                    if ((used & bit) != 0)
                        failures.add (juce::String (spec.name) + ": two notes on string "
                                        + juce::String (note.stringIndex));

                    used = (uint16_t) (used | bit);
                }

                if (voicing.numNotes > 0)
                    ++voiced;
            }
        }
    }

    CHECK_MSG (cases >= 25 * numTemplates,
               "only ran " + juce::String (cases) + " voicing cases");

    CHECK_MSG (failures.isEmpty(),
               juce::String (failures.size()) + " invalid voicings, first few: "
                 + failures.joinIntoString ("; ").substring (0, 400));

    // Not every chord fits every instrument - a seven-note chord has nowhere to
    // go on a four-string bass - but the great majority must.
    const double fraction = (double) voiced / (double) juce::jmax (1, cases);

    CHECK_MSG (fraction > 0.9,
               "only " + juce::String (fraction * 100.0, 1) + "% of chords were voiced at all");
}

//==============================================================================
/*  Voicing styles actually differ: a power chord is not a wide voicing. */
LUTHIER_TEST (RhythmPatterns, voicingStylesProduceDifferentVoicings)
{
    RhythmFixture fixture;
    fixture.holdChord();

    const double perBeat = samplesPerBeatAt (120.0);

    auto noteCountFor = [&fixture, perBeat] (VoicingStyle style)
    {
        fixture.engine.setVoicingStyle (style);
        fixture.engine.setVoicingDensity (100.0);
        fixture.engine.setPattern (quarterNoteDowns());

        int mostInOneStrum = 0;

        for (int block = 0; block < 120; ++block)
        {
            PlayEventQueue out;
            out.clear();

            RhythmTransport transport;
            transport.bpm = 120.0;
            transport.isPlaying = true;
            transport.ppqPosition = (double) (block * kBlock) / perBeat;

            fixture.engine.processBlock (kBlock, transport, out);
            mostInOneStrum = juce::jmax (mostInOneStrum, out.getNumNoteOns());
        }

        return mostInOneStrum;
    };

    const int power = noteCountFor (VoicingStyle::power);
    const int wide = noteCountFor (VoicingStyle::wide);
    const int shell = noteCountFor (VoicingStyle::shell);

    CHECK_MSG (power > 0, "the power style voiced nothing");
    CHECK_MSG (wide > 0, "the wide style voiced nothing");

    CHECK_MSG (power <= 3,
               "a power chord used " + juce::String (power) + " strings, expected at most 3");

    CHECK_MSG (wide >= power,
               "the wide voicing (" + juce::String (wide) + ") was narrower than the power one ("
                 + juce::String (power) + ")");

    CHECK_MSG (shell <= 4,
               "a shell voicing used " + juce::String (shell) + " strings, expected at most 4");
}
