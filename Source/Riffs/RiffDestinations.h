#pragma once

/*  Where a riff goes from the browser (riff-library.md 6, 7.4).

      - Drag to DAW: compiled at the key and tempo shown, turned into a
        MidiPerformance through MidiPerformance::fromScore plus the STRUM and
        BASS_TECH events, written with the plugin's own MidiProfiles writer
        to ~/Documents/Luthier/Riffs/Drag/<Name> - <Key> <bpm>.mid. Files
        there older than 30 days are pruned on startup.
      - Add to Tune: into the selected section's MelodyTrack (guitar) or
        BassTrack (bass, mode manual), transposed to the tune's key, clipped
        to the section, locked notes kept, as one undo entry.
      - Send to Looper: one layer, the riff's whole bars at the current tempo,
        played by a fresh engine through its own RiffPlayer - the audition
        path, so the layer sounds like the audition.
      - Save as riff: the capture's marked region (or the last N bars),
        quantised to 1/16, analysed (RiffAnalysis).

    Message thread.
*/

#include "RiffCompiler.h"
#include "../Export/MidiPerformance.h"
#include "../Export/MidiProfiles.h"
#include "../Model/Guitar/GuitarLibrary.h"

namespace luthier
{

class LuthierAudioProcessor;
class TuneSession;

namespace RiffDestinations
{
    /** The loaded instrument, as placement sees it. */
    GuitarSpecSummary guitarSummary (LuthierAudioProcessor& processor);

    /** A placed riff (CompiledRiff::toPlacedRiff) as a performance at `tempoBpm`. */
    MidiPerformance toPerformance (const Riff& placed, double tempoBpm, double sampleRate);

    /** "<Name> - <Key> <bpm>.mid", made safe for a file system. */
    juce::String dragFileName (const Riff& riff, const CompiledRiff& compiled, double tempoBpm);

    /** Writes the drag-out file. `tempoBpm` <= 0 uses the compiled riff's
        tempo. The same inputs give identical bytes. Returns File() on failure,
        with the reason in `error`. */
    juce::File writeDragFile (const Riff& riff, const CompiledRiff& compiled, MidiProfile profile,
                              const juce::File& folder, int ppq = 960, double tempoBpm = 0.0,
                              juce::String* error = nullptr);

    /** Deletes files in `folder` last modified more than `days` ago. Returns how many. */
    int pruneDragFolder (const juce::File& folder, int days = 30);

    //==========================================================================
    struct TuneInsert
    {
        bool ok = false;
        int inserted = 0;
        int droppedForLocks = 0;
        int clipped = 0;
        juce::String message;
    };

    /** Add to Tune (6.2). `startBeat` < 0 is the section's start. One undo entry. */
    TuneInsert addToTune (TuneSession& session, const Riff& riff, int sectionIndex, double startBeat = -1.0);

    //==========================================================================
    struct LooperSend
    {
        bool ok = false;
        int lengthSamples = 0;
        juce::String message;
    };

    /** Send to Looper (6.3): the riff's whole bars at `tempoBpm`. */
    LooperSend sendToLooper (LuthierAudioProcessor& processor, const Riff& riff,
                             const RiffPlaySettings& settings, double tempoBpm);

    /** The riff rendered by a fresh engine playing it through its RiffPlayer,
        exactly `lengthSamples` long. */
    juce::AudioBuffer<float> render (const Riff& riff, const RiffPlaySettings& settings, GuitarType guitar,
                                     double tempoBpm, double sampleRate, int lengthSamples);

    //==========================================================================
    /** Save as riff (7.4): the marked region of the performance capture, or
        else its last `lastBars` bars at `tempoBpm`, quantised to 1/16. Fills
        notes, tuning, instrument, tempo, metre, key, techniques and
        difficulty; name, type, genre and tags are the caller's. */
    bool riffFromCapture (LuthierAudioProcessor& processor, int lastBars, double tempoBpm,
                          Riff& out, juce::String* error = nullptr);

    /** "Playing Delta Turnaround 3 in E, 96 bpm, looping" (accessibility 10). */
    juce::String auditionAnnouncement (const juce::String& name, int rootPitchClass, double bpm, bool looping);
}

} // namespace luthier
