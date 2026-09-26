#pragma once

/*  midi-export.md 5, the import UI's model (MODEL-GAPS, TODO 10).

        File -> Import -> MIDI, or drag a .mid file onto the plugin window.
        - Auto-detects Luthier profile by header chunk.
        - Generic profile imports as a PerformanceScore with default realism
          values.
        - Import target: current session (adds to session recorder), tune
          builder (loads as a new tune), or looper (loads as a layer).

    MidiProfiles reads the file (either profile); this puts it where the user
    chose. The session gets the notes after what it holds; the tune builder
    gets a new tune whose first section's melody is the notes, recorded and
    quantised the way the TUNE tab's Record does; the looper gets the notes
    rendered by an engine playing the loaded guitar, as a layer. Message thread.
*/

#include "MidiProfiles.h"
#include "../Model/Guitar/GuitarLibrary.h"
#include "../Model/Playing/AutoArticulator.h"   // FEAT-ASSIST

namespace luthier
{

class LuthierAudioProcessor;

enum class MidiImportTarget
{
    session = 0,
    tune,
    looper,
    numTargets
};

const char* getMidiImportTargetName (MidiImportTarget) noexcept;   ///< "Current session", ...

struct MidiImportOutcome
{
    bool ok = false;
    MidiImportResult read;          ///< what MidiProfiles found in the file
    int notes = 0;                  ///< note-ons put where they were asked to go
    juce::String message;           ///< one line for the notification banner
};

namespace MidiImportTargets
{
    /** True for the files a drop or the chooser takes (.mid, .midi). */
    bool isMidiFile (const juce::File& file);

    MidiImportOutcome importFile (LuthierAudioProcessor& processor, const juce::File& file, MidiImportTarget target);
    MidiImportOutcome importPerformance (LuthierAudioProcessor& processor, const MidiPerformance& performance,
                                         MidiImportTarget target, const juce::String& name);

    /** The looper's layer: the performance played by a fresh engine on `guitar`, stereo, at `sampleRate`. */
    juce::AudioBuffer<float> render (const MidiPerformance& performance, GuitarType guitar, double sampleRate,
                                     double maxSeconds);

    /** auto-articulation.md 5, 9 (FEAT-ASSIST): the same, played with Performance
        Assist set as `assist`. A Luthier-profile file's NOTE events already carry
        their techniques, so its notes are pre-articulated and Assist leaves them
        alone - a round trip does not articulate twice. */
    juce::AudioBuffer<float> render (const MidiPerformance& performance, GuitarType guitar, double sampleRate,
                                     double maxSeconds, const AutoArticulationSettings& assist);
}

} // namespace luthier
