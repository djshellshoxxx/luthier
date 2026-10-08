#pragma once

/*  A MIDI file as a Tune (midi-export.md 5, tune-builder.md 9.2 and 15's
    "Export MIDI re-imported").

    The inverse of buildTuneMidiFile, as far as a standard MIDI file allows:
    tempo, time and key signature and the title come from the meta track, the
    section markers become sections, and the instrument tracks are sorted into
    the Tune Builder's parts. A file Luthier wrote is recognised by its track
    names (or its section-named tracks) and read back by channel, so what was
    exported comes back as the tune it was: chord cells rather than held notes,
    a manual bass line, a countermelody layer, and the pad / arpeggio /
    percussion layers as layers again rather than as their generated notes.

    Any other file is read by what its tracks do rather than what they are
    called: a track that mostly plays three or more notes at once is the chord
    track, a low or "bass"-named one is the bass, the first monophonic one is
    the melody, and channel 10 is drums and is skipped. Chords are recovered
    beat by beat with the rhythm engine's own ChordDetector, so a chord the
    importer writes is always one the engine can play back (TuneTheory's rule),
    and the Tune Builder then strums it with the section's pattern: an import
    is an arrangement of the file, not a playback of it. The original chord
    track is kept as a muted countermelody layer so nothing is lost, and every
    guess or default is listed in `warnings` for the panel to show.

    error-recovery 1: a refused file leaves `out` untouched.
*/

#include "TuneMelody.h"
#include "TuneMidi.h"

namespace luthier
{

//==============================================================================
struct TuneImportOptions
{
    /** The channel layout a Luthier-written file uses (TuneMidiOptions'
        defaults: chords 1, melody 2, bass 3, layers 4-7). */
    TuneMidiOptions midi;

    /** Melody and bass notes are exact by default (they were written by a
        sequencer, not played); quantise as Record does when asked. */
    bool quantise = false;
    QuantiseGrid grid = QuantiseGrid::sixteenth;

    /** Without markers, a file longer than `singleSectionMaxBars` is cut into
        sections of `chunkBars` so the section strip stays usable. */
    int singleSectionMaxBars = 16;
    int chunkBars = 8;

    /** What the imported sections play their chords with. */
    juce::String rhythmPatternId { "Folk Down Up" };
    juce::String genreKitId { "Folk Fingerstyle" };
};

//==============================================================================
/** Reads a `.mid` from disk. False, with `error`, when the file is missing,
    unreadable, not a MIDI file, in SMPTE time or without a note; `out` is
    then untouched. `warnings` (optional) lists what was defaulted or guessed. */
bool importMidiFile (const juce::File& file, Tune& out, const TuneImportOptions& options,
                     juce::String& error, juce::StringArray* warnings = nullptr);

/** The same from memory, for tests and drag-drop payloads. `sourceName` is the
    file name used in messages and as the title when the file has none. */
bool importMidiData (const void* data, size_t numBytes, const juce::String& sourceName, Tune& out,
                     const TuneImportOptions& options, juce::String& error,
                     juce::StringArray* warnings = nullptr);

/** The core: an already parsed MidiFile. */
bool importMidi (const juce::MidiFile& midi, const juce::String& sourceName, Tune& out,
                 const TuneImportOptions& options, juce::String& error,
                 juce::StringArray* warnings = nullptr);

} // namespace luthier
