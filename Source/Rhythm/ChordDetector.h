#pragma once

/*  Chord detection (rhythm-engine.md section 2).

    Turns a stack of held MIDI notes into a chord symbol. The detector is a
    template matcher over pitch classes: each template is a set of intervals
    above a root, and the best match over all twelve roots wins.

    Confidence is what makes this usable rather than merely clever. A guitarist
    holding three notes of a thirteenth chord has not played a thirteenth, and a
    detector that insisted otherwise would voice something nobody asked for. Below
    the confidence floor the symbol is reported as Unknown and the caller falls
    back to placing the held notes literally.

    No allocation, no strings built on the audio thread: the symbol carries a
    template index and the name is only rendered when something asks for it.
*/

#include "../DSP/Common/DspCommon.h"

#include <array>

namespace luthier
{

//==============================================================================
/** A chord template: a set of semitone intervals above the root. */
struct ChordTemplate
{
    const char* suffix;        ///< What follows the root in the symbol, e.g. "m7".
    const char* longName;      ///< For tooltips and the chord readout.
    uint16_t intervalMask;     ///< Bit n set if semitone n above the root is in the chord.
    uint8_t  noteCount;        ///< How many notes the template names.

    /** Intervals that may be dropped without changing the chord's identity: the
        fifth in almost everything, the root in a rootless voicing. A template
        still matches when only these are missing. */
    uint16_t optionalMask;
};

/** How many templates the detector knows. */
int getNumChordTemplates() noexcept;

const ChordTemplate& getChordTemplate (int index) noexcept;

/** Pitch-class names, sharp spelling. */
const char* getPitchClassName (int pitchClass) noexcept;

//==============================================================================
struct ChordSymbol
{
    static constexpr int kMaxExtensions = 6;

    int  root = -1;              ///< Pitch class 0-11, or -1 when unknown.
    int  bass = -1;              ///< Pitch class of the lowest sounding note.
    int  templateIndex = -1;     ///< Index into the template table, or -1.
    double confidence = 0.0;     ///< matched / heldCount.

    /** Intervals present above the seventh that the template did not name.
        Captured whether or not they end up voiced. */
    std::array<int, kMaxExtensions> extensions {};
    int numExtensions = 0;

    bool isKnown() const noexcept { return root >= 0 && templateIndex >= 0; }

    /** True when the bass is a chord tone other than the root, which is what
        makes the symbol a slash chord. */
    bool isSlash() const noexcept { return bass >= 0 && root >= 0 && bass != root; }

    /** "Am7", "G/B", "Cmaj9". Message thread only - it builds a String. */
    juce::String toString() const;

    bool operator== (const ChordSymbol& other) const noexcept
    {
        return root == other.root && bass == other.bass && templateIndex == other.templateIndex;
    }

    bool operator!= (const ChordSymbol& other) const noexcept { return ! (*this == other); }
};

//==============================================================================
class ChordDetector
{
public:
    /** Notes landing within this window of each other count as one chord, so a
        strummed chord is not read as six separate notes (rhythm-engine 2). */
    static constexpr double kBurstWindowSeconds = 0.030;

    /** Below this, the chord is reported as Unknown. */
    static constexpr double kConfidenceFloor = 0.6;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** Detects from a set of held MIDI notes. `notes` need not be sorted.
        Real-time safe: no allocation, no string building. */
    ChordSymbol detect (const int* midiNotes, int numNotes) const noexcept;

    /** The last symbol detected, for the UI. */
    ChordSymbol getLastSymbol() const noexcept { return lastSymbol; }

    /** Feeds a note-on. Returns true when the burst window has closed and a new
        chord is ready, which is when the caller should re-voice. */
    bool noteOn (int midiNote, int64_t sampleTime) noexcept;
    void noteOff (int midiNote) noexcept;
    void allNotesOff() noexcept;

    /** Call once per block; closes a burst whose window has expired. */
    bool advance (int64_t sampleTime) noexcept;

    int getNumHeldNotes() const noexcept { return numHeld; }
    const int* getHeldNotes() const noexcept { return held.data(); }

private:
    double sr = 44100.0;
    int64_t burstWindowSamples = 1323;

    std::array<int, 24> held {};
    int numHeld = 0;

    int64_t burstStart = -1;
    bool burstOpen = false;

    ChordSymbol lastSymbol;
};

} // namespace luthier
