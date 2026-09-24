#pragma once

/*  A Tune as MIDI (tune-builder.md 0.1, 8 and 9.2).

    "The Tune Builder is a control-only module. It writes MIDI into the rhythm
    engine and note engine; no new audio-path DSP." This is that MIDI, and
    nothing else. A TuneTimeline is built from a Tune on the message thread (it
    allocates freely) and is then read-only: the audio thread may walk it with
    forEachEventInRange or renderBlock, which neither allocate nor lock.

    What the timeline holds, per section occurrence in setlist order:

      - chords: each cell's notes held for the cell's length, on the chord
        channel. This is what the rhythm engine reads - it detects the held
        chord and strums or picks it with the section's pattern
        (rhythm-engine 2, 3), exactly as it would a chord the player held.
      - melody, bass and the enabled layers, as notes on their own channels,
        with the realism controllers the engine already maps (CC1 vibrato,
        CC67 palm mute, CC65 slide, CC73 harmonic, pitch bend) when asked for.
      - rhythm changes (pattern, kit, on/off, feel, strum, state boundary) as a
        separate list, because they are settings, not MIDI: the caller applies
        them to the RhythmEngine at the listed positions.

    Positions are quarter notes (the host's ppq) from the tune's start. The
    same timeline writes the exported `.mid` (9.2), so what is exported is what
    was heard.

    One wiring note for the caller: LuthierEngine replaces the interpreter's
    events with the rhythm engine's whenever the rhythm engine is driving, so
    the melody and bass channels must reach the note engine by a path that
    does not go through that replacement, or they will be swallowed while the
    chord channel strums.
*/

#include "TuneMelody.h"
#include "../Notation/PerformanceScore.h"

#include <algorithm>
#include <cmath>

namespace luthier
{

//==============================================================================
enum class TunePart { chords = 0, melody, bass, pad, arpeggio, countermelody, percussion, marker, numParts };

const char* getTunePartName (TunePart part) noexcept;

struct TuneMidiOptions
{
    int chordChannel = 1;
    int melodyChannel = 2;
    int bassChannel = 3;

    /** Layers play on base + LayerType: pad, arpeggio, countermelody, percussion. */
    int layerChannelBase = 4;

    bool includeChords = true;
    bool includeMelody = true;
    bool includeBass = true;
    bool includeLayers = true;

    /** Section names as marker meta events (type 6) at each section start. */
    bool includeMarkers = true;

    /** 9.2: realism (articulation and technique controllers, bends) or plain
        note-on / note-off. Staccato still shortens either way: that is timing,
        not an event. */
    bool includeRealism = true;

    /** Generic-profile text metas ("LUTHIER: BEND ...", midi-export 3) beside
        each technique. For files; the live path does not want them. */
    bool realismTextMetas = false;

    /** Which loop pass an Improvise section renders (4.4). Each section
        occurrence within the pass differs too. */
    int improvisePass = 0;

    /** The chord track's lowest note: E2, so the held chord sits on a guitar
        and the voicer can place it. */
    int chordLowestNote = 40;

    /** The bend range the pitch wheel is scaled for; the MIDI file declares
        it by RPN (midi-export 3). */
    double bendRangeSemitones = 2.0;
};

struct TuneEvent
{
    double ppq = 0.0;              ///< Quarter notes from the tune's start.
    juce::MidiMessage message;     ///< Its timestamp is also `ppq`.
    TunePart part = TunePart::chords;
    int spanIndex = 0;             ///< Which section occurrence it belongs to.
};

/** One section occurrence, where it starts and how long it is. */
struct TuneSectionMarker
{
    double ppq = 0.0;
    double lengthPpq = 0.0;
    int spanIndex = 0;
    int sectionIndex = 0;
    juce::String name;
};

/** The rhythm settings in force from `ppq` on (3.5, 8). */
struct TuneRhythmChange
{
    double ppq = 0.0;
    int spanIndex = 0;
    int sectionIndex = 0;          ///< The section playing; its rhythm may come from a linked one.
    juce::String patternId;
    juce::String genreKitId;
    bool rhythmOn = true;
    double feel = 0.5;
    double strum = 0.5;

    /** 8: reset mod envelopes and the rhythm engine's phase here. Only ever
        true at a section's start. */
    bool stateBoundary = false;
};

//==============================================================================
class TuneTimeline
{
public:
    /** Message thread or worker. */
    static TuneTimeline build (const Tune& tune, const TuneMidiOptions& options = {});

    double getLengthPpq() const noexcept { return lengthPpq; }

    /** Sorted by position; at one position, markers, then note-offs, then
        controller resets, then controller sets, then note-ons. */
    const std::vector<TuneEvent>& getEvents() const noexcept { return events; }

    const std::vector<TuneSectionMarker>& getSections() const noexcept { return sections; }
    const std::vector<TuneRhythmChange>& getRhythmChanges() const noexcept { return rhythmChanges; }

    /** True when an Improvise section means each loop pass needs its own
        timeline (built with the next `improvisePass`). */
    bool needsRebuildEachPass() const noexcept { return improvises; }

    /** The occurrence playing at `ppq`, or -1 past the end without loop. */
    int findSpanAt (double ppq, bool loop) const noexcept;

    /** Every event, or one part's, timestamped in ppq. */
    juce::MidiMessageSequence toSequence() const;
    juce::MidiMessageSequence toSequence (TunePart part) const;

    //==========================================================================
    /*  Calls `callback (const TuneEvent&, double unwrappedPpq)` for every event
        in [fromPpq, toPpq). Real-time safe: no allocation, no locks.

        With `loop` the tune repeats end to end (8, "Loop mode plays the setlist
        end-to-end and repeats") and events are reported at their unwrapped
        positions. The note-offs at the very end of the tune are reported at the
        start of the next pass, ahead of that pass's note-ons, so a note held to
        the end never hangs.

        For every event to land in exactly one block, pass the previous block's
        `toPpq` as the next block's `fromPpq`: the boundary tests then compare
        the same numbers from both sides. */
    template <typename Callback>
    void forEachEventInRange (double fromPpq, double toPpq, bool loop, Callback&& callback) const
    {
        if (events.empty() || ! (toPpq > fromPpq))
            return;

        if (! loop || lengthPpq <= 0.0)
        {
            visit (fromPpq, toPpq, 0.0, true, callback);
            return;
        }

        const auto firstPass = juce::jmax ((int64_t) 0, (int64_t) std::floor (fromPpq / lengthPpq));
        const auto lastPass = (int64_t) std::floor (toPpq / lengthPpq);

        for (auto pass = firstPass; pass <= lastPass; ++pass)
        {
            const double offset = (double) pass * lengthPpq;
            const double lo = fromPpq - offset;
            const double hi = toPpq - offset;

            // The previous pass's closing note-offs belong to this pass's start.
            if (pass > 0 && lo <= 0.0 && 0.0 < hi)
                for (auto it = firstEndEvent(); it != events.end(); ++it)
                    callback (*it, offset);

            visit (lo, hi, offset, false, callback);
        }
    }

    /** Writes [fromPpq, toPpq) into `out` at sample offsets, one block's worth.
        `out` should have been given capacity up front (MidiBuffer::ensureSize)
        so that adding events does not allocate on the audio thread. */
    void renderBlock (double fromPpq, double toPpq, double samplesPerQuarter, int numSamples,
                      bool loop, juce::MidiBuffer& out) const noexcept;

private:
    template <typename Callback>
    void visit (double from, double to, double offset, bool includeEnd, Callback& callback) const
    {
        const double lowest = juce::jmax (0.0, from);
        const double endLimit = includeEnd ? to : juce::jmin (to, endEventThreshold());

        auto it = std::lower_bound (events.begin(), events.end(), lowest,
                                    [] (const TuneEvent& e, double t) { return e.ppq < t; });

        for (; it != events.end() && it->ppq < endLimit; ++it)
            callback (*it, it->ppq + offset);
    }

    /** Events at or after this are the closing note-offs at the tune's end. */
    double endEventThreshold() const noexcept { return lengthPpq - 1.0e-9; }

    std::vector<TuneEvent>::const_iterator firstEndEvent() const noexcept
    {
        return std::lower_bound (events.begin(), events.end(), endEventThreshold(),
                                 [] (const TuneEvent& e, double t) { return e.ppq < t; });
    }

    std::vector<TuneEvent> events;
    std::vector<TuneSectionMarker> sections;
    std::vector<TuneRhythmChange> rhythmChanges;
    double lengthPpq = 0.0;
    bool improvises = false;
};

//==============================================================================
/** 9.2's MIDI export options. The Luthier profile's SysEx encoding belongs to
    midi-export.md's exporter; this writes the Generic profile, with section
    markers and realism as controllers plus LUTHIER: text metas. */
struct TuneMidiFileOptions
{
    enum class TrackSplit
    {
        single = 0,       ///< One instrument track.
        perSection,       ///< One track per section.
        perInstrument,    ///< Guitar (chords, melody, layers) and Bass.
        perString         ///< Needs fretting the voicer does; written as perInstrument for now.
    };

    TrackSplit split = TrackSplit::perInstrument;
    int ticksPerQuarter = 960;    ///< midi-export 1: 960 default, 96-3840.
    TuneMidiOptions midi;
};

/** Format 1: track 0 carries the title, tempo, time and key signature and the
    section markers; instrument tracks follow. */
juce::MidiFile buildTuneMidiFile (const Tune& tune, const TuneMidiFileOptions& options = {});

/** Writes it atomically (temp file, then one rename). Worker thread. */
bool writeTuneMidiFile (const Tune& tune, const juce::File& destination,
                        const TuneMidiFileOptions& options, juce::String& error);

//==============================================================================
/*  9.2 "Luthier profile or Generic profile (midi-export.md)", DECISIONS C-53:
    the tune as the MidiPerformance midi-export's profiles are written from,
    so the TUNE tab's MIDI export is MidiProfiles::exportToFile like every
    other MIDI export in the plugin. It is the timeline - what was heard - at
    the tune's tempo: every channel message at its sample, the bass on part 1
    (the per-instrument split), and a SECTION event at each section occurrence
    (the per-section split). The chords are the held chord track the rhythm
    engine strums, as in the Generic `.mid`; the melody's realism rides as the
    controllers the engine maps when `options.includeRealism`. */
class MidiPerformance;

MidiPerformance buildTunePerformance (const Tune& tune, double sampleRate, const TuneMidiOptions& options = {});

//==============================================================================
/** 0.5 and 9.3: the tune as a PerformanceScore, so notation export needs no
    Tune-specific writer. Melody notes are fretted on the lowest comfortable
    string near the previous note; chord symbols and section names are kept. */
struct TuneScoreOptions
{
    bool includeChordSymbols = true;
    int maxFret = 22;
};

void buildTuneScore (const Tune& tune, PerformanceScore& score, const TuneScoreOptions& options = {});

} // namespace luthier
