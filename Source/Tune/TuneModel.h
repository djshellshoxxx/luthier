#pragma once

/*  The Tune Builder's data model (tune-builder.md section 1).

    A Tune is a plain value: copy it, compare it, keep two of it. That is the
    whole undo design (tune-builder 0.3, "Nothing is destructive"): the caller
    snapshots the Tune before an edit and after it, and an undo entry is the
    pair. Every edit is therefore a method (or, for the harmony and melody tools,
    a free function taking Tune&) that returns whether anything changed, so a
    no-op never becomes an undo entry. The comment on each names the entry
    class action-and-undo.md 3.9 assigns it.

    Nothing here touches audio, files or the message thread's UI state; the
    model can be built and edited from a test or a worker thread. The only
    audio-adjacent code in the Tune Builder is TuneTimeline, which reads a built
    timeline and writes MIDI (tune-builder 0.1: control only).

    Forward compatibility (file-formats 0.3): every struct that is a JSON object
    in `.luthiertune` carries `extra`, the fields a newer version wrote that this
    one does not know. They are kept on load and written back on save, so
    opening a file in an older Luthier and saving it loses nothing.

    Units: beats are quarter notes (a 6/8 bar is 3 beats), positions inside a
    section are relative to the section's start, and tempo is quarter notes
    per minute - what a host's AudioPlayHead reports.
*/

#include "TuneTheory.h"

#include <optional>
#include <vector>

namespace luthier
{

//==============================================================================
/** ChordCell.emphasis (tune-builder 1.1). */
enum class ChordEmphasis { normal = 0, accent, ghost, numEmphases };

/** MelodyNote.articulation and MelodyTrack.articulation_default (1.2).
    `inherit` is only meaningful on a note: it defers to the track default. */
enum class NoteArticulation { inherit = 0, natural, legato, staccato, palmMuted, letRing, numArticulations };

/** MelodyNote.technique (1.2). */
enum class NoteTechnique { none = 0, bend, slide, hammerOn, pullOff, vibrato, harmonic, numTechniques };

/** Which of the entry methods last wrote a melody (tune-builder 2.3, 4, 13).
    `autoGenerate` is written "auto" in the file. */
enum class MelodySource { draw = 0, autoGenerate, record, improvise, sing, numSources };

/** The bass options of tune-builder 6. `off` is the spec's `bass_track: null`. */
enum class BassMode { off = 0, root, rootFifth, walking, genre, manual, numModes };

/** tune-builder 7's four layers. */
enum class LayerType { pad = 0, arpeggio, countermelody, percussion, numTypes };

/** The section strip's colour tag (3.3, "Set as intro / verse / ..."). */
enum class SectionRole { none = 0, intro, verse, chorus, bridge, outro, numRoles };

/** tune-builder 4.5's style-transfer library. Phrasing only, never pitch. */
enum class MelodyStyle { none = 0, bluegrassFiddle, jazzSax, bluesGuitar, classicalGuitar, countryChicken, numStyles };

/** action-and-undo.md 3.9's entry classes, so an undo entry can be labelled
    with the class the spec names. */
enum class TuneEditClass { sectionEdit = 0, chordEdit, melodyEdit, melodyRecord, melodyGenerate, other };

/** The note lists one piano roll edits (tune-builder 3.4, 6 "Manual: same
    piano-roll editor as melody", 7 "Countermelody: a second melody track"):
    the melody track, the bass line and the countermelody layer. */
enum class TuneNotePart { melody = 0, bass, countermelody, numParts };

const char* getTuneNotePartName (TuneNotePart) noexcept;   ///< "melody", "bass", "countermelody"

/** The file names of the enums above ("palm_muted", "root_fifth", ...), and
    their inverses. The parsers accept exactly what the namers produce. */
const char* getChordEmphasisName (ChordEmphasis) noexcept;
const char* getNoteArticulationName (NoteArticulation) noexcept;
const char* getNoteTechniqueName (NoteTechnique) noexcept;
const char* getMelodySourceName (MelodySource) noexcept;
const char* getBassModeName (BassMode) noexcept;
const char* getLayerTypeName (LayerType) noexcept;
const char* getSectionRoleName (SectionRole) noexcept;
const char* getMelodyStyleName (MelodyStyle) noexcept;
const char* getTuneEditClassName (TuneEditClass) noexcept;   ///< "tune-section-edit", ...

bool parseChordEmphasis (const juce::String&, ChordEmphasis&);
bool parseNoteArticulation (const juce::String&, NoteArticulation&);
bool parseNoteTechnique (const juce::String&, NoteTechnique&);
bool parseMelodySource (const juce::String&, MelodySource&);
bool parseBassMode (const juce::String&, BassMode&);
bool parseLayerType (const juce::String&, LayerType&);
bool parseSectionRole (const juce::String&, SectionRole&);
bool parseMelodyStyle (const juce::String&, MelodyStyle&);

/** Equality for the unknown-field sets. juce::var compares objects by
    identity, so two loads of the same file would never be equal; this
    compares names in order and values by their JSON text instead. */
bool tuneExtrasEqual (const juce::NamedValueSet& a, const juce::NamedValueSet& b);

//==============================================================================
/** One chord of a progression (tune-builder 1.1). */
struct ChordCell
{
    int root = 0;                     ///< Pitch class 0-11.
    juce::String quality;             ///< Canonical suffix (TuneTheory): "" major, "m" minor, "7", "m7b5", ...
    int bass = -1;                    ///< Slash-chord bass pitch class; -1 means the root.
    juce::StringArray extensions;     ///< "9", "#11", "add9", ...

    /** Quarter-note beats. Zero or less on the last cell of a section means
        "hold to fill" (1.1: "the builder holds it to fill"); anywhere else it
        is read as one bar. */
    double durationBeats = 4.0;

    juce::String strumOverride;       ///< A pattern name; empty means the section's pattern.
    ChordEmphasis emphasis = ChordEmphasis::normal;

    /** The pill menu's "Lock" (3.2): a locked cell is left alone by
        Reharmonize and the substitution offers, as a locked note is by the
        melody generators (0.3). Written as `locked` when true. */
    bool locked = false;

    juce::NamedValueSet extra;

    static ChordCell make (int root, const juce::String& quality, double beats = 4.0);

    bool holdsToFill() const noexcept { return durationBeats <= 0.0; }

    /** Known quality, known extensions, pitch classes in range. */
    bool isValid() const;

    bool operator== (const ChordCell& other) const;
    bool operator!= (const ChordCell& other) const { return ! (*this == other); }
};

//==============================================================================
/** A melody note's pitch: absolute, or relative to the chord it lands on
    (1.2), so a melody can follow chord changes. */
struct MelodyPitch
{
    enum class Kind
    {
        absolute = 0,   ///< `value` is a MIDI note; written as a number.
        rootOffset,     ///< `value` semitones above the chord root in octave 4; "root+7".
        chordTone       ///< The value-th chord tone, 1 = root, counting up and on into the next octave; "chord_tone_3".
    };

    Kind kind = Kind::absolute;
    int value = 60;

    static MelodyPitch absolute (int midiNote) noexcept     { return { Kind::absolute, midiNote }; }
    static MelodyPitch rootOffset (int semitones) noexcept  { return { Kind::rootOffset, semitones }; }
    static MelodyPitch chordTone (int index) noexcept       { return { Kind::chordTone, index }; }

    bool isAbsolute() const noexcept { return kind == Kind::absolute; }

    /** 69, "root+7", "root-5", "root", "chord_tone_3". */
    juce::var toVar() const;

    /** The inverse of toVar. False for anything toVar could not have written. */
    static bool fromVar (const juce::var& v, MelodyPitch& result);

    bool operator== (const MelodyPitch& o) const noexcept { return kind == o.kind && value == o.value; }
    bool operator!= (const MelodyPitch& o) const noexcept { return ! (*this == o); }
};

//==============================================================================
/** tune-builder 1.2's MelodyNote. Also the note type of the manual bass line
    and the countermelody layer, which are "the same piano-roll editor" (6). */
struct MelodyNote
{
    double startBeat = 0.0;           ///< From the section's start.
    double durationBeats = 1.0;
    MelodyPitch pitch;
    int velocity = 100;               ///< 1-127.
    NoteArticulation articulation = NoteArticulation::inherit;
    NoteTechnique technique = NoteTechnique::none;

    /** A manual edit. Regenerating leaves locked notes byte-identical
        (tune-builder 0.3, 3.4, 15). */
    bool locked = false;

    juce::NamedValueSet extra;

    static MelodyNote make (double start, double duration, int midiNote, int velocity = 100);

    double getEndBeat() const noexcept { return startBeat + durationBeats; }

    bool operator== (const MelodyNote& other) const;
    bool operator!= (const MelodyNote& other) const { return ! (*this == other); }
};

//==============================================================================
struct MelodyTrack
{
    std::vector<MelodyNote> notes;

    int stringHint = 0;               ///< 0 = "auto", else 1..N biases the voicer (1.2).
    NoteArticulation articulationDefault = NoteArticulation::natural;

    MelodySource source = MelodySource::draw;
    int seed = 1;                     ///< tune-builder 4.1: "Regenerate increments the seed".
    double density = 4.0;             ///< Notes per bar (4.1, `melody_density`).
    int rangeLow = 48;                ///< C3 (4.1, `melody_range` default C3 to G5).
    int rangeHigh = 79;               ///< G5.
    bool followChords = false;        ///< 4.3, "Follow chord changes".

    juce::NamedValueSet extra;

    int getNumLockedNotes() const noexcept;

    bool operator== (const MelodyTrack& other) const;
    bool operator!= (const MelodyTrack& other) const { return ! (*this == other); }
};

//==============================================================================
/** tune-builder 6. Every mode but `manual` is derived from the chords when the
    tune is rendered, so the bass always follows a chord edit; only a manual
    line stores notes. */
struct BassTrack
{
    BassMode mode = BassMode::off;
    std::vector<MelodyNote> notes;

    juce::NamedValueSet extra;

    bool isActive() const noexcept { return mode != BassMode::off; }

    bool operator== (const BassTrack& other) const;
    bool operator!= (const BassTrack& other) const { return ! (*this == other); }
};

//==============================================================================
/** tune-builder 7: "Every layer has its own on / off, volume, and pan." */
struct TuneLayer
{
    LayerType type = LayerType::pad;
    bool enabled = true;
    double volume = 0.8;              ///< 0..1.
    double pan = 0.0;                 ///< -1 (left) .. +1 (right).
    juce::String patternId;           ///< Arpeggio: a fingerpick pattern name, for the rhythm engine.
    int seed = 1;                     ///< Countermelody generation.
    std::vector<MelodyNote> notes;    ///< Countermelody: the generated (or edited) line.

    juce::NamedValueSet extra;

    bool operator== (const TuneLayer& other) const;
    bool operator!= (const TuneLayer& other) const { return ! (*this == other); }
};

//==============================================================================
/** One section of the arrangement (tune-builder 1). */
struct TuneSection
{
    juce::String name { "Section" };
    int lengthBars = 4;

    std::vector<ChordCell> chords;

    juce::String rhythmPatternId;     ///< A PatternLibrary name (rhythm-engine 6).
    juce::String genreKitId;          ///< A GenreKitLibrary name (rhythm-engine 7).

    std::optional<MelodyTrack> melody;   ///< `melody_track: MelodyTrack | null`.
    BassTrack bass;                       ///< `bass_track`; mode off is null.
    std::vector<TuneLayer> layers;        ///< `active_layers`, at most one of each type.

    SectionRole role = SectionRole::none;
    bool rhythmOn = true;             ///< The rhythm strip's "On" (3.1).
    juce::String rhythmLinkedTo;      ///< 3.5 "Link rhythm to X": X's rhythm plays here.
    double feel = 0.5;                ///< Rhythm strip Feel, 0..1.
    double strum = 0.5;               ///< Rhythm strip Strum, 0..1.
    bool stateBoundary = false;       ///< 8: reset mod envelopes and rhythm phase at this section.
    MelodyStyle style = MelodyStyle::none;   ///< 4.5, applied when rendered.

    juce::NamedValueSet extra;

    const TuneLayer* findLayer (LayerType type) const noexcept;

    bool operator== (const TuneSection& other) const;
    bool operator!= (const TuneSection& other) const { return ! (*this == other); }
};

//==============================================================================
/** One step of the setlist: "Verse x2" (tune-builder 1, 11). Sections are
    referenced by name, as the file does, and the model keeps names unique. */
struct TuneSetlistEntry
{
    juce::String section;
    int repeats = 1;

    juce::NamedValueSet extra;

    bool operator== (const TuneSetlistEntry& other) const;
    bool operator!= (const TuneSetlistEntry& other) const { return ! (*this == other); }
};

/** The sections and the order they play in. An empty setlist plays every
    section once, in order. */
struct TuneArrangement
{
    std::vector<TuneSection> sections;
    std::vector<TuneSetlistEntry> setlist;

    bool operator== (const TuneArrangement& other) const;
    bool operator!= (const TuneArrangement& other) const { return ! (*this == other); }
};

/** An A/B idea of the same tune (tune-builder 1, `variations`). */
struct TuneVariation
{
    juce::String name;
    TuneArrangement arrangement;

    juce::NamedValueSet extra;

    bool operator== (const TuneVariation& other) const;
    bool operator!= (const TuneVariation& other) const { return ! (*this == other); }
};

//==============================================================================
struct TuneMeta
{
    juce::String title { "Untitled Tune" };
    juce::String artist;
    juce::String author;              ///< "Factory" for shipped content (file-formats 12).

    double tempoBpm = 120.0;          ///< Quarter notes per minute.
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;

    int keyTonic = 0;                 ///< Pitch class.
    TuneMode mode = TuneMode::ionian;

    double swingPercent = 0.0;        ///< 0 straight .. 100 full triplet swing on offbeat eighths.
    double feelPercent = 0.0;         ///< Humanise amount for the rhythm engine, 0..100.

    juce::StringArray tags;
    juce::String notes;
    juce::String created;             ///< ISO 8601 UTC; set by the caller, never by save.
    juce::String modified;

    juce::NamedValueSet extra;

    bool operator== (const TuneMeta& other) const;
    bool operator!= (const TuneMeta& other) const { return ! (*this == other); }
};

//==============================================================================
/** One section occurrence in play order: the setlist flattened. */
struct TuneSpan
{
    int sectionIndex = 0;
    int setlistIndex = -1;            ///< -1 when there is no setlist.
    int repeat = 0;                   ///< Which pass through the setlist entry.
    double startBeat = 0.0;           ///< From the start of the tune.
    double lengthBeats = 0.0;
};

//==============================================================================
struct Tune
{
    static constexpr int kMaxSections = 64;
    static constexpr int kMaxBars = 256;
    static constexpr int kMaxRepeats = 64;
    static constexpr double kMinTempo = 20.0;
    static constexpr double kMaxTempo = 400.0;

    TuneMeta meta;
    TuneArrangement arrangement;
    std::vector<TuneVariation> variations;

    juce::NamedValueSet extra;

    bool operator== (const Tune& other) const;
    bool operator!= (const Tune& other) const { return ! (*this == other); }

    //==========================================================================
    // Queries

    /** Quarter-note beats per bar: 4 in 4/4, 3 in 3/4 and in 6/8. */
    double getBeatsPerBar() const noexcept;

    int getNumSections() const noexcept { return (int) arrangement.sections.size(); }
    bool isValidSection (int index) const noexcept;

    TuneSection* getSection (int index) noexcept;
    const TuneSection* getSection (int index) const noexcept;

    /** Index of the section with exactly this name, or -1. */
    int findSection (const juce::String& name) const noexcept;

    double getSectionLengthBeats (int index) const noexcept;

    /** The setlist flattened into play order. Entries naming a missing section
        are skipped (validate() reports them). */
    std::vector<TuneSpan> getPlayOrder() const;

    double getTotalBeats() const;

    /** The section whose rhythm plays in `index`: itself, or the one it is
        linked to (3.5). One hop only, so a cycle cannot hang anything. */
    int getRhythmSourceIndex (int index) const noexcept;

    /** "Verse" if free, else "Verse 2", "Verse 3", ... */
    juce::String makeUniqueSectionName (const juce::String& base) const;

    bool preferFlats() const noexcept { return tunetheory::keyPrefersFlats (meta.keyTonic, meta.mode); }

    /** Problems a user should hear about: setlist entries naming no section,
        duplicate section names, unknown chord qualities. Empty when clean. */
    juce::StringArray validate() const;

    //==========================================================================
    // Sections - action-and-undo 3.9 `tune-section-edit`

    /** Inserts at `insertAt` (-1 appends), renaming it to be unique. Returns
        its index, or -1 at the section limit. */
    int addSection (TuneSection section, int insertAt = -1);

    /** Also drops setlist entries that name it and rhythm links to it. */
    bool removeSection (int index);

    /** Reorders the section list. The setlist is by name, so play order is
        unchanged unless there is no setlist (then sections play in list order). */
    bool moveSection (int fromIndex, int toIndex);

    /** Inserts a copy after the original with a unique name. Returns its index. */
    int duplicateSection (int index);

    /** Fails on an empty or taken name. Setlist entries and rhythm links follow. */
    bool renameSection (int index, const juce::String& newName);

    bool setSectionLength (int index, int bars);
    bool setSectionRole (int index, SectionRole role);
    bool setSectionRhythm (int index, const juce::String& patternId, const juce::String& genreKitId);
    bool setSectionRhythmOn (int index, bool on);

    /** 3.5 "Link rhythm to X". An empty name unlinks. */
    bool linkSectionRhythm (int index, const juce::String& targetName);

    /** 3.3 "Repeat count". Sets the repeats of every setlist entry naming the
        section; with no setlist, first writes one that plays the sections as
        they are now, so the rest of the tune does not change. */
    bool setRepeatCount (int sectionIndex, int repeats);

    bool setSetlist (std::vector<TuneSetlistEntry> newSetlist);

    //==========================================================================
    // Chords - `tune-chord-edit`

    bool insertChord (int sectionIndex, int chordIndex, const ChordCell& cell);   ///< -1 appends
    bool removeChord (int sectionIndex, int chordIndex);
    bool moveChord (int sectionIndex, int fromIndex, int toIndex);
    bool setChord (int sectionIndex, int chordIndex, const ChordCell& cell);
    bool setChordDuration (int sectionIndex, int chordIndex, double beats);
    bool setChords (int sectionIndex, std::vector<ChordCell> cells);

    /** The pill menu's Lock (3.2). */
    bool setChordLocked (int sectionIndex, int chordIndex, bool locked);

    //==========================================================================
    // The piano roll's parts (3.4, 6, 7) - `tune-melody-edit` for the melody,
    // `tune-section-edit` for the bass and the layer

    /** The notes of a part, or null when the section has no such part: no
        melody track, a bass in a derived mode (whose notes are generated when
        rendered, TuneMelody's generateBassLine), no countermelody layer. */
    const std::vector<MelodyNote>* getPartNotes (int sectionIndex, TuneNotePart part) const noexcept;

    /** Replaces a part's notes, canonicalised, creating the part when needed:
        a melody track, a Manual bass, a countermelody layer. The roll's own
        edits (draw, delete, nudge, paste, velocity) all come through here so
        every part is edited the same way. Notes keep the `locked` flag they
        were given. */
    bool setPartNotes (int sectionIndex, TuneNotePart part, std::vector<MelodyNote> notes);

    //==========================================================================
    // Melody - `tune-melody-edit` unless noted

    /** Adds or removes the section's melody track. Removing is undoable by
        snapshot like every other edit. */
    bool setMelodyEnabled (int sectionIndex, bool enabled);

    /** Draw mode: adds a note (creating the track if needed) and locks it,
        because it is a manual edit. Returns its index, or -1. */
    int addMelodyNote (int sectionIndex, MelodyNote note);

    /** Replaces a note and locks it. */
    bool setMelodyNote (int sectionIndex, int noteIndex, MelodyNote note);

    bool removeMelodyNotes (int sectionIndex, std::vector<int> noteIndices);

    /** The right-click "unlock" (3.4), and its inverse. */
    bool setMelodyNoteLocked (int sectionIndex, int noteIndex, bool locked);

    /** Arrow-key nudge (3.4). Absolute pitches move by semitones; relative
        ones keep their meaning and only move in time. Nudged notes lock. */
    bool nudgeMelodyNotes (int sectionIndex, const std::vector<int>& noteIndices,
                           double deltaBeats, int deltaSemitones);

    /** A whole take: Record, Sing, or Improvise's Freeze. `tune-melody-record`. */
    bool setMelodyNotes (int sectionIndex, std::vector<MelodyNote> notes, MelodySource source);

    //==========================================================================
    // Bass and layers - `tune-section-edit`

    bool setBassMode (int sectionIndex, BassMode mode);
    bool setBassNotes (int sectionIndex, std::vector<MelodyNote> notes);

    /** Adds the layer, or replaces the section's layer of the same type. */
    bool setLayer (int sectionIndex, const TuneLayer& layer);
    bool removeLayer (int sectionIndex, LayerType type);

    //==========================================================================
    // Meta - `other`

    bool setTempo (double bpm);
    bool setTimeSignature (int numerator, int denominator);

    /** Only relabels the key; transposeTune and shiftMode (TuneHarmony) move
        the music. */
    bool setKey (int tonic, TuneMode mode);

    //==========================================================================
    // Variations (A/B ideas) - `tune-section-edit`

    /** Stores the current arrangement as a named variation. Returns its index. */
    int storeVariation (const juce::String& name);

    /** Swaps the current arrangement with a stored one, so A/B is one call and
        calling it again swaps back. */
    bool swapWithVariation (int index);

    bool removeVariation (int index);
};

} // namespace luthier
