/*  Performance capture (notation-export.md 6 and 7.1).

    The capture tests of 7.1 - voiced notes not MIDI, no allocation, overflow
    drops the oldest, off costs nothing, transport and free-play timing, the
    live TAB view, a Luthier-profile round trip - and the take's own behaviour:
    arming, the rolling window, quantisation, techniques, chords, meters and
    the BASS_TECH and slide events.
*/

#include "TestFramework.h"

#include "../Capture/PerformanceCapture.h"
#include "../Export/MidiProfiles.h"
#include "../LuthierEngine.h"

#include <algorithm>
#include <cstring>

/*  notation-export 7.1's no-allocation check measures for real only with the
    counter on. The test target defines LUTHIER_ALLOCATION_COUNTER
    (CMakeLists.txt); until that line lands this TU defines it itself, which
    is harmless once it does. */
#if ! defined (LUTHIER_ALLOCATION_COUNTER)
 #define LUTHIER_ALLOCATION_COUNTER 1
#endif

#include "AllocationCounter.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    CaptureClock stoppedAt (juce::int64 blockStart)
    {
        CaptureClock clock;
        clock.blockStartSample = blockStart;
        clock.sampleRate = kSr;
        clock.transportPlaying = false;
        return clock;
    }

    /** The host rolling from ppq 0 at `bpm`, block by block. */
    CaptureClock rollingAt (juce::int64 blockStart, double bpm = 120.0, int numerator = 4, int denominator = 4)
    {
        CaptureClock clock;
        clock.blockStartSample = blockStart;
        clock.sampleRate = kSr;
        clock.transportPlaying = true;
        clock.bpm = bpm;
        clock.blockStartPpq = (double) blockStart * bpm / (60.0 * kSr);
        clock.timeSigNumerator = numerator;
        clock.timeSigDenominator = denominator;
        return clock;
    }

    juce::int64 sampleOfBeat (double beat, double bpm = 120.0)
    {
        return (juce::int64) std::llround (beat * 60.0 / bpm * kSr);
    }

    struct PlayedNote
    {
        double beat, lengthBeats;
        int stringIndex, fret, midiNote;
    };

    /** Plays notes with the transport rolling, as the engine's hook would
        report them: block by block, each at its own offset. */
    void playMusically (PerformanceCapture& capture, const std::vector<PlayedNote>& played,
                        double bpm = 120.0, int numerator = 4, int denominator = 4,
                        const std::function<void (int offset, juce::int64 sample)>& extra = {})
    {
        juce::int64 end = 0;

        for (const auto& note : played)
            end = juce::jmax (end, sampleOfBeat (note.beat + note.lengthBeats, bpm) + kBlock);

        for (juce::int64 blockStart = 0; blockStart < end; blockStart += kBlock)
        {
            capture.beginBlock (rollingAt (blockStart, bpm, numerator, denominator));

            for (int offset = 0; offset < kBlock; ++offset)
            {
                const auto sample = blockStart + offset;

                for (const auto& note : played)
                {
                    if (sampleOfBeat (note.beat + note.lengthBeats, bpm) == sample)
                        capture.noteOff (offset, note.stringIndex);

                    if (sampleOfBeat (note.beat, bpm) == sample)
                        capture.noteOn (offset, note.stringIndex, note.midiNote, note.fret, 0.8f);
                }

                if (extra)
                    extra (offset, sample);
            }
        }
    }

    //==========================================================================
    struct FlatNote
    {
        double start;
        int string, fret, midi;
        double duration, velocity;
        std::vector<ScoreTechnique> techniques;
    };

    std::vector<FlatNote> flatten (const PerformanceScore& score)
    {
        std::vector<FlatNote> notes;

        const auto& meta = score.getMeta();
        const double beatsPerMeasure = (double) meta.timeSignatureNumerator * 4.0
                                         / (double) juce::jmax (1, meta.timeSignatureDenominator);
        const auto& track = score.getTrack (0);

        for (size_t m = 0; m < track.measures.size(); ++m)
            for (const auto* note : track.measures[m].collectNotes())
                notes.push_back ({ (double) m * beatsPerMeasure + note->startBeat, note->stringIndex, note->fret,
                                   note->midiNote, note->durationBeats, note->velocity, note->techniques });

        std::sort (notes.begin(), notes.end(), [] (const FlatNote& a, const FlatNote& b)
        {
            return a.start < b.start || (a.start == b.start && a.string < b.string);
        });

        return notes;
    }

    juce::String describeTechniques (const std::vector<ScoreTechnique>& techniques)
    {
        juce::StringArray items;

        for (const auto& technique : techniques)
        {
            juce::String item (getTechniqueName (technique.type));
            item << " " << juce::String (technique.value, 3);

            for (const auto& [position, semitones] : technique.curve)
                item << " " << juce::String (position, 3) << ":" << juce::String (semitones, 3);

            items.add (item);
        }

        items.sort (false);
        return items.joinIntoString ("; ");
    }

    /** One block's worth of string activity, kept, for comparing against. */
    struct Activity
    {
        juce::int64 sample;
        int stringIndex, midiNote;
        bool isNoteOn;
    };

    /** An E major chord through the engine, in Poly mode, with capture fed from
        its string activity. Returns the mono render. */
    std::vector<float> renderChord (PerformanceCapture* capture, std::vector<Activity>* activity = nullptr)
    {
        LuthierEngine engine;
        engine.prepare (kSr, kBlock);

        if (capture != nullptr)
            capture->setTuning (PerformanceCapture::getOpenNotes (engine.getTuningEngine(), engine.getNumStrings()),
                                engine.getNumStrings(), 0);

        juce::AudioBuffer<float> buffer (2, kBlock);
        std::vector<float> out;

        for (int block = 0; block < 60; ++block)
        {
            juce::MidiBuffer midi;

            // Every note on channel 1: a naive reading would put them all on
            // one string. The voicer spreads them across six.
            if (block == 0)
                for (int note : { 40, 47, 52, 56, 59, 64 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);

            if (block == 40)
                for (int note : { 40, 47, 52, 56, 59, 64 })
                    midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);

            const auto blockStart = (juce::int64) block * kBlock;

            if (capture != nullptr)
                capture->beginBlock (stoppedAt (blockStart));

            buffer.clear();
            engine.processBlock (buffer, midi);

            const auto& stringActivity = engine.getStringActivity();

            if (activity != nullptr)
                for (int i = 0; i < stringActivity.size(); ++i)
                    activity->push_back ({ blockStart + stringActivity[i].sampleOffset, stringActivity[i].stringIndex,
                                           stringActivity[i].midiNote, stringActivity[i].isNoteOn });

            if (capture != nullptr)
                capture->captureStringActivity (stringActivity);

            for (int i = 0; i < kBlock; ++i)
                out.push_back (0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i)));
        }

        return out;
    }
}

//==============================================================================
/*  7.1: "Capture records voiced notes, not MIDI." */
LUTHIER_TEST (Capture, recordsVoicedNotesNotMidi)
{
    PerformanceCapture capture;
    capture.prepare (kSr);

    std::vector<Activity> voiced;
    renderChord (&capture, &voiced);

    capture.drain();

    const auto& notes = capture.getNotes();
    const auto openNotes = [] { LuthierEngine e; e.prepare (kSr, kBlock);
                                return PerformanceCapture::getOpenNotes (e.getTuningEngine(), e.getNumStrings()); }();

    int voicedOns = 0;

    for (const auto& a : voiced)
        if (a.isNoteOn)
            ++voicedOns;

    CHECK_MSG (voicedOns == 6, juce::String (voicedOns) + " strings started");
    CHECK_MSG ((int) notes.size() == voicedOns, juce::String ((int) notes.size()) + " notes captured");

    juce::SortedSet<int> strings;

    for (const auto& note : notes)
    {
        const auto match = std::find_if (voiced.begin(), voiced.end(), [&note] (const Activity& a)
        {
            return a.isNoteOn && a.sample == note.startSample && a.midiNote == note.midiNote;
        });

        CHECK_MSG (match != voiced.end(), "a captured note the engine did not play");

        if (match != voiced.end())
            CHECK_MSG (match->stringIndex == note.stringIndex,
                       "MIDI " + juce::String (note.midiNote) + " captured on string " + juce::String (note.stringIndex)
                         + ", played on " + juce::String (match->stringIndex));

        CHECK_NEAR (note.fret, note.midiNote - openNotes[(size_t) note.stringIndex], 1.0e-9);
        CHECK (note.fret >= 0.0);
        CHECK (! note.isSounding());

        strings.add (note.stringIndex);
    }

    // The naive mapping - channel 1 is string 0 - would have one string.
    CHECK_MSG (strings.size() == 6, juce::String (strings.size()) + " distinct strings, expected the voicer's six");
}

/*  7.1: "No allocation on the audio thread during 10 000 captured notes." */
LUTHIER_TEST (Capture, capturingTenThousandNotesDoesNotAllocate)
{
    PerformanceCapture capture;
    capture.prepare (kSr);

    static constexpr const char* chordNames[] = { "Am", "F", "C", "G7" };

   #if defined (LUTHIER_ALLOCATION_COUNTER)
    const auto before = allocationsOnThisThread();
   #endif

    for (int i = 0; i < 10000; ++i)
    {
        capture.beginBlock (rollingAt ((juce::int64) i * 64));
        capture.noteOn (3, i % 6, 40 + i % 30, (double) (i % 12), 0.7f, Technique::HammerOn);
        capture.bend (5, i % 6, (double) (i % 100));
        capture.mark (6, i % 6, ScoreTechnique::Type::palmMute);
        capture.chordSymbol (7, chordNames[i % 4]);
        capture.noteOff (40, i % 6);
    }

   #if defined (LUTHIER_ALLOCATION_COUNTER)
    const auto after = allocationsOnThisThread();
    CHECK_MSG (after == before, juce::String (after - before) + " allocations while capturing");
   #endif

    // Five reports a note plus the one meter.
    CHECK (capture.getRecordsWritten() == (juce::uint64) (10000 * 5 + 1));

    capture.drain();
    CHECK (capture.getDroppedCount() == 10000 * 5 + 1 - PerformanceCapture::kRingCapacity);
}

/*  7.1: "Ring overflow drops oldest and counts." */
LUTHIER_TEST (Capture, ringOverflowDropsTheOldestAndCountsExactly)
{
    PerformanceCapture capture;
    capture.prepare (kSr);

    // One meter record, then 10 000 notes one sample apart, and nobody draining.
    for (int i = 0; i < 10000; ++i)
    {
        capture.beginBlock (stoppedAt (i));
        capture.noteOn (0, i % 6, 40 + i % 30, 0.0, 0.5f);
    }

    capture.drain();

    const auto expectedDropped = 10000 + 1 - PerformanceCapture::kRingCapacity;
    const auto& notes = capture.getNotes();

    CHECK_MSG (capture.getDroppedCount() == expectedDropped,
               juce::String (capture.getDroppedCount()) + " dropped, expected " + juce::String (expectedDropped));
    CHECK ((int) notes.size() == PerformanceCapture::kRingCapacity);

    // The oldest are gone - the meter and the first notes - and the newest are there.
    CHECK (capture.getMeters().empty());
    CHECK (! notes.empty() && notes.front().startSample == expectedDropped - 1);
    CHECK (! notes.empty() && notes.back().startSample == 9999);

    // Draining again finds nothing new and loses nothing more.
    CHECK (capture.drain() == 0);
    CHECK (capture.getDroppedCount() == expectedDropped);
}

/*  7.1: "Off costs nothing." No ring write, and the audio is what it is with
    no capture at all. */
LUTHIER_TEST (Capture, offWritesNothingAndLeavesTheAudioAlone)
{
    PerformanceCapture off;
    off.prepare (kSr);
    off.setState (CaptureState::off);

    const auto withOff = renderChord (&off);
    const auto without = renderChord (nullptr);
    const auto control = renderChord (nullptr);

    CHECK_MSG (off.getRecordsWritten() == 0,
               juce::String ((juce::int64) off.getRecordsWritten()) + " records written while off");
    CHECK (off.drain() == 0);
    CHECK (off.getNotes().empty());

    const auto identical = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        return a.size() == b.size() && std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0;
    };

    if (! identical (without, control))
    {
        CHECK_MSG (false, "the engine does not repeat itself from a fresh prepare, so bit-identity cannot be judged");
        return;
    }

    CHECK_MSG (identical (withOff, without), "capture switched off changed the audio");

    // Rolling reads the engine's output and writes nothing back into it.
    PerformanceCapture rolling;
    rolling.prepare (kSr);
    CHECK_MSG (identical (renderChord (&rolling), without), "rolling capture changed the audio");
    CHECK (rolling.getRecordsWritten() > 0);
}

//==============================================================================
/*  7.1: "With the host rolling at 120 bpm, a note on beat 3 is recorded at
    quarter-note position 2.0 within 1 ms." */
LUTHIER_TEST (Capture, transportTimingIsInQuarterNotes)
{
    PerformanceCapture capture;
    capture.prepare (kSr);

    playMusically (capture, { { 2.0, 1.0, 0, 0, 64 } });
    capture.drain();

    CHECK (capture.getNotes().size() == 1);

    if (capture.getNotes().size() == 1)
    {
        const auto& note = capture.getNotes().front();

        CHECK (note.musical);
        CHECK_NEAR (note.startPpq, 2.0, 0.001 * 120.0 / 60.0);   // 1 ms at 120 bpm
        CHECK_NEAR (note.endPpq, 3.0, 0.001 * 120.0 / 60.0);
    }

    CHECK (! capture.getMeters().empty());
    CHECK_NEAR (capture.getMeters().front().bpm, 120.0, 1.0e-3);
}

/*  7.1: "Free-play timing is recorded in seconds when the transport is
    stopped." And 6.4: it is quantisable afterwards. */
LUTHIER_TEST (Capture, freePlayIsInSecondsAndQuantisesAfterwards)
{
    PerformanceCapture capture;
    capture.prepare (kSr);

    // Three notes, a little off the beat at 120 bpm: 0, 0.525 and 1.0167 beats.
    const juce::int64 starts[] = { 0, 12600, 24400 };

    for (int i = 0; i < 3; ++i)
    {
        capture.beginBlock (stoppedAt (starts[i]));
        capture.noteOn (0, i, 60 + i, 5.0, 0.8f);
        capture.beginBlock (stoppedAt (starts[i] + 6000));
        capture.noteOff (0, i);
    }

    capture.drain();

    const auto& notes = capture.getNotes();
    CHECK (notes.size() == 3);

    for (const auto& note : notes)
        CHECK (! note.musical);

    if (notes.size() == 3)
        CHECK_NEAR ((double) notes[1].startSample / kSr, 12600.0 / kSr, 1.0e-12);

    // Unquantised, the score keeps what happened.
    CaptureScoreOptions options;
    options.freeTempoBpm = 120.0;

    PerformanceScore raw;
    capture.toScore (raw, options);
    auto flat = flatten (raw);

    CHECK (flat.size() == 3);

    if (flat.size() == 3)
    {
        CHECK_NEAR (flat[1].start, 0.525, 1.0e-9);
        CHECK_NEAR (flat[2].start, 24400.0 / 24000.0, 1.0e-9);
    }

    // Sixteenths: onto the grid.
    options.quantiseBeats = 0.25;

    PerformanceScore snapped;
    capture.toScore (snapped, options);
    flat = flatten (snapped);

    if (flat.size() == 3)
    {
        CHECK_NEAR (flat[0].start, 0.0, 1.0e-9);
        CHECK_NEAR (flat[1].start, 0.5, 1.0e-9);
        CHECK_NEAR (flat[2].start, 1.0, 1.0e-9);
        CHECK_NEAR (flat[1].duration, 0.25, 1.0e-9);
    }

    // Half strength: half way there.
    options.quantiseStrength = 0.5;

    PerformanceScore half;
    capture.toScore (half, options);
    flat = flatten (half);

    if (flat.size() == 3)
        CHECK_NEAR (flat[1].start, 0.5125, 1.0e-9);

    // Nothing was quantised on the way in (6.5).
    CHECK (capture.getNotes()[1].startSample == 12600);
}

//==============================================================================
/*  7.1: "The live TAB view shows what was played." */
LUTHIER_TEST (Capture, theLiveTabShowsWhatWasPlayed)
{
    PerformanceCapture capture;
    capture.prepare (kSr);

    // Standard tuning, one bar: string 0 fret 3, string 2 fret 5, string 4
    // fret 7, string 5 fret 10, a beat each.
    playMusically (capture, { { 0.0, 1.0, 0, 3, 67 }, { 1.0, 1.0, 2, 5, 60 },
                              { 2.0, 1.0, 4, 7, 52 }, { 3.0, 1.0, 5, 10, 50 } });
    capture.drain();

    const auto tab = capture.renderLiveTab (1);
    CHECK (tab.isNotEmpty());

    juce::StringArray staff;

    for (const auto& line : juce::StringArray::fromLines (tab))
        if (line.isNotEmpty() && juce::CharacterFunctions::isLetter (line[0])
              && line.containsChar ('|') && line.containsChar ('-'))
            staff.add (line);

    CHECK_MSG (staff.size() >= 6, juce::String (staff.size()) + " staff lines in:\n" + tab);

    if (staff.size() < 6)
        return;

    const int columnOf3 = staff[0].indexOf ("3");
    const int columnOf5 = staff[2].indexOf ("5");
    const int columnOf7 = staff[4].indexOf ("7");
    const int columnOf10 = staff[5].indexOf ("10");

    CHECK_MSG (columnOf3 > 0 && columnOf5 > 0 && columnOf7 > 0 && columnOf10 > 0,
               "a played fret is missing from its string:\n" + tab);
    CHECK_MSG (columnOf3 < columnOf5 && columnOf5 < columnOf7 && columnOf7 < columnOf10,
               "the notes are out of order:\n" + tab);

    // Strings nobody played carry no frets.
    CHECK (! staff[1].containsAnyOf ("0123456789"));
    CHECK (! staff[3].containsAnyOf ("0123456789"));
}

/*  7.1: "Capture a phrase, export as Luthier-profile MIDI, re-import, assert
    the note list is identical." */
LUTHIER_TEST (Capture, aCapturedPhraseRoundTripsThroughLuthierMidi)
{
    PerformanceCapture capture;
    capture.prepare (kSr);

    playMusically (capture, { { 0.0, 1.0, 5, 0, 40 }, { 1.0, 0.5, 3, 2, 52 },
                              { 1.5, 0.5, 2, 2, 57 }, { 2.0, 2.0, 1, 3, 62 } },
                   120.0, 4, 4,
                   [&capture] (int offset, juce::int64 sample)
                   {
                       // A palm mute on the low E, and a whole-tone bend up on the B.
                       if (sample == sampleOfBeat (0.25))
                           capture.mark (offset, 5, ScoreTechnique::Type::palmMute);

                       if (sample == sampleOfBeat (2.5))
                           capture.bend (offset, 1, 100.0);

                       if (sample == sampleOfBeat (3.0))
                           capture.bend (offset, 1, 200.0);

                       if (sample == sampleOfBeat (1.0))
                           capture.bassTechnique (offset, 3, "slap", 0.2);
                   });

    capture.drain();

    PerformanceScore original;
    capture.toScore (original);

    const auto performance = capture.toPerformance (kSr);
    const auto bytes = MidiProfiles::exportToMemory (performance, MidiExportOptions {});

    MidiPerformance imported;
    const auto result = MidiProfiles::importFromMemory (bytes.getData(), bytes.getSize(), imported, kSr);

    CHECK_MSG (result.ok, result.error);
    CHECK (result.detectedProfile == MidiProfile::luthier);
    CHECK (imported.countEvents (LuthierEventClass::bassTech) == 1);

    PerformanceScore rebuilt;
    imported.toScore (rebuilt);

    const auto before = flatten (original);
    const auto after = flatten (rebuilt);

    CHECK_MSG (before.size() == 4 && after.size() == before.size(),
               juce::String ((int) before.size()) + " notes captured, " + juce::String ((int) after.size()) + " back");

    for (size_t i = 0; i < juce::jmin (before.size(), after.size()); ++i)
    {
        const auto& a = before[i];
        const auto& b = after[i];
        const auto which = "note " + juce::String ((int) i);

        CHECK_MSG (a.string == b.string && a.fret == b.fret && a.midi == b.midi, which + " moved string, fret or pitch");
        CHECK_NEAR (b.start, a.start, 1.0e-6);
        CHECK_NEAR (b.duration, a.duration, 1.0e-3);
        CHECK_NEAR (b.velocity, a.velocity, 1.0 / 127.0);
        CHECK_MSG (describeTechniques (a.techniques) == describeTechniques (b.techniques),
                   which + ": " + describeTechniques (b.techniques) + " against " + describeTechniques (a.techniques));
    }
}

//==============================================================================
/*  6.3: armed clears and starts from the next note. */
LUTHIER_TEST (Capture, armedStartsCleanFromTheNextNote)
{
    PerformanceCapture capture;
    capture.prepare (kSr);

    capture.beginBlock (stoppedAt (0));
    capture.noteOn (0, 0, 64, 0.0, 0.8f);
    capture.drain();

    CHECK (capture.getNotes().size() == 1);

    capture.setState (CaptureState::armed);
    CHECK (capture.getNotes().empty());

    // The old note ending, and a bend on it, belong to the old take.
    capture.beginBlock (stoppedAt (1000));
    capture.bend (0, 0, 50.0);
    capture.noteOff (10, 0);
    capture.noteOn (100, 2, 55, 0.0, 0.6f);

    capture.drain();

    CHECK (capture.getNotes().size() == 1);

    if (capture.getNotes().size() == 1)
    {
        CHECK (capture.getNotes().front().stringIndex == 2);
        CHECK (capture.getNotes().front().bend.empty());
    }

    // The take still knows its tempo.
    CHECK (! capture.getMeters().empty());
    CHECK (capture.getState() == CaptureState::armed);
}

/*  6.3 / 1: rolling keeps the last N minutes. */
LUTHIER_TEST (Capture, rollingKeepsTheLastMinutes)
{
    PerformanceCapture capture;
    capture.prepare (kSr);
    capture.setRollingMinutes (0.1);    // six seconds

    capture.beginBlock (stoppedAt (0));
    capture.noteOn (0, 0, 64, 0.0, 0.8f);
    capture.beginBlock (stoppedAt ((juce::int64) kSr));
    capture.noteOff (0, 0);
    capture.chordSymbol (0, "E");

    // Twenty seconds later, a note that is still ringing.
    capture.beginBlock (stoppedAt ((juce::int64) (20.0 * kSr)));
    capture.noteOn (0, 1, 59, 0.0, 0.8f);

    capture.drain();

    CHECK (capture.getNotes().size() == 1);
    CHECK (! capture.getNotes().empty() && capture.getNotes().front().midiNote == 59);
    CHECK (capture.getChords().empty());
    CHECK (capture.getMeters().size() == 1);   // the tempo in force is kept

    // An armed take keeps everything.
    capture.setState (CaptureState::armed);
    capture.beginBlock (stoppedAt ((juce::int64) (21.0 * kSr)));
    capture.noteOn (0, 0, 64, 0.0, 0.8f);
    capture.beginBlock (stoppedAt ((juce::int64) (22.0 * kSr)));
    capture.noteOff (0, 0);
    capture.beginBlock (stoppedAt ((juce::int64) (60.0 * kSr)));
    capture.noteOn (0, 1, 59, 0.0, 0.8f);
    capture.drain();

    CHECK (capture.getNotes().size() == 2);
}

/*  6.1: techniques, bends, chord symbols and meters reach the score. */
LUTHIER_TEST (Capture, techniquesBendsChordsAndMetersReachTheScore)
{
    PerformanceCapture capture;
    capture.prepare (kSr);

    const double bpm = 90.0;

    capture.beginBlock (rollingAt (0, bpm, 3, 4));
    capture.chordSymbol (0, "Am");
    capture.noteOn (0, 2, 57, 2.0, 0.8f, Technique::HammerOn);
    capture.mark (10, 2, ScoreTechnique::Type::palmMute);

    capture.beginBlock (rollingAt (sampleOfBeat (1.0, bpm), bpm, 3, 4));
    capture.bend (0, 2, 200.0);

    capture.beginBlock (rollingAt (sampleOfBeat (2.0, bpm), bpm, 3, 4));
    capture.noteOff (0, 2);
    capture.chordSymbol (0, "Am");   // unchanged: not a new symbol
    capture.chordSymbol (1, "Dm");

    capture.drain();

    CHECK (capture.getChords().size() == 2);

    PerformanceScore score;
    capture.toScore (score);

    CHECK_NEAR (score.getMeta().tempoBpm, 90.0, 1.0e-3);
    CHECK (score.getMeta().timeSignatureNumerator == 3);
    CHECK (score.getMeta().timeSignatureDenominator == 4);

    const auto notes = flatten (score);
    CHECK (notes.size() == 1);

    if (notes.size() == 1)
    {
        const auto& note = notes.front();
        CHECK (note.string == 2 && note.fret == 2 && note.midi == 57);
        CHECK_NEAR (note.duration, 2.0, 1.0e-6);

        bool hammer = false, palm = false, bend = false;

        for (const auto& technique : note.techniques)
        {
            hammer = hammer || technique.type == ScoreTechnique::Type::hammerOn;
            palm = palm || technique.type == ScoreTechnique::Type::palmMute;

            if (technique.type == ScoreTechnique::Type::bend)
            {
                bend = true;
                CHECK_NEAR (technique.value, 2.0, 1.0e-6);
                CHECK (! technique.curve.empty() && technique.curve.front().first > 0.4
                         && technique.curve.front().first < 0.6);
            }
        }

        CHECK (hammer && palm && bend);
    }

    const auto& firstMeasure = score.getTrack (0).measures.front();
    CHECK (! firstMeasure.chordSymbols.empty() && firstMeasure.chordSymbols.front().second == "Am");
}

/*  6.1: the BASS_TECH and slide events become Luthier events for MIDI export. */
LUTHIER_TEST (Capture, bassAndSlideEventsBecomeLuthierEvents)
{
    PerformanceCapture capture;
    capture.prepare (kSr);

    capture.beginBlock (stoppedAt (0));
    capture.noteOn (0, 3, 40, 0.0, 0.9f);
    capture.bassTechnique (0, 3, "slap", 0.15);
    capture.slideBar (100, 7.5, "full");
    capture.beginBlock (stoppedAt (24000));
    capture.noteOff (0, 3);
    capture.drain();

    CHECK (capture.getEvents().size() == 2);

    const auto performance = capture.toPerformance (kSr);

    CHECK (performance.countEvents (LuthierEventClass::bassTech) == 1);
    CHECK (performance.countEvents (LuthierEventClass::slideBar) == 1);

    for (const auto& event : performance.getEvents())
    {
        if (event.eventClass == LuthierEventClass::bassTech)
        {
            CHECK (event.get ("tech") == "slap");
            CHECK (event.getInt ("str") == 3);
            CHECK_NEAR (event.getReal ("pos"), 0.15, 1.0e-6);
            CHECK (event.part == 1);
        }
        else if (event.eventClass == LuthierEventClass::slideBar)
        {
            CHECK_NEAR (event.getReal ("pos"), 7.5, 1.0e-6);
            CHECK (event.get ("pressure") == "full");
        }
    }
}
