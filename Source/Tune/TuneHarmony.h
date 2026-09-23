#pragma once

/*  Chords for the Tune Builder: where they fall, what notes they hold, how a
    musician types them, and tune-builder.md section 5's progression tools.

    Where they fall (1.1): a section is built from its cells until
    `length_bars` is filled. A progression shorter than the section repeats; a
    last cell whose duration is left unspecified holds to the end instead.

    How they are typed (2.2): `Am F C G`, `| Am7 | D7 | Gmaj7 | Cmaj7 |`,
    `[Verse] Am F C G [Chorus] F C G Am`, with `Am*2` / `Am*0.5` scaling a
    cell. The parser is strict where ProgressionLooper (practice-tools 7) is
    forgiving: a malformed progression is refused with a named error and the
    position it was found at (tune-builder 15), because the text field above the
    pills rewrites the progression live and must never silently drop a chord.
    One addition: `G*fill` marks a last cell that holds to fill the section,
    so every progression the pills can show can also be typed.
*/

#include "TuneModel.h"

namespace luthier
{

//==============================================================================
/** Where one cell of a section's progression plays, section-relative. */
struct ChordSpan
{
    int cellIndex = 0;
    double startBeat = 0.0;
    double endBeat = 0.0;
};

/** The section's cells laid over its length (tune-builder 1.1). Empty when the
    section has no chords. */
std::vector<ChordSpan> resolveChordSpans (const TuneSection& section, double beatsPerBar);

/** The span sounding at a section-relative beat, or -1. */
int findChordSpanAt (const std::vector<ChordSpan>& spans, double beat) noexcept;

//==============================================================================
/** Semitones above the root in chord-tone order: root, third, fifth, seventh
    (or sixth), then the tensions, rising, so C9 is { 0, 4, 7, 10, 14 }.
    `chord_tone_N` (1.2) counts along this, into the next octave past the end. */
std::vector<int> getChordToneIntervals (const ChordCell& cell);

/** The same, as pitch classes. */
std::vector<int> getChordTones (const ChordCell& cell);

/** Bit n set when pitch class n sounds in the chord, bass included. */
uint16_t getChordPitchClassMask (const ChordCell& cell);

/** A playable spread of the chord: the bass (or root) at the lowest pitch at
    or above `lowestNote`, the chord tones stacked above it, tensions on top.
    This is what the chord track holds for the rhythm engine to detect and
    voice (rhythm-engine 2 and 3), so it only has to spell the chord. */
std::vector<int> voiceChord (const ChordCell& cell, int lowestNote, int maxNotes = 6);

/** "Am7", "C(add9)/E", "Bb7(#11)". */
juce::String getChordSymbol (const ChordCell& cell, bool preferFlats);

/** The key's tonic triad, used as "the chord" where a section has none. */
ChordCell makeTonicChord (int tonic, TuneMode mode);

//==============================================================================
/** tune-builder 15: malformed progressions produce named errors. */
enum class ProgressionError
{
    none = 0,
    unknownRoot,          ///< A token that does not start with A-G.
    unknownQuality,       ///< "Cfoo", "C7(x)".
    badSlashBass,         ///< "C/H".
    badDuration,          ///< "Am*0", "Am*x", "Am*999".
    unclosedSection,      ///< "[Verse Am F".
    emptySectionName,     ///< "[] Am".
    unexpectedCharacter,  ///< A stray ']' or '*'.
    emptyBar,             ///< "| Am | | F |".
    tooManyChords
};

/** "UnknownRoot", "UnknownQuality", ... */
const char* getProgressionErrorName (ProgressionError error) noexcept;

struct ParsedSection
{
    juce::String name;                ///< Empty when the text had no [Section] header.
    std::vector<ChordCell> cells;
    int bars = 0;                     ///< Whole bars the cells fill, rounded up.
};

struct ProgressionParseResult
{
    ProgressionError error = ProgressionError::none;
    juce::String token;               ///< The offending text.
    int position = -1;                ///< Character index it starts at.
    std::vector<ParsedSection> sections;

    bool ok() const noexcept { return error == ProgressionError::none; }

    /** "UnknownQuality at 4: 'Cfoo'" - for the notification the text field shows. */
    juce::String describe() const;
};

constexpr int kMaxProgressionChords = 256;

/** Parses shorthand. Without pipes each chord is one bar; inside `| ... |` a
    bar's chords share it equally. `*n` multiplies either. */
ProgressionParseResult parseProgression (const juce::String& text, double beatsPerBar);

/** One chord symbol, without a duration suffix: "Am7/G", "C7(b9,#11)". */
bool parseChordSymbol (const juce::String& text, ChordCell& result, ProgressionError& error);

/** Writes cells back as shorthand that parses to the same cells: one token per
    cell with `*n` where the cell is not one bar. The progression text field
    shows this so pills and text stay in step. */
juce::String formatProgression (const std::vector<ChordCell>& cells, double beatsPerBar, bool preferFlats);

/** Applies typed shorthand to a tune (the text field, 3.2). Without section
    headers it replaces the active section's chords; with them it replaces the
    chords of each named section, adding sections that do not exist yet.
    Section lengths grow to fit and never shrink. Nothing changes on a parse
    error. `tune-chord-edit`. */
ProgressionParseResult applyProgressionText (Tune& tune, int activeSection, const juce::String& text);

//==============================================================================
// tune-builder 5

/** The degree-th diatonic chord of a key (1..7), a triad or a seventh chord:
    the "diatonic palette". */
ChordCell makeDiatonicChord (int tonic, TuneMode mode, int degree, bool seventh, double beats);

/** 1..7 when the chord's root is a scale degree and its third and fifth are in
    the key, else 0. The progression pills colour by this (3.2). */
int getDiatonicDegree (const ChordCell& cell, int tonic, TuneMode mode);

/** "I", "ii", "viio" ("o" stands in for the degree sign), "V7"; "" when not
    diatonic. */
juce::String getRomanNumeral (const ChordCell& cell, int tonic, TuneMode mode);

/** "Suggest next chord": three common next moves from `last` (or from nothing,
    for an empty progression), flavoured by the genre kit's name - blues
    suggests dominant sevenths, jazz suggests seventh chords. */
std::vector<ChordCell> suggestNextChords (const ChordCell* last, int tonic, TuneMode mode,
                                          const juce::String& genreKit, double beats);

/** One right-click "Suggest substitution" offer (3.2). */
struct ChordSubstitution
{
    juce::String name;                ///< "Tritone substitution", "ii-V into next", ...
    std::vector<ChordCell> cells;     ///< Replaces the one cell; may be two.
};

std::vector<ChordSubstitution> suggestSubstitutions (const std::vector<ChordCell>& cells, int index,
                                                     int tonic, TuneMode mode);

struct ReharmonizeOptions
{
    bool tritoneSubstitutions = true;    ///< A dominant resolving down a fifth becomes its bII7.
    bool secondaryDominants = true;      ///< V7/x takes the second half of the chord before x.
    bool modalInterchange = true;        ///< IV -> iv before I in a major key; v -> V7 in a minor one.
};

/** A reharmonization of the cells (5, "Reharmonize"). Deterministic. The total
    duration is unchanged. */
std::vector<ChordCell> reharmonize (const std::vector<ChordCell>& cells, int tonic, TuneMode mode,
                                    const ReharmonizeOptions& options);

/** Replaces one section's progression with its reharmonization. One-shot;
    undo is the snapshot. `tune-chord-edit`. */
bool reharmonizeSection (Tune& tune, int sectionIndex, const ReharmonizeOptions& options);

/** Shifts every chord, every absolute melody, bass and layer pitch, and the
    key by `semitones` (5, "Transpose; melodies follow"). Relative pitches
    follow on their own. Variations move too, so A/B stays in one key.
    `tune-chord-edit`. */
bool transposeTune (Tune& tune, int semitones);

/** Swaps the key's mode (5, "Modal shift"). Diatonic chords move to the same
    degree of the new mode, keeping any seventh; other chords are left alone.
    Absolute melody pitches stay put unless `followMode`, in which case scale
    tones move to the same degree of the new mode. `tune-chord-edit`. */
bool shiftMode (Tune& tune, TuneMode newMode, bool followMode);

} // namespace luthier
