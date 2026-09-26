#pragma once

/*  The tune builder's export (tune-builder.md 9, DECISIONS C-53).
    TUNE-HELP-ONBOARDING workstream.

    "One dialog, four destinations" - this is what the dialog (TuneExportDialog)
    calls, one function per destination, each usable without a UI:

      AUDIO (9.1)  The tune rendered offline through a fresh instance of the
          plugin loaded with the live state - the same arrangement as the
          preset's audio export (AudioExporter) - so what is written is what is
          heard. The main stereo out, and with stems every aux bus 1-8 as its
          own file. WAV, AIFF or FLAC at 16 / 24 / 32-float; the loop tail
          (0-5 s) rings after the last beat. MP3 is not offered: the build has
          no MP3 encoder (JUCE ships none, and the LAME licence is not one this
          project takes on) - DECISIONS / coverage note.

      MIDI (9.2, C-53)  Through midi-export.md's profiles: the tune's MIDI
          (buildTuneMidiFile, with or without realism) becomes a MidiPerformance
          with its section markers as section events, which MidiProfiles writes
          in the Luthier or Generic profile with any of its four track splits.

      NOTATION (9.3)  buildTuneScore -> NotationExporter: MusicXML, Guitar Pro
          or ASCII tab, section headings and chord symbols kept.

      PROJECT (9.4)  The `.luthiertune`, optionally with the preset and guitar
          bundled into it (as a `bundle` object the file format keeps as an
          unknown field in older builds), so it is self-contained.

    Everything here runs off the audio thread; audio rendering is slow and
    belongs on a worker (the dialog runs it on one).
*/

#include <juce_audio_processors/juce_audio_processors.h>

#include "AudioExporter.h"
#include "../Export/MidiProfiles.h"
#include "../Notation/NotationExport.h"
#include "../Tune/TuneModel.h"

#include <functional>

namespace luthier
{

namespace TuneExport
{
    //==========================================================================
    // 9.1 Audio

    struct AudioOptions
    {
        juce::File folder;                        ///< ~/Documents/Luthier/Renders by default
        juce::String baseName { "Tune" };
        AudioExporter::Format format = AudioExporter::Format::Wav;
        int bitDepth = 24;                        ///< 16, 24 or 32 (float)
        double sampleRate = 48000.0;              ///< "current host or user-selected"
        bool stems = false;                       ///< every aux bus as its own file
        double tailSeconds = 2.0;                 ///< 0 - 5 s after the final beat
        bool includeJamBand = true;               ///< FEAT-JAM (jam-mode 9): the band in the render, when it is enabled
    };

    static constexpr int kNumAuxStems = 8;
    static constexpr double kMaxTailSeconds = 5.0;

    struct Render
    {
        juce::AudioBuffer<float> main;                 ///< stereo
        std::vector<juce::AudioBuffer<float>> aux;     ///< stereo each; empty without stems
        double seconds = 0.0;
    };

    /** How long the tune plays, in seconds, without the tail. */
    double getTuneSeconds (const Tune& tune);

    /** Renders the tune in `pluginState` (the plugin's whole state, the tune
        with it) through a fresh offline instance. `progress` is told 0..1 and
        may return false to cancel. */
    bool renderAudio (const juce::MemoryBlock& pluginState, double sampleRate, int blockSize,
                      double tailSeconds, bool stems, Render& result,
                      const std::function<bool (double)>& progress = {},
                      bool includeJamBand = true);   // FEAT-JAM: with stems, Aux 9 and 10 as well

    /** Renders and writes the files: "<base>.wav", and with stems
        "<base> - Aux 1.wav" ... Returns the files written; `error` says why not. */
    juce::Array<juce::File> exportAudio (const juce::MemoryBlock& pluginState, const AudioOptions& options,
                                         juce::String& error, const std::function<bool (double)>& progress = {});

    //==========================================================================
    // 9.2 MIDI

    struct MidiOptions
    {
        MidiExportOptions profile;       ///< Luthier or Generic, split, PPQ (midi-export)
        bool includeRealism = true;      ///< "with realism events ... or as plain note-on / note-off"
        bool includeJamBand = true;      ///< FEAT-JAM (jam-mode 9): appendJamTracks after the tune's own
    };

    /** The tune as a performance: its instrument parts (0 guitar, 1 bass) and
        its sections as section events. */
    MidiPerformance buildPerformance (const Tune& tune, double sampleRate, bool includeRealism);

    bool exportMidi (const Tune& tune, const juce::File& destination, const MidiOptions& options, juce::String& error);

    /** FEAT-JAM (jam-mode 9): plays the tune once through a fresh offline
        instance with the Jam band as `pluginState` has it, and appends the
        band's "Jam Drums" and "Jam Bass" tracks to the MIDI file already at
        `midiFile`, on its PPQ. False, with a reason, when the band is not
        enabled in the state or played nothing. */
    bool appendJamTracks (const juce::MemoryBlock& pluginState, const juce::File& midiFile, juce::String& error,
                          double sampleRate = 48000.0);

    //==========================================================================
    // 9.3 Notation

    bool exportNotation (const Tune& tune, NotationFormat format, const juce::File& destination,
                         const NotationExportOptions& options, juce::String& error);

    //==========================================================================
    // 9.4 Project

    /** Writes the tune; with `bundle`, the preset and guitar go inside it. */
    bool exportProject (const Tune& tune, const juce::File& destination, bool bundle,
                        const juce::var& presetState, const juce::var& guitarState, juce::String& error);

    /** What a bundled project carries, or void when it has none. */
    juce::var getBundledPreset (const Tune& tune);
    juce::var getBundledGuitar (const Tune& tune);
}

} // namespace luthier
