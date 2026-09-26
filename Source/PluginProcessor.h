#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "LuthierEngine.h"
#include "Parameters.h"
#include "Presets/PresetManager.h"
#include "Support/MidiLearn.h"
#include "Support/UndoHistory.h"
#include "Support/MidiCapture.h"
#include "Capture/PerformanceCapture.h"
#include "Presets/PresetMorph.h"
#include "Tune/TuneSession.h"
#include "Tune/TuneHumCapture.h"
#include "Support/AudioExporter.h"
#include "Support/Diagnostics.h"
#include "Routing/RoutingMatrix.h"
#include "Routing/MidiOutRouter.h"
#include "Export/LiveMidiOut.h"
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
#include "Practice/PracticeRoutineProgress.h"
#include "Practice/PracticeRoutineSetup.h"
#include "ToneMatch/ToneMatch.h"
#include "Updates/Telemetry.h"
#include "PhysicalRange.h"
#include "Model/Workshop/PartAcoustics.h"
#include "Workshop/WorkshopBench.h"
#include "Accessibility/Accessibility.h"
#include "Accessibility/Localisation.h"
#include "Support/InstallLayout.h"
#include "Support/SoundingNotesPublisher.h"

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

    /** notation-export.md 6: what the engine played, voiced, for the NOTATION
        tab, the live TAB view and notation / MIDI export. */
    PerformanceCapture& getPerformanceCapture() noexcept { return performanceCapture; }

    /*  notation-export 3 (MODEL-GAPS, TODO 9): "render the current bar to the
        on-plugin fretboard as tablature dots". The NOTATION tab's switch; the
        fretboard reads it. Message thread. */
    void setTabDotsOnFretboard (bool shouldShow) noexcept { tabDotsOnFretboard = shouldShow; }
    bool isShowingTabDotsOnFretboard() const noexcept { return tabDotsOnFretboard; }

    /** Drains the capture and keeps its tuning current (10 Hz on the message
        thread; tests call it directly). */
    void drainPerformanceCapture();

    /** ambiguity-resolutions.md 5: morphing between two presets. */
    PresetMorph& getPresetMorph() noexcept { return presetMorph; }

    /** Follows `preset_morph_position` (the timer's work; tests call it). */
    void updatePresetMorph();

    /** tune-builder.md: the TUNE tab's player (audio thread) and the tune it
        plays (message thread). */
    TunePlayer&  getTunePlayer() noexcept  { return tunePlayer; }
    TuneSession& getTuneSession() noexcept { return tuneSession; }

    /** tune-builder 13: sung / hummed melody capture from the audio input (TUNE-HELP-ONBOARDING). */
    TuneHumCapture& getHumCapture() noexcept { return humCapture; }

    /** tune-builder 14: a tune parameter's value with automation and modulation,
        which is how routes move a section over the tune's timeline. Any thread. */
    float tuneModValue (const char* parameterId) const noexcept;

    /** tune-builder 14: "snapshots capture the current section state, so a live
        rig can switch sections with a footswitch". What a snapshot keeps of the
        tune (the section playing or selected, by name), and the recall's
        request, which the message thread carries out (PluginProcessorTune.cpp). */
    juce::var captureTuneSnapshotState() const;
    void requestTuneSnapshotState (const juce::var& state) noexcept;
    void applyPendingTuneSection();

    /** How many tune state boundaries (8) the audio thread has acted on. */
    int getNumTuneStateBoundaries() const noexcept { return tuneStateBoundaries.load (std::memory_order_relaxed); }

    /** The tune's message-thread work: rhythm changes at section starts,
        improvised passes, old timelines, the take (the timer's; tests call it). */
    void serviceTune();

    /** practice-tools 0.2: the click (practice metronome and the tune's) goes
        to the main out instead of the monitor bus. Without a monitor bus it
        goes there regardless. Saved with the session. */
    void setClickToMain (bool toMain) noexcept { clickToMain.store (toMain, std::memory_order_relaxed); }
    bool isClickToMain() const noexcept        { return clickToMain.load (std::memory_order_relaxed); }

    AudioExporter&      getExporter() noexcept      { return exporter; }
    Diagnostics&        getDiagnostics() noexcept   { return diagnostics; }
    RoutingMatrix&      getRouting() noexcept        { return routing; }
    const RoutingMatrix& getRouting() const noexcept { return routing; }
    MidiOutRouter&      getMidiOutRouter() noexcept  { return midiOutRouter; }

    /** midi-export.md 6: a Workshop part change, sent as Luthier SysEx at the
        next block when the routing's WORKSHOP source is on. Message thread. */
    void postWorkshopChange (const juce::String& slotId, const juce::String& fitted, const juce::String& was);

    const LuthierSysExOut& getLuthierSysExOut() const noexcept { return sysExOut; }
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

    /*  action-and-undo.md 3.7: the UI's save and recall, each one entry
        ("Save snapshot [i] [name]", "Recall snapshot [i] [name]"). MIDI and
        setlist recalls use recallSnapshot and push nothing. */
    bool captureSnapshotAsUserAction (int index, const juce::String& label = {});
    bool recallSnapshotAsUserAction (int index);
    void renameSnapshotAsUserAction (int index, const juce::String& label);
    void setSnapshotColourAsUserAction (int index, int colourTag);
    /** `removeSlot` erases the slot (the bank shifts); false empties it in place. */
    void deleteSnapshotAsUserAction (int index, bool removeSlot);

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
    const juce::File& getSetlistFile() const noexcept { return setlistFile; }

    /** Applies the setlist's current entry - its preset and its snapshot - and
        pre-loads the one after it. */
    bool applyCurrentSetlistEntry (bool asUndoStep = true);   // action-and-undo.md 3.10

    //==========================================================================
    // The practice tools (practice-tools.md).

    Metronome&           getMetronome() noexcept    { return metronome; }
    Looper&              getLooper() noexcept       { return looper; }
    BackingTrackPlayer&  getBackingTrack() noexcept { return backingTrack; }
    SessionRecorder&     getSessionRecorder() noexcept { return sessionRecorder; }
    ScaleTrainer&        getScaleTrainer() noexcept { return scaleTrainer; }
    EarTrainer&          getEarTrainer() noexcept   { return earTrainer; }
    ProgressionLooper&   getProgressionLooper() noexcept { return progression; }

    /** practice-tools 10-12: routines, the history and what counts minutes. */
    PracticeRoutineRunner&   getPracticeRoutineRunner() noexcept   { return practiceRunner; }
    PracticeStats&           getPracticeStats() noexcept           { return practiceStats; }
    PracticeActivityTracker& getPracticeActivityTracker() noexcept { return practiceTracker; }

    /** Where the history is saved (tests point it at a temporary file). */
    void setPracticeStatsFile (const juce::File& file) { practiceStatsFile = file; }
    bool savePracticeStats() const;

    /** Every practice tool, for the runner, the tracker and the defaults. */
    PracticeTargets getPracticeTargets() noexcept;

    /** A routine asks for the drawer to open on a tool; the editor's timer
        takes the request (there may be no editor when it is made). */
    void requestPracticeDrawer (PracticeTool tool) noexcept { pendingDrawerTool.store ((int) tool); }
    int takePracticeDrawerRequest() noexcept                { return pendingDrawerTool.exchange (-1); }

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

    //==========================================================================
    /** advanced-ranges.md: the preset's stock/advanced state. */
    RangeState&       getRanges() noexcept       { return ranges; }
    const RangeState& getRanges() const noexcept { return ranges; }

    /*  Applies a new range state and reports how many values it had to clamp
        (advanced-ranges.md 1.3). Widening always reports zero; narrowing
        reports what it moved, and the caller says so rather than letting a
        clamp happen silently.

        Message thread. */
    int setRanges (const RangeState& newState);

    /*  setRanges as a user action: pushes an undo state first, so undoing a
        lock brings back the values it clamped (advanced-ranges.md 7), and
        pushes the result to the engine. Every UI route goes through this. */
    int changeRanges (const RangeState& newState, const juce::String& undoDescription);

    /*  guitar-workshop.md: the guitar as a bill of parts. The guitar-type
        parameter is a shortcut to a factory guitar file (0.6); the Workshop
        edits the current guitar directly. Message thread. */
    PartLibrary& getPartLibrary() noexcept { return partLibrary; }

    /** The Workshop bench's model (workshop-ui.md). */
    WorkshopBench& getBench() noexcept { return bench; }

    /*  workshop-ui.md 3.2 / ui-wiring.md 6.3: plays `candidate` without
        committing it - the committed guitar, its override, the parameters and
        the undo stack are untouched. nullptr puts the committed guitar back. */
    void auditionGuitar (const WorkshopGuitar* candidate);
    const WorkshopGuitar& getCurrentGuitar() const noexcept { return currentGuitar; }
    bool hasPartsGuitar() const noexcept { return partsGuitarLoaded; }

    /** The factory file a guitar type stands for, relative to Resources/Guitars. */
    static juce::String getFactoryGuitarPath (GuitarType type);

    /*  Makes `guitar` the instrument: maps it, applies it to the engine and
        writes its parts into the parameters that overlap them (string
        material, body, pickup types, circuit, setup), so the controls show
        the guitar that is playing. Missing-part and compatibility messages go
        to the notification queue and the error log. */
    void applyGuitar (const WorkshopGuitar& guitar, GuitarType standsFor,
                      const PartLibrary::LoadReport& report, bool writeParameters = true);

    /*  guitar-workshop.md 8: the Workshop's edits. The edited guitar becomes
        the instrument and, until it is saved as a file, travels whole in the
        preset's `guitar.override`. */
    void applyEditedGuitar (const WorkshopGuitar& guitar);

    /*  guitar-illustration.md 12: changes the guitar's family. Parts that suit
        the new family stay, the rest come from the family's template, and the
        guitar type follows the template (so bass mode and the type's defaults
        follow too). The banner line (12.1) is queued with the guitar notices.
        Undoable. False if the family is unknown. */
    bool switchGuitarFamily (const juce::String& family);

    /*  guitar-workshop.md 6 (Ctrl+G): writes the current guitar to the user
        guitars folder by reference - or with its parts alongside when
        `bundleParts` is set, for sharing - and points the preset at the new
        file. Returns the file, or an empty File if the write failed. */
    juce::File saveGuitarAs (const juce::String& name, bool bundleParts = false);

    /*  guitar-workshop.md 7: saves a fitted part's current fields as a user
        part under `name`, rescans the library and fits the saved part in its
        slot. Returns the saved part, or nullptr if the slot is empty or the
        write failed. */
    PartPtr savePartAs (GuitarSlot slot, const juce::String& name);

    /** The preset's `guitar` block for the current guitar (file-formats.md 2). */
    juce::var getGuitarBlock() const;

    /*  ambiguity-resolutions.md 4.5: the capo is the player's accessory
        (guitar-workshop.md 2) and a partial one clamps only the strings in
        its part's `string_mask`. The capo fret stays the `capo_fret`
        parameter; the part travels in the preset's guitar block. */
    void setCapoPart (const PartPtr& capo);
    PartPtr getCapoPart() const noexcept { return capoPart; }

    /*  The fitted slide (guitar-workshop.md 2, the drawer's Slide card): the
        engine plays its material, mass and size (slide-guitar.md 2.1). Travels
        in the preset's guitar block like the capo; nullptr is the engine's own
        default bar, which is what a preset without one gets (VISUAL-WORKSHOP-QA). */
    void setSlidePart (const PartPtr& slidePart);
    PartPtr getSlidePart() const noexcept { return slidePart; }

    /** A slide part's bar as the engine takes it; the default bar for nullptr. */
    static SlideBar slideBarFor (const Part* slidePart);

    /** "Factory/<Family>/<Name>.luthierguitar" or "User/<Name>.luthierguitar". */
    const juce::String& getGuitarReference() const noexcept { return guitarReference; }

    /** The file the reference resolves to, or an empty File. */
    juce::File getGuitarFile() const { return resolveGuitarReference (guitarReference); }

    /** True if the guitar was edited since it was loaded or saved. */
    bool isGuitarEdited() const noexcept { return ! guitarOverride.isVoid(); }

    /** The notices applyGuitar raised since the last call (gui-integration 15). */
    juce::StringArray takeGuitarNotices();

    /*  advanced-ranges.md 5: randomise stays inside stock by default. The
        preference is user-global and lives in UiPreferences, which the engine
        cannot see, so the editor mirrors it here. */
    void setRandomiseRespectsStock (bool shouldRespect) noexcept { randomiseRespectsStock = shouldRespect; }
    bool getRandomiseRespectsStock() const noexcept              { return randomiseRespectsStock; }

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
    // piano-roll-chord-display.md 2-3: what the strings sound (published by the
    // audio thread every block) and notes played from the piano roll's keys,
    // which reach the engine as channel 1 MIDI like any other note.
    const SoundingNotes& getSoundingNotes() const noexcept { return soundingNotes; }
    void playKeyboardNote (int midiNote, float velocity);
    void releaseKeyboardNote (int midiNote);
    /** Every note-on at the same sample, so the interpreter strums them as one chord. */
    void playKeyboardChord (const juce::Array<int>& midiNotes, float velocity);
    void releaseKeyboardChord (const juce::Array<int>& midiNotes);

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

    /*  One undo step for an action that writes several parameters (a style
        preset, a snapshot of values). Pushes the entry, then stops each
        write's change gesture from pushing its own until it goes out of
        scope - otherwise undo would reverse only the last write. */
    class ScopedUndoAction
    {
    public:
        ScopedUndoAction (LuthierAudioProcessor& p, const juce::String& description)
            : processor (p), wasSuppressed (p.gestureUndoSuppressed)
        {
            processor.pushUndoState (description);
            processor.gestureUndoSuppressed = true;
        }

        ~ScopedUndoAction() { processor.gestureUndoSuppressed = wasSuppressed; }

    private:
        LuthierAudioProcessor& processor;
        bool wasSuppressed;

        JUCE_DECLARE_NON_COPYABLE (ScopedUndoAction)
    };
    bool canUndo() const noexcept { return undoHistory.canUndo(); }

    /** How many actions can be undone (tests, and the Edit menu's count). */
    int getNumUndoSteps() const noexcept { return undoHistory.getNumUndoSteps(); }
    bool canRedo() const noexcept { return undoHistory.canRedo(); }
    void undo();
    void redo();

    // ---- action-and-undo.md 1, 4, 5, 9, 12 (the stack lives in Support/UndoHistory) ----
    /** An entry with a class and target, so repeats within 200 ms group (4). */
    void pushUndoAction (const juce::String& description, const juce::String& actionClass,
                         const juce::String& target);
    /** A state boundary (5): preset / guitar load, family switch, setlist step. */
    void pushUndoBoundary (const juce::String& description, const juce::String& actionClass = {});
    /** An entry for state outside the blob: undo/redo call the functions. */
    void pushUndoCallback (const juce::String& description, const juce::String& actionClass,
                           const juce::String& target, std::function<void()> undoFn,
                           std::function<void()> redoFn);
    bool isUndoStoppedAtBoundary() const noexcept { return undoHistory.isStoppedAtBoundary(); }
    /** Ctrl-Alt-Z: one undo that may cross a boundary. */
    void undoAcrossBoundary();
    int getNumRedoSteps() const noexcept { return undoHistory.getNumRedoSteps(); }
    /** Newest first; stepsBack undos reach the state before each entry. */
    juce::Array<UndoHistory::Item> getUndoHistory (int maxItems = UndoHistory::kMaxEntries) const { return undoHistory.getHistory (maxItems); }
    /** Undoes `steps` entries, crossing boundaries (the history list's click). */
    void undoSteps (int steps);
    void setUndoClock (std::function<double()> clock) { undoHistory.setClock (std::move (clock)); }
    juce::String getUndoDescription() const;
    juce::String getRedoDescription() const;

    /*  action-and-undo.md 3.8: a preset load from the UI - one entry named
        "Load preset [name]", the load, then the engine update. */
    bool loadPresetAsUserAction (int index);
    bool stepPresetAsUserAction (bool forward);

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

        /** onboarding.md 11 (TUNE-HELP-ONBOARDING): the practice drawer reopens as left. */
        bool practiceDrawerOpen = false;

        /** workshop-ui.md 7: the bench's eight A/B guitars, workspace not preset. */
        std::array<juce::var, 8> benchSlots;

        // piano-roll-chord-display.md 6: session state, not preset data.
        bool pianoRollExpanded = true;
        int  pianoRollHeight = 72;
        bool pianoLatch = false;
        bool pianoShowFingering = false;
        juce::Array<int> pianoLatchedNotes;
    };

    UiState& getUiState() noexcept { return uiState; }

    /** Host tempo, updated each block. */
    double getHostTempo() const noexcept { return hostTempo.load(); }

    /** Snapshot of the plugin state, for the exporter. */
    juce::MemoryBlock captureStateBlock();

    /** Factory used by the exporter to make an offline instance. */
    static std::unique_ptr<juce::AudioProcessor> createOfflineInstance();

    /** installer.md 6: what the constructor's first-run check found (first run,
        or the version this install upgraded from). */
    const InstallLayout::Result& getInstallLayoutResult() const noexcept { return installLayoutResult; }

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
    PerformanceCapture performanceCapture;
    bool tabDotsOnFretboard = false;   // MODEL-GAPS
    PresetMorph presetMorph { *this };
    std::array<int, kMaxStrings> captureOpenNotes {};
    int captureStringCount = -1, captureCapo = -1;
    juce::uint32 captureCapoMask = 0;
    int captureDrainTick = 0;
    AudioExporter exporter;
    Diagnostics diagnostics;
    RoutingMatrix routing;
    MidiOutRouter midiOutRouter;

    // midi-export.md 6: character / noise and Workshop events as Luthier SysEx.
    void sendLuthierSysEx (const MidiOutConfig& config, juce::MidiBuffer& midi, int numSamples) noexcept;

    struct WorkshopChange
    {
        char slot[32] {};
        char fit[96] {};
        char was[96] {};
    };

    static constexpr int kWorkshopQueue = 16;
    std::array<WorkshopChange, kWorkshopQueue> workshopChanges {};
    juce::AbstractFifo workshopFifo { kWorkshopQueue };
    LuthierSysExOut sysExOut;

    // midi-export 2.1 / 6 (MODEL-GAPS, TODO 10): the CHARACTER class's seed and
    // environment, sent when they change. Audio thread.
    uint64_t sentCharacterSeed = 0;
    int sentTemperature = -1, sentHumidity = -1;
    bool characterStated = false;
    void sendCharacterChanges() noexcept;

public:
    /** The CHARACTER event's environment in the file's units (character-wear 9's three steps). */
    static double temperatureCelsius (Temperature t) noexcept;
    static double humidityPercent (Humidity h) noexcept;

private:
    ModMatrix modMatrix;
    PatternLibrary patternLibrary;
    GenreKitLibrary genreKits;

    // tune-builder 8: the session is declared after the player it drives. The
    // player's chord channel joins the host MIDI (the rhythm engine strums
    // it); everything else goes to the engine as direct notes.
    TunePlayer tunePlayer;
    TuneSession tuneSession;
    TuneHumCapture humCapture;
    std::atomic<int> pendingTuneSection { -1 };
    std::atomic<int> tuneStateBoundaries { 0 };

    /** True while undo/redo restores a snapshot: the tune is not part of it. */
    bool restoringPluginUndo = false;
    juce::MidiBuffer tuneToEngine, tuneToMidiOut, tuneDirect;
    Metronome tuneClick;                  ///< fires on the tune's grid, not its own
    juce::AudioBuffer<float> tuneClickBuffer;
    bool tuneClickRinging = false;
    std::atomic<bool> clickToMain { false };

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

    // practice-tools 10-12: the routine runner and the practice history are
    // the processor's, so the drawer that advances them and the PRACTICE tab
    // that shows them see the same objects.
    PracticeRoutineRunner practiceRunner;
    PracticeStats practiceStats;
    juce::File practiceStatsFile { PracticeStats::getStatsFile() };
    PracticeActivityTracker practiceTracker;
    std::atomic<int> pendingDrawerTool { -1 };

    std::atomic<bool> practicePanelOpen { false };   // set by the UI, read by the audio thread

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

    /*  advanced-ranges.md: which parameter families this preset has unlocked.

        Preset state rather than user state (0.4), so it travels in the preset
        file and a preset sounds the same on someone else's machine. Owned here
        because it has to be applied to the APVTS before a preset's parameter
        values are written - see PresetManager::fromVar. */
    RangeState ranges;
    bool randomiseRespectsStock = true;

    /*  REALISM-A (string-aging.md 8, environment.md 6): the aging state and the
        environment reference from the state's character block, and the legacy
        temperature / humidity conversion. After the character engine's fromVar. */
    void applyRealismCharacterBlock (const juce::var& characterBlock);

    // The guitar as parts (guitar-workshop.md).
    bool loadGuitarForType (GuitarType type);
    bool loadGuitarFrom (const juce::String& reference, const juce::var& override,
                         GuitarType type, bool writeParameters);
    void writeGuitarParameters (const DerivedAcoustics& derived);
    bool guitarLoadKeepsHostWrites = false;   ///< only a type load defers to the host's writes

    /** strum-dynamics 4 / bass-techniques 8: moves the strum parameters still on
        one family's defaults to the other's. */
    void retargetStrumDefaults (bool fromBass, bool toBass);

    /*  A state load hands over its `guitar` block here; the guitar is then
        loaded (or kept) by loadGuitarForType when the bridge next applies. */
    void takeGuitarBlock (const juce::var& block);

    /** Resolves a preset's reference to a guitar file: user, then factory. */
    static juce::File resolveGuitarReference (const juce::String& reference);

    PartLibrary partLibrary;
    InstallLayout::Result installLayoutResult;   // installer.md 6
    PartPtr capoPart;
    PartPtr slidePart;   // VISUAL-WORKSHOP-QA: the fitted slide
    WorkshopGuitar currentGuitar;
    WorkshopBench bench { *this };
    bool partsGuitarLoaded = false;
    bool strumFamilyIsBass = false;   ///< strum-dynamics 4: the family the strum parameters' defaults follow
    juce::StringArray guitarNotices;

    /*  What the guitar is (file-formats.md 2): a reference to a guitar file,
        plus the whole guitar when it has been edited since. `guitarSourceType`
        is the guitar-type value this source goes with; the type moving away
        from it means the user picked another factory guitar. The key names
        what the engine has, so reapplying the same source is free and does
        not overwrite the parameters that refine it. */
    juce::String guitarReference;
    juce::var guitarOverride;
    int guitarSourceType = -1;
    juce::String loadedGuitarKey;

    /*  Set by a state load: its parameters were saved alongside this guitar
        and are the refinements to keep, so the load must not overwrite them
        with the guitar's own values. */
    bool guitarParametersFromState = false;

    /*  A factory preset or a reset names its guitar's parts as the winners
        (guitar block "partsWin") and lists the recipe values to restore over
        them ("keep", parameter id -> normalised). Applied after the parts are
        written, then cleared. Message thread. */
    juce::NamedValueSet pendingGuitarKeep;

    /** Set with partsWin: the parts are written even over values the preset or
        reset has just written (which the writtenSinceGuitarType guard would
        otherwise take for the host's). Cleared after the load. */
    bool guitarPartsWin = false;

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
    std::atomic<bool> transportWasRunning { false };   // read by getEffectiveTempo on the UI thread

    /** This block's tempo: the host's, or the tapped one when that wins. */
    double blockTempo = 120.0;

    double currentSampleRate = 44100.0;
    bool initialStateApplied = false;   ///< the bridge has built the instrument once (prepare or save)
    int currentBlockSize = 512;

    /*  A host may hand processBlock more samples than it promised in
        prepareToPlay; every scratch buffer here is sized from that promise, so
        such a block is rendered in slices of at most currentBlockSize. These
        carry each slice's MIDI in and the whole block's MIDI out, sized in
        prepareToPlay. */
    juce::MidiBuffer sliceMidi, sliceMidiOut;
    juce::MidiBuffer liveMidiKept;   ///< handleLiveMidi's output, sized in prepareToPlay

    /** The macro parameters' values, looked up once: a lookup by ID builds a
        String, which is an allocation the audio thread must not make. */
    std::array<std::atomic<float>*, ParamIDs::kNumMacros> macroValues {};

    void processSlice (juce::AudioBuffer<float>&, juce::MidiBuffer&);
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

    /*  Declick around structural changes (ParameterBridge::beforeStructuralChange).
        The message thread asks for a fade-out and waits (briefly) for the audio
        thread to finish it; the change is applied into silence; the audio thread
        then fades back in. The wait is skipped when no audio thread is running
        (offline, or a caller rendering on the message thread itself). */
    enum DeclickState { declickIdle = 0, declickFadingOut, declickSilent, declickFadingIn };
    std::atomic<int> declickState { declickIdle };
    std::atomic<double> lastAudioCallbackMs { 0.0 };
    std::atomic<juce::Thread::ThreadID> audioThreadId { nullptr };
    float declickGain = 1.0f;   ///< audio thread only
    int declickDepth = 0;       ///< message thread: nesting of fade requests
    void applyDeclick (juce::AudioBuffer<float>& buffer) noexcept;
    void fadeOutBeforeStructuralChange();
    void fadeInAfterStructuralChange();
    AuditionPhrase::Type auditionType = AuditionPhrase::Type::MajorScale;
    juce::MidiMessageSequence auditionSequence;
    int auditionEventIndex = 0;
    double auditionPositionSeconds = 0.0;
    double auditionEndSeconds = 0.0;

    // --- preview notes from the fretboard ---------------------------------------
    juce::MidiBuffer previewMidi;
    juce::CriticalSection previewLock;

    // --- piano-roll-chord-display.md 2 ---------------------------------------------
    SoundingNotes soundingNotes;
    SoundingNotesPublisher soundingPublisher;

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

    void undoOnce (bool crossBoundary);

    /*  action-and-undo.md 3.17 / 7: restores an entry's state but leaves the
        session layers (view, Live Mode, A/B, locks, tune, metronome) alone. */
    void applyUndoState (const juce::MemoryBlock& state);
    bool restoringForUndo = false;

    bool gestureUndoSuppressed = false;

    UndoHistory undoHistory;   // action-and-undo.md
    juce::File setlistFile;    // ui-wiring 17: the loaded setlist's reference
    double gestureStartMs = 0.0;

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
