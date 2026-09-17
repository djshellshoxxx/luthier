#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "LuthierEngine.h"
#include "Parameters.h"
#include "Presets/PresetManager.h"
#include "Support/MidiLearn.h"
#include "Support/MidiCapture.h"
#include "Support/AudioExporter.h"
#include "Support/Diagnostics.h"

namespace luthier
{

//==============================================================================
class LuthierAudioProcessor : public juce::AudioProcessor,
                              private juce::Timer
{
public:
    LuthierAudioProcessor();
    ~LuthierAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    //==========================================================================
    // Program (preset) interface, so hosts can recall presets by program change.

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    LuthierEngine&      getEngine() noexcept        { return engine; }
    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }
    PresetManager&      getPresetManager() noexcept { return presets; }
    MidiLearnManager&   getMidiLearn() noexcept     { return midiLearn; }
    MidiCapture&        getMidiCapture() noexcept   { return midiCapture; }
    AudioExporter&      getExporter() noexcept      { return exporter; }
    Diagnostics&        getDiagnostics() noexcept   { return diagnostics; }
    ParameterBridge&    getParameterBridge() noexcept { return bridge; }

    //==========================================================================
    /** Plays a built-in phrase through the live engine. The AUDITION button. */
    void startAudition (AuditionPhrase::Type type);
    void stopAudition();
    bool isAuditioning() const noexcept { return auditionActive.load(); }
    AuditionPhrase::Type getAuditionType() const noexcept { return auditionType; }

    /** Triggers a single note from the fretboard component. */
    void triggerPreviewNote (int stringIndex, double fretPosition, double velocity);
    void releasePreviewNote (int stringIndex);

    //==========================================================================
    /** Releases every string and clears all state. The Panic button. */
    void panic();

    /** Restores every parameter, the MIDI map and the UI state to defaults. */
    void resetEverything();

    /** The destructive reset in the debug panel: defaults plus removing caches. */
    void hardResetAndClearCaches();

    /** Randomises with the current locks. */
    void randomiseParameters();

    void setParameterLocked (const juce::String& paramId, bool locked);
    bool isParameterLocked (const juce::String& paramId) const;
    juce::StringArray getLockedParameters() const { return lockedParameters; }

    //==========================================================================
    // A/B compare and undo, both owned by the processor so they survive the editor.

    void storeToSlot (bool slotB);
    void recallSlot (bool slotB);
    void copyAtoB();
    bool isSlotBActive() const noexcept { return slotBActive; }
    void setSlotBActive (bool b);

    void pushUndoState (const juce::String& description);
    bool canUndo() const noexcept { return undoPosition > 0; }
    bool canRedo() const noexcept { return undoPosition + 1 < undoStack.size(); }
    void undo();
    void redo();
    juce::String getUndoDescription() const;
    juce::String getRedoDescription() const;

    //==========================================================================
    // UI state that belongs with the plugin rather than with the editor.

    struct UiState
    {
        bool advancedMode = false;
        bool tooltipsEnabled = true;
        int  selectedString = 0;
        int  advancedTab = 0;
        bool easterEggFound = false;
        int  editorWidth = 1200;
        int  editorHeight = 720;
        AuditionPhrase::Type auditionType = AuditionPhrase::Type::MajorScale;
    };

    UiState& getUiState() noexcept { return uiState; }

    /** Host tempo, updated each block. */
    double getHostTempo() const noexcept { return hostTempo.load(); }

    /** Snapshot of the plugin state, for the exporter. */
    juce::MemoryBlock captureStateBlock();

    /** Factory used by the exporter to make an offline instance. */
    static std::unique_ptr<juce::AudioProcessor> createOfflineInstance();

private:
    void timerCallback() override;
    void updateLatency();
    void processAuditionMidi (juce::MidiBuffer& midi, int numSamples);
    void logMidiForDiagnostics (const juce::MidiBuffer& midi) noexcept;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    LuthierEngine engine;
    ParameterBridge bridge;
    PresetManager presets;
    MidiLearnManager midiLearn;
    MidiCapture midiCapture;
    AudioExporter exporter;
    Diagnostics diagnostics;

    UiState uiState;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    int reportedLatency = 0;

    std::atomic<double> hostTempo { 120.0 };
    int64_t samplePosition = 0;

    // --- audition -------------------------------------------------------------
    std::atomic<bool> auditionActive { false };
    AuditionPhrase::Type auditionType = AuditionPhrase::Type::MajorScale;
    juce::MidiMessageSequence auditionSequence;
    int auditionEventIndex = 0;
    double auditionPositionSeconds = 0.0;
    double auditionEndSeconds = 0.0;

    // --- preview notes from the fretboard ---------------------------------------
    juce::MidiBuffer previewMidi;
    juce::CriticalSection previewLock;

    // --- A/B and undo -------------------------------------------------------------
    juce::MemoryBlock slotA, slotB;
    bool slotBActive = false;

    struct UndoEntry
    {
        juce::MemoryBlock state;
        juce::String description;
    };

    juce::Array<UndoEntry> undoStack;
    int undoPosition = -1;
    static constexpr int kMaxUndoSteps = 64;

    juce::StringArray lockedParameters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuthierAudioProcessor)
};

} // namespace luthier
