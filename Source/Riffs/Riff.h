#pragma once

/*  A riff, lick, strum part or bass line (riff-library.md 3 and 4).

    The stored form is the `.luthierriff` JSON. In memory a riff is its
    metadata plus its notes as PerformanceScore notes (with absolute start
    beats), so the tab reader, MidiPerformance::fromScore, the notation
    exporters and the tab view all take a riff through toScore() without an
    adapter. Strums and BASS_TECH events are kept alongside.

    toJson() writes the canonical layout that Tools/generate_factory_riffs.py
    writes, byte for byte: a factory file loaded and saved again is
    identical (RL-03). Change one, change both.

    Message thread or worker only: nothing here is for the audio thread.
*/

#include "../Notation/PerformanceScore.h"

#include <vector>

namespace luthier
{

//==============================================================================
/** The riff library's fixed vocabulary (riff-library 2.1, 3). */
namespace RiffVocabulary
{
    struct Genre { const char* id; const char* folder; const char* displayName; };

    /** The twelve genres, in catalog order. */
    const std::vector<Genre>& genres();
    int indexOfGenre (const juce::String& id);

    /** riff, lick, strum, bass. */
    const juce::StringArray& types();

    /** guitar6 ... bass6. */
    const juce::StringArray& instruments();
    int stringsForInstrument (const juce::String& instrument);
    bool isBassInstrument (const juce::String& instrument);

    /** The note technique tokens, in the wire order of docs/MIDI_EXPORT_LUTHIER_PROFILE.md
        6 (MidiPerformance.cpp's kTechniques). */
    const juce::StringArray& noteTechniques();

    /** Every token a riff's `techniques` may hold: the note tokens, then
        `strum`, then the bass techniques (slap, pop, thump, lhslap). */
    const juce::StringArray& allTechniques();

    /** A note token's ScoreTechnique type; false for an unknown token. */
    bool typeForToken (const juce::String& token, ScoreTechnique::Type& type);
    const char* tokenForType (ScoreTechnique::Type type);

    /** The display name of a technique token ("Hammer-On"). */
    juce::String techniqueDisplayName (const juce::String& token);

    /** ScaleType names as the file writes them, ionian ... blues, then chromatic. */
    const juce::StringArray& scales();
    juce::String scaleDisplayName (const juce::String& scale);

    const juce::StringArray& feels();

    /** 0-11 for a root spelling ("A", "Bb", "F#"); -1 if unknown. */
    int pitchClassOfRoot (const juce::String& root);
    /** The spelling the UI uses for a pitch class (sharps, except Bb and Eb). */
    juce::String rootName (int pitchClass);
}

//==============================================================================
struct RiffStrum
{
    double beat = 0.0;
    bool down = true;
    double cv = 200.0;            ///< crossing velocity, strings per second
    int mask = 0;                 ///< bit 0 is string 0 (the highest)
    juce::String striker { "pick" };
    double mute = 0.0;

    bool operator== (const RiffStrum& o) const noexcept;
};

struct RiffBassTech
{
    double beat = 0.0;
    int str = 0;
    juce::String tech { "slap" }; ///< slap, pop, thump, lhslap
    double pos = 0.5;
    double force = 0.8;

    bool operator== (const RiffBassTech& o) const noexcept;
};

struct RiffChord
{
    double beat = 0.0;
    juce::String symbol;
};

struct RiffMeta
{
    juce::String id, name;
    juce::String author { "Factory" };
    juce::String origin { "original" };
    juce::StringArray tags;
    juce::String created, modified;
    juce::String versionCreated, versionModified;
    juce::String notes;
};

//==============================================================================
struct Riff
{
    // file-formats 14 / riff-library 3: above these a file is refused.
    static constexpr int kMaxNotes = 4096;
    static constexpr double kMaxBeats = 64.0;
    static constexpr double kMinTempo = 30.0, kMaxTempo = 300.0;
    static constexpr int kMaxFret = 36;

    RiffMeta meta;

    juce::String type { "lick" }, genre { "rock" }, instrument { "guitar6" };
    std::vector<int> tuning { 64, 59, 55, 50, 45, 40 };   ///< open-string MIDI notes, highest first
    juce::String tuningName { "Standard" };
    int capo = 0;

    juce::String keyRoot { "A" };
    juce::String scale { "chromatic" };

    double tempoBpm = 120.0;
    int meterNumerator = 4, meterDenominator = 4;
    double lengthBeats = 4.0;
    juce::String feel { "straight" };
    int difficulty = 1;

    /** As stored. The loader recomputes and filtering uses computeTechniques(). */
    juce::StringArray techniques;
    std::vector<RiffChord> chords;

    /** startBeat is from the riff's start (not the measure's); stringIndex 0
        is the highest string; midiNote and pitchHz are derived from the
        tuning, capo and fret. */
    std::vector<ScoreNote> notes;
    std::vector<RiffStrum> strums;
    std::vector<RiffBassTech> bassTech;

    juce::String source;

    //==========================================================================
    int getNumStrings() const noexcept { return (int) tuning.size(); }
    bool isBass() const { return RiffVocabulary::isBassInstrument (instrument); }
    double getBeatsPerBar() const noexcept;
    int getNumBars() const noexcept;
    int getRootPitchClass() const { return RiffVocabulary::pitchClassOfRoot (keyRoot); }

    /** The sounding MIDI note of a string and fret on this riff's instrument. */
    int pitchOf (int stringIndex, int fret) const noexcept;

    /** Fills midiNote and pitchHz of every note from the tuning and capo. */
    void updatePitches();

    /** The set riff-library 3 defines: the note tokens used (wire order), then
        `strum` if there are strums, then the bass techniques used. */
    juce::StringArray computeTechniques() const;

    /** The riff as a one-track score, measures at the riff's metre. */
    PerformanceScore toScore (const juce::String& title = {}) const;

    /** FEAT2-TAB: the inverse of toScore, so an imported tab can be compiled and
        played through the RiffPlayer. Takes one track of a parsed score, flattens
        its measures into absolute-beat notes, and carries over tuning, capo,
        tempo and metre. Everything is clamped to the riff's limits (kMaxNotes,
        kMaxBeats, kMaxFret) so the result is always safe to compile. */
    static Riff fromScore (const PerformanceScore& score, int trackIndex = 0);

    //==========================================================================
    /** The canonical `.luthierriff` bytes. */
    juce::String toJson() const;

    /** Reads a `.luthierriff`. Refuses a wrong magic, a missing schema and
        anything past the limits; on refusal `out` is unchanged. `warnings`
        collects soft problems (a stored technique set that does not match). */
    static juce::Result fromJson (const juce::String& text, Riff& out, juce::StringArray* warnings = nullptr);

    static juce::Result loadFromFile (const juce::File& file, Riff& out, juce::StringArray* warnings = nullptr);

    /** Atomic (file-formats 13): written beside the target, then moved over it. */
    juce::Result saveToFile (const juce::File& file) const;
};

//==============================================================================
/** The canonical JSON writer shared by riffs and the catalog: sorted keys,
    one-space indent, arrays of scalars and objects inside arrays on one line,
    reals with at most six decimals. Tools/generate_factory_riffs.py `emit`. */
namespace RiffJson
{
    juce::String formatReal (double value);
    juce::String quote (const juce::String& text);
    juce::String write (const juce::var& value);
}

} // namespace luthier
