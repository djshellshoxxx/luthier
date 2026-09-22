#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "LuthierEngine.h"
#include "Parameters.h"
#include "Presets/PresetManager.h"
#include "Support/MidiLearn.h"
#include "Support/MidiCapture.h"
#include "Support/AudioExporter.h"
#include "Support/Diagnostics.h"
#include "Routing/RoutingMatrix.h"
#include "Routing/MidiOutRouter.h"
#include "Modulation/ModMatrix.h"
#include "Rhythm/GenreKit.h"
#include "Live/Snapshots.h"
#include "Live/Setlist.h"
#include "Live/TapTempo.h"
#include "Live/LiveControls.h"
#include "Practice/Metronome.h"
#include "Practice/Looper.h"
#include "Practice/BackingTrack.h"
#include "Practice/Trainers.h"
#include "ToneMatch/ToneMatch.h"
#include "Updates/Telemetry.h"
#include "Accessibility/Accessibility.h"
#include "Accessibility/Localisation.h"

namespace luthier
{

//==============================================================================
class LuthierAudioProcessor : public juce::AudioProcessor,
                              private juce::AudioProcessorParameter::Listener,
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
    bool hasEditor() const override { return ! LUTHIER_HEADLESS; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }

    // Routing-io 6. Always true: the host has to be told the plugin has a MIDI
    // output port before the user can switch one on, and a port that emits
    // nothing is harmless. Hosts that do not support MIDI out from an instrument
    // ignore it, which is the silent no-op the spec asks for.
    bool producesMidi() const override { return true; }
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
    RoutingMatrix&      getRouting() noexcept        { return routing; }
    const RoutingMatrix& getRouting() const noexcept { return routing; }
    MidiOutRouter&      getMidiOutRouter() noexcept  { return midiOutRouter; }
    ModMatrix&          getModMatrix() noexcept      { return modMatrix; }
    const ModMatrix&    getModMatrix() const noexcept { return modMatrix; }

    /*  The rhythm engine's two libraries live here rather than in the engine.
        Both read the file system when they refresh, so they belong to the
        message thread; the engine only ever receives a pattern by value. */
    PatternLibrary&       getPatternLibrary() noexcept       { return patternLibrary; }
    const PatternLibrary& getPatternLibrary() const noexcept { return patternLibrary; }
    GenreKitLibrary&      getGenreKits() noexcept            { return genreKits; }
    const GenreKitLibrary& getGenreKits() const noexcept     { return genreKits; }

    /** Applies a genre kit to the rhythm engine and returns the rig preset it
        would like, or an empty string. The caller decides whether to load it. */
    juce::String applyGenreKit (int kitIndex);

    //==========================================================================
    // The live surface (live-performance.md).

    SnapshotBank&       getSnapshots() noexcept       { return snapshots; }
    const SnapshotBank& getSnapshots() const noexcept { return snapshots; }

    SetlistPlayer&      getSetlist() noexcept         { return setlist; }
    TapTempo&           getTapTempo() noexcept        { return tapTempo; }
    KillSwitch&         getKillSwitch() noexcept      { return killSwitch; }
    MonitorMix&         getMonitorMix() noexcept      { return monitorMix; }

    ExpressionCalibrationSet&       getExpression() noexcept       { return expression; }
    const ExpressionCalibrationSet& getExpression() const noexcept { return expression; }

    /** Captures the live state into a snapshot, including the state of the
        modules that do not live in the parameter tree. */
    bool captureSnapshot (int index, const juce::String& label = {}, int colourTag = -1);

    /** Recalls a snapshot. The crossfade is carried by the audio thread's own
        clock, so this returns before the fade has finished. */
    bool recallSnapshot (int index);

    void nextSnapshot();
    void previousSnapshot();

    /** live-performance 5: registers a tap, using the plugin's own clock. */
    void tapTempoNow();

    /** The tempo everything tempo-synced should use, host or tapped. */
    double getEffectiveTempo() const noexcept;

    /** live-performance 10: Live Mode. */
    void setLiveMode (bool shouldBeLive);
    bool isLiveMode() const noexcept { return uiState.liveMode; }

    /** Loads a setlist file and moves to its first entry. */
    bool loadSetlist (const juce::File& file);

    /** Applies the setlist's current entry - its preset and its snapshot - and
        pre-loads the one after it. */
    bool applyCurrentSetlistEntry();

    //==========================================================================
    // The practice tools (practice-tools.md).

    Metronome&           getMetronome() noexcept    { return metronome; }
    Looper&              getLooper() noexcept       { return looper; }
    BackingTrackPlayer&  getBackingTrack() noexcept { return backingTrack; }
    SessionRecorder&     getSessionRecorder() noexcept { return sessionRecorder; }
    ScaleTrainer&        getScaleTrainer() noexcept { return scaleTrainer; }
    EarTrainer&          getEarTrainer() noexcept   { return earTrainer; }
    ProgressionLooper&   getProgressionLooper() noexcept { return progression; }

    /** practice-tools 0.1: the panel being closed is what makes the tools cost
        nothing, so the processor is told rather than guessing. */
    void setPracticePanelOpen (bool isOpen) noexcept { practicePanelOpen = isOpen; }
    bool isPracticePanelOpen() const noexcept { return practicePanelOpen; }

    //==========================================================================
    // Tone match (tone-match.md).

    IrSlot& getBodyIrSlot() noexcept    { return bodyIr; }
    IrSlot& getCabIrSlot (int index) noexcept { return cabIr[(size_t) juce::jlimit (0, 1, index)]; }
    Capture& getCapture() noexcept     { return capture; }

    //==========================================================================
    // Updates and privacy (updates-telemetry.md).

    Telemetry& getTelemetry() noexcept { return telemetry; }
    License&   getLicense() noexcept   { return license; }

    /*  gui-integration 15's sample-rate trigger, as a question the window asks
        rather than a message the audio thread sends.

        Returns the new rate once, and zero every other time. "Once" is the whole
        point: prepareToPlay can be called repeatedly with the same rate - a
        block-size change alone does it - and a banner per prepareToPlay would
        appear every time a user touched their buffer size. It also returns zero
        for the first prepare of all, because opening a plugin at 48 kHz is not
        news; being moved from 48 to 96 is.

        Message thread. Claiming the change is what clears it, so two windows
        cannot both report it and a closed window does not lose it - the next one
        to open picks it up, which is right, since re-resampling happened whether
        anyone was watching or not. */
    double claimSampleRateChange() noexcept;

    /** Which of the advertised layouts the host actually negotiated. */
    BusLayout getNegotiatedLayout() const noexcept;

    /** True when the host gave us a sidechain input bus and enabled it. */
    bool hasSidechainInput() const noexcept;
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
        bool liveMode = false;
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
    /** The advertised bus layout. A static member because BusesProperties is
        protected in AudioProcessor, and because it is needed in the constructor's
        initialiser list, before any instance exists. */
    static BusesProperties buildBusesProperties();

    void timerCallback() override;
    void updateLatency();
    void updateRoutingLatencyReport();
    void processAuditionMidi (juce::MidiBuffer& midi, int numSamples);
    void logMidiForDiagnostics (const juce::MidiBuffer& midi) noexcept;

    /** live-performance 2: program change recalls a snapshot, bank select picks
        a preset. Consumes the messages it acts on so nothing downstream sees
        them twice. */
    void handleLiveMidi (juce::MidiBuffer& midi) noexcept;

    /** Writes the snapshot's non-parameter blobs into the modules that own
        them. Called from the bank at the crossfade midpoint. */
    void applySnapshotModules (const Snapshot& snapshot);

    /** Feeds the modulation matrix the note and controller events it uses as
        sources. Called before the engine consumes the buffer. */
    void feedModulationSources (const juce::MidiBuffer& midi) noexcept;

    /** Measures the block the matrix's envelope followers watch. */
    void buildModBlockContext (const juce::AudioBuffer<float>& output,
                               int numSamples, ModBlockContext& context) noexcept;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    LuthierEngine engine;
    ParameterBridge bridge;
    PresetManager presets;
    MidiLearnManager midiLearn;
    MidiCapture midiCapture;
    AudioExporter exporter;
    Diagnostics diagnostics;
    RoutingMatrix routing;
    MidiOutRouter midiOutRouter;
    ModMatrix modMatrix;
    PatternLibrary patternLibrary;
    GenreKitLibrary genreKits;

    SnapshotBank snapshots;
    SetlistPlayer setlist;
    TapTempo tapTempo;
    KillSwitch killSwitch;
    MonitorMix monitorMix;
    ExpressionCalibrationSet expression;

    /*  The monitor mix is rendered into its own buffer and then written to the
        monitor aux bus, because it must not be summed into the main output: the
        whole point of it is that the audience does not hear it. */
    juce::AudioBuffer<float> monitorBuffer;

    // --- practice tools ---------------------------------------------------------------
    Metronome metronome;
    Looper looper;
    BackingTrackPlayer backingTrack;
    SessionRecorder sessionRecorder;
    ScaleTrainer scaleTrainer;
    EarTrainer earTrainer;
    ProgressionLooper progression;

    bool practicePanelOpen = false;

    /** The metronome's click and the backing track are rendered into their own
        buffers and then mixed, so neither can be written over by the other. */
    juce::AudioBuffer<float> clickBuffer, backingBuffer;

    // --- tone match ---------------------------------------------------------------------
    IrSlot bodyIr;
    std::array<IrSlot, 2> cabIr;
    Capture capture;

    // --- updates and privacy ---------------------------------------------------------------
    Telemetry telemetry;
    License license;

    /*  live-performance 2: a program change or bank select arrives on the audio
        thread, but acting on either can allocate - a snapshot recall walks the
        parameter tree, and a preset load reads a file. The audio thread records
        the request and the timer carries it out, which keeps the callback free
        of both. -1 means nothing pending. */
    std::atomic<int> pendingSnapshotRecall { -1 };
    std::atomic<int> pendingPresetSelect { -1 };

    UiState uiState;

    /*  The sidechain has to be copied out before the engine renders.

        A host hands processBlock a single buffer whose channels are shared
        between the input and output buses: with a stereo sidechain and a stereo
        main output, the sidechain arrives in exactly the two channels the engine
        is about to write its output into. Reading it in place would mean reading
        the guitar back, and only the first block would look right.
    */
    juce::AudioBuffer<float> sidechainCopy;

    // Transport state, so the matrix can retrigger synced sources exactly once
    // when the host starts rolling.
    bool transportWasRunning = false;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    int reportedLatency = 0;

    /*  gui-integration 15: "Sample rate changed to 96 kHz, IRs and circuit filters
        re-resampled." The rate the window has already accounted for.

        prepareToPlay is called from the host's thread with no editor guaranteed
        to exist - a rate change while the window is closed is the ordinary case,
        not the exotic one - so rather than have that thread push a message at a
        window that may not be there, it records the rate and the window compares
        when it next looks.

        `preparedSampleRate` is the processor's own record rather than
        AudioProcessor::getSampleRate(), which is set by setRateAndBufferSizeDetails
        and therefore only by a host: calling prepareToPlay directly leaves it
        stale, which is how a test caught this depending on something that is not
        prepareToPlay's job to set.

        `sampleRateKnownToUi` is zero until a window has accounted for a rate,
        which is how the first prepare of all is told apart from a change: opening
        a plugin at 48 kHz is not news, being moved from 48 to 96 is. Both atomic
        because the writer is the host's thread and the reader is the message
        thread; prepareToPlay may allocate, so a plain store costs nothing there. */
    std::atomic<double> preparedSampleRate { 0.0 };
    std::atomic<double> sampleRateKnownToUi { 0.0 };

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

    /*  action-and-undo.md 3.1: a parameter edit becomes one undo entry per
        gesture, not one per intermediate value.

        JUCE's gesture pair is what separates a user's hand from everything else
        that writes to a parameter. A slider being dragged opens a gesture; host
        automation, a learned MIDI CC and the modulation matrix all write values
        without one. Section 3.1 says none of those three may generate an undo
        entry, and listening to gestures rather than to value changes gets that
        for free rather than by trying to guess the source afterwards. */
    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override;

    struct UndoEntry
    {
        juce::MemoryBlock state;
        juce::String description;
    };

    juce::Array<UndoEntry> undoStack;
    int undoPosition = -1;

    /** action-and-undo.md 2. */
    static constexpr int kMaxUndoSteps = 200;

    /*  The state as it was when the current gesture started, held until the
        gesture ends and we know whether anything actually changed. */
    /*  Ableton calls setCurrentProgram(0) immediately after setStateInformation,
        to "restore" the plugin to its first program.

        Luthier's setCurrentProgram actually loads a preset, so obeying that call
        would overwrite the state the host had just restored - the user reopens a
        project and finds factory preset 0 instead of the sound they saved. The
        troubleshooting note that documents this
        (troubleshooting/parameter-issues/ableton-preset-interference-state-restoration-JUCE-20251107.md)
        suggests returning 0 from getNumPrograms, but host-integration.md 12
        requires the program interface so Program Change can address presets. So
        the call is swallowed once instead: the state a host restores wins over
        the program change that follows it, and every later Program Change works
        normally. */
    std::atomic<bool> ignoreNextProgramChange { false };

    juce::MemoryBlock gestureStartState;
    juce::String gestureParameterName;
    float gestureStartValue = 0.0f;
    int gestureParameterIndex = -1;

    juce::StringArray lockedParameters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuthierAudioProcessor)
};

} // namespace luthier
