#pragma once

/*  Melody and bass for the Tune Builder (tune-builder.md sections 4 and 6).

    Every generator here is a pure function of a Tune and a seed. The same
    inputs give the same notes on every run and every machine (15: "same seed
    produces byte-identical melody"): randomness comes only from RtRandom, which
    is a fixed xorshift, never from juce::Random's time-seeded default or from
    the order of an unordered container.

    Ground rule 0.3 is enforced here rather than trusted to the caller: every
    generator that writes into a melody keeps the locked notes exactly as they
    were and fits its own notes around them. A locked note is never moved,
    shortened, re-pitched or dropped.

    The bass line (6) is derived, not stored, for every mode but Manual: the
    renderer asks for it each time, so a chord edit is heard in the bass at
    once without a regenerate step the user would have to remember.
*/

#include "TuneHarmony.h"

namespace luthier
{

//==============================================================================
/** How a genre kit shapes the Auto generator (4.1, "Follow the genre kit's
    phrasing profile"). */
enum class MelodyProfile
{
    standard = 0,   ///< Steps along the mode's scale.
    country,        ///< Pentatonic runs: steps along the pentatonic, keeping direction.
    jazz,           ///< Guide tones: each chord change lands on its third or seventh.
    folk,           ///< Stepwise motion that leans toward the chord root.
    blues           ///< The blues scale on the key's tonic.
};

/** Reads the kit's name: "Nashville Country" is country, "Bossa Nova" jazz. */
MelodyProfile getMelodyProfileForKit (const juce::String& genreKitName);

/** tune-builder 4.1: "default 4 notes / bar in most kits". The exceptions are
    the busy styles (bluegrass, funk) and the spacious ones (ambient, dub). The
    panel sets a track's density from this when its kit changes. */
double getKitMelodyDensity (const juce::String& genreKitName);

//==============================================================================
/** Resolves a note's pitch (1.2). `chord` may be null, in which case the key's
    tonic chord stands in. Relative pitches are placed from the chord root in
    octave 4 (C4 = 60). Always 0-127. */
int resolveMelodyPitch (const MelodyPitch& pitch, const ChordCell* chord, int tonic, TuneMode mode);

/** The same for a note in a section, finding the chord it starts under. */
int resolveNotePitch (const Tune& tune, int sectionIndex, const MelodyNote& note);

//==============================================================================
// 4.1 Auto

/** The section's melody regenerated with `seed`: the track's locked notes,
    untouched, plus generated notes that do not overlap them. Sorted by start.
    The track's range and density apply; a section without a track uses the
    defaults. */
std::vector<MelodyNote> generateAutoMelody (const Tune& tune, int sectionIndex, int seed);

/** Runs Auto with the track's current seed and stores the result (creating the
    track if needed). `tune-melody-generate`. */
bool generateMelody (Tune& tune, int sectionIndex);

/** "Regenerate increments the seed" (4.1), then generates. */
bool regenerateMelody (Tune& tune, int sectionIndex);

//==============================================================================
// 4.4 Improvise

/** One loop pass of Improvise: Auto reseeded from the track's seed and the
    pass number, so every pass differs and pass N is the same every time it is
    asked for. Locked notes play as written. */
std::vector<MelodyNote> generateImprovisedPass (const Tune& tune, int sectionIndex, int pass);

/** "Freeze": captures pass N into the section as written notes.
    `tune-melody-record`. */
bool freezeImprovisedPass (Tune& tune, int sectionIndex, int pass);

//==============================================================================
// 7 Countermelody

/** A second line that fills the main melody's gaps: it moves where the melody
    holds or rests, sits below it, and lands on chord tones. */
std::vector<MelodyNote> generateCountermelody (const Tune& tune, int sectionIndex, int seed);

//==============================================================================
// 4.2 Draw and 4.3 Record

/** The record quantise grids of 4.3. */
enum class QuantiseGrid { quarter = 0, eighth, eighthTriplet, sixteenth, sixteenthTriplet, numGrids };

const char* getQuantiseGridName (QuantiseGrid grid) noexcept;   ///< "1/4", "1/8T", ...
double getQuantiseGridBeats (QuantiseGrid grid) noexcept;       ///< 1, 0.5, 1/3, 0.25, 1/6

/** Snaps a beat to the nearest grid line (Draw's click-drag, 3.4). */
double snapBeatToGrid (double beat, double gridBeats) noexcept;

/** Snap to key (3.4: "Snap to key by default"). The lower neighbour wins a tie. */
int snapPitchToKey (int midiNote, int tonic, TuneMode mode) noexcept;

/** A note as it arrived from MIDI in, in section-relative beats. */
struct RecordedNote
{
    double startBeat = 0.0;
    double endBeat = 0.0;
    int pitch = 60;
    int velocity = 100;
};

/** "Quantise on release" (4.3): starts and ends go to the grid, velocity is
    kept. With `followChordChanges`, a note held across a chord change is split
    there and the part under the new chord moves to that chord's nearest chord
    tone (if it is not one already). Recorded notes are the player's hand, so
    they come back locked. */
std::vector<MelodyNote> quantiseRecording (const std::vector<RecordedNote>& notes, QuantiseGrid grid,
                                           const Tune& tune, int sectionIndex,
                                           bool followChordChanges, bool snapToKey);

//==============================================================================
// 4.5 Style transfer

/** Display name for the style menu: "Bluegrass fiddle", "Jazz sax", ... */
const char* getMelodyStyleDisplayName (MelodyStyle style) noexcept;

/** Re-phrases notes in a style: articulation, technique, velocity and small
    timing moves. Never pitch, never the number of notes (4.5). Applied when the
    tune is rendered, so the written melody is untouched. */
std::vector<MelodyNote> applyMelodyStyle (const std::vector<MelodyNote>& notes, MelodyStyle style);

//==============================================================================
// 6 Bass line

/** What "Genre" plays for a kit, and what the other modes are. */
enum class BassPattern { root = 0, rootFifth, walking, boogie, reggae, bossa, funk, drivingEighths };

BassPattern getGenreBassPattern (const juce::String& genreKitName);

/** The section's bass line in section-relative beats, absolute pitches in the
    bass register (E1 upward). Manual returns the stored notes; Off returns
    none. Deterministic. */
std::vector<MelodyNote> generateBassLine (const Tune& tune, int sectionIndex);

/** The same for an explicit pattern, for previews. */
std::vector<MelodyNote> generateBassPattern (const Tune& tune, int sectionIndex, BassPattern pattern);

} // namespace luthier
