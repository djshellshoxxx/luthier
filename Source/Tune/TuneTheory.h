#pragma once

/*  The music theory the Tune Builder stands on (tune-builder.md 1, 4 and 5).

    Keys, modes, scales, pitch spelling and the chord vocabulary. None of it
    knows about a Tune: it is the arithmetic the model, the progression tools
    and the melody generators share, kept in one place so that "is this note in
    the key" has exactly one answer.

    The chord vocabulary is not a second table. It is rhythm-engine 2's template
    table in ChordDetector, read through the suffixes it already defines, plus a
    short list of spellings musicians type ("maj", "min", "M7", "-7", "o7") that
    resolve onto those suffixes. A chord the Tune Builder can write is therefore
    always one the rhythm engine can detect when the chord track plays it.

    Beats are quarter notes throughout, the convention Metronome's TimeSignature
    and PerformanceScore already use: a 6/8 bar is three beats long.
*/

#include "../Rhythm/ChordDetector.h"

#include <array>
#include <vector>

namespace luthier
{

//==============================================================================
/** The seven diatonic modes (tune-builder 5, "Modal shift"). */
enum class TuneMode
{
    ionian = 0, dorian, phrygian, lydian, mixolydian, aeolian, locrian,
    numModes
};

/** Lower-case name as written to `.luthiertune`: "ionian", "aeolian", ... */
const char* getTuneModeName (TuneMode mode) noexcept;

/** Accepts the seven names in any case, plus "major" and "minor". */
bool parseTuneMode (const juce::String& text, TuneMode& result);

/** Semitones of each scale degree above the tonic. */
const std::array<int, 7>& getModeIntervals (TuneMode mode) noexcept;

/** True for the modes a musician calls minor (dorian, phrygian, aeolian,
    locrian): the ones whose key is written "Am" rather than "A". */
bool modeHasMinorThird (TuneMode mode) noexcept;

//==============================================================================
namespace tunetheory
{
    /*  Every beat value the Tune Builder stores is snapped to a millionth of a
        beat. That is far below anything audible (a microsecond at 60 bpm) and it
        is what makes tune-builder 15's "byte-identical round trip" hold exactly:
        a snapped value prints as at most six decimals and parses back to the
        same double, so save -> load -> save cannot drift, and load(save(t))
        compares equal to t. Triplets land on 0.333333, 0.666667. */
    constexpr double kBeatResolution = 1.0e-6;

    /** Snaps to kBeatResolution. Non-finite values become 0, and -0 becomes 0,
        so that the text a value prints as is unique. */
    double canonical (double value) noexcept;

    /** 0-11, for any integer. */
    int wrapPitchClass (int pitch) noexcept;

    //==========================================================================
    /** Reads a note name (A-G, either case, optional '#' or 'b') starting at
        `start`. Returns the pitch class and sets `charsUsed`, or returns -1 and
        sets `charsUsed` to 0 when the text does not start with a note. */
    int parsePitchClass (const juce::String& text, int start, int& charsUsed);

    /** "C#" or "Db", as asked. */
    juce::String spellPitchClass (int pitchClass, bool preferFlats);

    /** Whether a key is written with flats: the relative major's signature
        decides, so D minor (F major) and Bb mixolydian (Eb major) use flats. */
    bool keyPrefersFlats (int tonic, TuneMode mode) noexcept;

    /** The key signature as MIDI's key-signature meta event wants it: positive
        for sharps, negative for flats (-7..+7). */
    int keySignatureAccidentals (int tonic, TuneMode mode) noexcept;

    /** "Am", "C", "Eb", "D#m": the tonic spelled for its key, with "m" for the
        minor-third modes (file-formats: `meta.key`). */
    juce::String formatKey (int tonic, TuneMode mode);

    /** Accepts "Am", "C", "F#m", "Bb", "A minor", "D dorian". Returns false and
        leaves the outputs untouched when the text is not a key. */
    bool parseKey (const juce::String& text, int& tonic, TuneMode& mode);

    //==========================================================================
    /** The seven pitch classes of a key, tonic first. */
    std::array<int, 7> getScalePitchClasses (int tonic, TuneMode mode) noexcept;

    /** Bit n set when pitch class n is in the key. */
    uint16_t getScaleMask (int tonic, TuneMode mode) noexcept;

    /** Pentatonic masks for tune-builder 4.1's country phrasing (major
        pentatonic in a major-third mode, minor pentatonic otherwise), and the
        blues scale (minor pentatonic plus the flat fifth) on the tonic. */
    uint16_t getPentatonicMask (int tonic, TuneMode mode) noexcept;
    uint16_t getBluesMask (int tonic) noexcept;

    bool isInMask (int midiNote, uint16_t mask) noexcept;
    bool isInScale (int midiNote, int tonic, TuneMode mode) noexcept;

    /** 0-6, or -1 when the pitch class is not in the key. */
    int getScaleDegree (int midiNote, int tonic, TuneMode mode) noexcept;

    /** The nearest pitch in the mask; on a tie the lower one, so snapping is
        deterministic (tune-builder 3.4, "Snap to key by default"). */
    int snapToMask (int midiNote, uint16_t mask) noexcept;
    int snapToScale (int midiNote, int tonic, TuneMode mode) noexcept;

    /** Moves by whole steps of the mask: +1 is the next pitch up that is in it.
        A pitch outside the mask moves to the nearest mask pitch in that
        direction first. Clamped to 0-127. */
    int moveInMask (int midiNote, int steps, uint16_t mask) noexcept;
    int moveByScaleSteps (int midiNote, int steps, int tonic, TuneMode mode) noexcept;

    //==========================================================================
    /*  The chord vocabulary.

        A chord quality is stored as a canonical suffix: exactly the `suffix` of
        one of ChordDetector's templates. "" is major and "m" is minor, which is
        what the templates call them; `.luthiertune` writes those two as "maj"
        and "min" to match tune-builder 11's example. */

    /** Resolves a canonical suffix or an alias ("maj", "min", "M7", "-7", "o7",
        "sus", "+") to a canonical suffix. Case-sensitive, because "M7" and "m7"
        are different chords. */
    bool resolveQuality (const juce::String& nameOrAlias, juce::String& canonicalSuffix);

    /** The longest quality name (canonical or alias) that `text` starts with
        at `start`. Returns how many characters it used, which is 0 for a plain
        major triad, and sets `canonicalSuffix`. */
    int matchQualityPrefix (const juce::String& text, int start, juce::String& canonicalSuffix);

    /** Interval mask of a canonical suffix, or 0 when it is not one. */
    uint16_t getQualityMask (const juce::String& canonicalSuffix) noexcept;

    bool isKnownQuality (const juce::String& canonicalSuffix) noexcept;

    /** "" -> "maj", "m" -> "min", anything else unchanged. */
    juce::String qualityToFileName (const juce::String& canonicalSuffix);

    /** Semitones above the root that an extension adds ("b9" -> 13, "#11" -> 18,
        "add9" -> 14, "6" -> 9), or -1 when it is not an extension. */
    int getExtensionSemitones (const juce::String& extension) noexcept;
}

} // namespace luthier
