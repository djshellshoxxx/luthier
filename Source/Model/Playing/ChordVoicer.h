#pragma once

/*  Chord voicing engine (build spec, "Polyphonic / chord mode").

    Identity rule 9: the plugin refuses to voice a chord that no hand could play.
    Given a set of pitches, this finds the assignment of notes to strings that a
    guitarist would actually use - one note per string, a fret span a hand can
    reach, no barre across a gap that the fingers cannot cover - and if the exact
    voicing is impossible it returns the closest playable one rather than a
    physically absurd fingering.

    The search is exhaustive with pruning over at most 12 strings and 8 notes, and
    it runs in a few microseconds, so it can live on the audio thread.
*/

#include "PlayingEvents.h"
#include "TuningEngine.h"
#include <array>
#include <cstdint>

namespace luthier
{

//==============================================================================
/** One note placed on the fretboard. */
struct VoicedNote
{
    int    midiNote     = 60;
    int    stringIndex  = 0;
    double fretPosition = 0.0;
    double velocity     = 0.8;
    bool   valid        = false;
};

//==============================================================================
struct ChordVoicing
{
    static constexpr int kMaxNotes = kMaxStrings;

    std::array<VoicedNote, kMaxNotes> notes {};
    int numNotes = 0;

    int lowestFret = 0;
    int highestFret = 0;
    int fretSpan = 0;
    bool requiresBarre = false;
    bool playable = false;

    /** Notes the voicer could not place at all. */
    int droppedNotes = 0;
};

//==============================================================================
class ChordVoicer
{
public:
    void prepare (const TuningEngine* tuning, int numStrings) noexcept;

    void setNumStrings (int n) noexcept { numStrings = juce::jlimit (1, kMaxStrings, n); }

    /** Forgets where the hand was. The preferred position is carried from chord to
        chord on purpose, so a reset has to clear it or the first render after a
        prepare voices differently from every later one. */
    void reset() noexcept { preferredPosition = 0; occupied = 0; }

    /** Strings already sounding a held note, one bit per string index. The
        voicer never places a note on one of these: a note that arrives while
        another is held must go to a free string, or it kills the held one
        (engine.md "Mode B" - one note per string). Cleared by reset(). */
    void setOccupiedStrings (uint16_t mask) noexcept { occupied = mask; }
    uint16_t getOccupiedStrings() const noexcept { return occupied; }

    /** Maximum fret span a hand can cover. Four is comfortable; five is a stretch
        that a real player will make when the chord needs it. */
    void setMaxFretSpan (int frets) noexcept { maxFretSpan = juce::jlimit (2, 7, frets); }
    int getMaxFretSpan() const noexcept { return maxFretSpan; }

    /** Highest fret the voicer will use. */
    void setMaxFret (int fret) noexcept { maxFret = juce::jlimit (5, 30, fret); }

    /** Lowest fret the voicer will use. This is what a capo is: it does not
        transpose anything, it simply removes every fret below it from play, and
        the string that was open is now stopped at the capo. Fret positions stay
        measured from the nut, which is what the string engine expects. */
    void setMinFret (int fret) noexcept { minFret = juce::jlimit (0, 24, fret); }
    int getMinFret() const noexcept { return minFret; }

    /** Prefer voicings near this fret, so a progression does not jump around the
        neck. Updated to the last chord's position after each call. */
    void setPreferredPosition (int fret) noexcept { preferredPosition = juce::jlimit (0, 24, fret); }
    int getPreferredPosition() const noexcept { return preferredPosition; }

    /** Allows or forbids open strings. Barre chords high up the neck use none. */
    void setAllowOpenStrings (bool allow) noexcept { allowOpen = allow; }

    //==========================================================================
    /** Voices a set of MIDI notes across the strings.

        @param midiNotes    the pitches, in any order
        @param velocities   one per note, may be nullptr for a uniform 0.8
        @param numNotes     how many
        @returns            the best playable voicing found
    */
    ChordVoicing voice (const int* midiNotes, const double* velocities, int numNotes) noexcept;

    /** Places a single note, respecting the preferred hand position. Returns a
        VoicedNote with valid == false if no string can play it. */
    VoicedNote voiceSingleNote (int midiNote, double velocity, int preferStringIndex = -1) noexcept;

    //==========================================================================
    /** Identifies a chord from a set of pitch classes, e.g. "Am7". Empty if the
        set does not match a known chord. Used by the UI, not by the audio path. */
    static juce::String identifyChord (const int* midiNotes, int numNotes);

    /** Number of chord shapes in the built-in library. */
    static int getNumLibraryChords() noexcept;

    /** A library chord shape: fret per string, -1 meaning muted. */
    struct LibraryChord
    {
        const char* name;
        int frets[6];      ///< String 0 = high E.
        int fingers[6];    ///< 0 = open, 1-4 = finger, -1 = muted.
    };

    static const LibraryChord& getLibraryChord (int index) noexcept;

    /** Finds library chords whose name contains `query`, case-insensitively.
        Writes up to `maxResults` indices into `dest`; returns how many. */
    static int searchLibrary (const juce::String& query, int* dest, int maxResults);

private:
    struct Candidate
    {
        int stringIndex;
        int fret;
    };

    static constexpr int kMaxCandidates = kMaxStrings;

    int findCandidates (int midiNote, Candidate* dest) const noexcept;
    double scoreAssignment (const ChordVoicing& v) const noexcept;

    bool search (int noteIndex,
                 const int* sortedNotes,
                 const double* sortedVelocities,
                 int numNotes,
                 bool* stringUsed,
                 ChordVoicing& current,
                 ChordVoicing& best,
                 double& bestScore) noexcept;

    const TuningEngine* tuningEngine = nullptr;
    int numStrings = 6;
    int maxFretSpan = 4;
    int maxFret = 22;
    int minFret = 0;
    int preferredPosition = 0;
    bool allowOpen = true;
    uint16_t occupied = 0;
};

} // namespace luthier
