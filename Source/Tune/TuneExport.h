#pragma once

/*  The one-screen export (tune-builder.md 9; 2.6 "the export dialog is one
    screen"; DECISIONS C-53): what the dialog asks for, and the writers behind
    each of its four destinations. None of the writers is new: audio is the
    plugin's own AudioExporter rendering a second instance that carries the
    tune in its state (TuneSession's render intent), MIDI is midi-export's
    MidiProfiles over buildTunePerformance, notation is NotationExporter over
    buildTuneScore, and the project is TuneFile.

    Everything here but startTuneAudioExport is synchronous and runs on the
    caller's thread (message or worker; it writes files). The audio render
    runs on the exporter's own thread, as it does for the header's Export.
*/

#include "TuneMidi.h"
#include "../Export/MidiProfiles.h"
#include "../Notation/NotationExport.h"
#include "../Support/AudioExporter.h"

namespace luthier
{

//==============================================================================
/** 9.1's stem choice. */
enum class TuneStemChoice { mainStereo = 0, everyBus, numChoices };

/** What the dialog's one screen holds. Every destination writes
    `<folder>/<baseName>[ - suffix].<ext>`. */
struct TuneExportRequest
{
    juce::File folder;                 ///< 9.1: ~/Documents/Luthier/Renders by default.
    juce::String baseName;             ///< The tune's title, made legal.

    // ---- 9.1 audio ------------------------------------------------------------------
    bool audio = true;
    AudioExporter::Format audioFormat = AudioExporter::Format::Wav;
    int bitDepth = 24;
    double sampleRate = 48000.0;
    double tailSeconds = 2.0;          ///< "Loop tail: 0 - 5 s of decay after the final beat."
    TuneStemChoice stems = TuneStemChoice::mainStereo;

    // ---- 9.2 MIDI -------------------------------------------------------------------
    bool midi = true;
    MidiProfile profile = MidiProfile::luthier;
    MidiTrackSplit split = MidiTrackSplit::perInstrument;
    bool realism = true;               ///< Bends, slides, vibratos as controllers, or plain notes.
    int ppq = MidiExportOptions::kDefaultPpq;

    // ---- 9.3 notation ---------------------------------------------------------------
    bool notation = false;
    NotationFormat notationFormat = NotationFormat::musicXml;
    bool chordSymbols = true;

    // ---- 9.4 project ----------------------------------------------------------------
    bool project = true;

    /** True when at least one destination is ticked. */
    bool exportsAnything() const noexcept { return audio || midi || notation || project; }

    /** `<folder>/<baseName> - <suffix>.<extension>`, or without the suffix when
        it is empty. The extension includes its dot. */
    juce::File fileFor (const juce::String& suffix, const juce::String& extension) const;
};

/** What was written, and what could not be. */
struct TuneExportReport
{
    juce::Array<juce::File> files;
    juce::StringArray errors;

    bool ok() const noexcept { return errors.isEmpty(); }
    juce::String describe() const;
};

//==============================================================================
namespace TuneExport
{
    /** 9.2: the tune through midi-export's profiles (C-53). The Luthier
        profile carries the section markers and the parts; Generic writes the
        plain channel stream. False, with `error`, when nothing was written. */
    bool writeMidi (const Tune& tune, const TuneMidiOptions& midiOptions, const TuneExportRequest& request,
                    const juce::File& destination, juce::String& error);

    /** 9.3: MusicXML, Guitar Pro or ASCII tab from the tune's PerformanceScore. */
    bool writeNotation (const Tune& tune, const TuneExportRequest& request,
                        const juce::File& destination, juce::String& error);

    /** 9.4: the `.luthiertune`. Atomic, no backup (it is a copy, not the file
        being edited). */
    bool writeProject (const Tune& tune, const juce::File& destination, juce::String& error);

    /** Writes every synchronous destination the request ticks (MIDI, notation,
        project) and reports each file or error. Audio is started separately. */
    TuneExportReport writeFiles (const Tune& tune, const TuneMidiOptions& midiOptions,
                                 const TuneExportRequest& request);

    //==========================================================================
    // 9.1 audio

    /** How long the tune plays once through, in seconds. */
    double getTuneLengthSeconds (const Tune& tune);

    /** The sequence the exporter is given. The offline instance plays the tune
        itself (TuneSession's render intent), so the sequence only fixes the
        render's length: one "all notes off" at the tune's end. */
    juce::MidiMessageSequence makeRenderSequence (const Tune& tune);

    /** The exporter's options for one file of the request: the tune's tempo,
        stereo, the request's format, depth, rate and tail. */
    AudioExporter::Options makeAudioOptions (const Tune& tune, const TuneExportRequest& request,
                                             const juce::File& destination);

    /** The stem files 9.1 names, in bus order: "Main", then every aux bus by
        its name (Aux 1 ... Aux 8). `mainStereo` gives just the first. */
    juce::StringArray getStemNames (TuneStemChoice choice);

    /** The output bus a stem renders: 0 is the main bus, stem n is aux n. */
    int getStemBusIndex (int stemIndex) noexcept;

    /** Wraps a fresh plugin instance so that one of its output buses becomes
        the stereo output the exporter records (9.1 "every routing bus as its
        own file"). Bus 0 returns the instance itself. */
    std::unique_ptr<juce::AudioProcessor> wrapForBus (std::unique_ptr<juce::AudioProcessor> instance, int busIndex);
}

//==============================================================================
/** A plugin instance whose output is one of its output buses (9.1 stems).
    Every aux bus is enabled on the instance, so the routing panel's sends
    reach them as they do live; the exporter sees a stereo processor. */
class TuneBusRenderProcessor : public juce::AudioProcessor
{
public:
    TuneBusRenderProcessor (std::unique_ptr<juce::AudioProcessor> innerProcessor, int busIndex);
    ~TuneBusRenderProcessor() override;

    juce::AudioProcessor& getInner() noexcept { return *inner; }
    int getBusIndex() const noexcept { return bus; }

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    void setStateInformation (const void* data, int sizeInBytes) override;
    void getStateInformation (juce::MemoryBlock& destData) override;

    const juce::String getName() const override;
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

private:
    std::unique_ptr<juce::AudioProcessor> inner;
    int bus;
    juce::AudioBuffer<float> wide;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneBusRenderProcessor)
};

} // namespace luthier
