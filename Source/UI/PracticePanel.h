#pragma once

/*  The practice drawer (practice-tools.md section 9).

    A slide-out from the bottom of the window with eight tabs across it. Collapsed
    it is a 32-pixel strip showing the tempo, the loop status and the track title,
    which is the only part of it that exists when it is closed - practice-tools
    0.1 says a closed tool consumes no CPU, so a collapsed drawer stops its timer
    and the processor stops rendering the tools at all.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Widgets.h"
#include "../Practice/Metronome.h"
#include "../Practice/Looper.h"
#include "../Practice/BackingTrack.h"
#include "../Practice/Trainers.h"
#include "../Practice/PracticeRoutine.h"
#include "../Practice/PracticeRoutineSetup.h"
#include "../Notation/NotationExport.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** The four-dot beat indicator (practice-tools 1). */
class BeatIndicator : public juce::Component
{
public:
    explicit BeatIndicator (LuthierAudioProcessor& processor);

    void refresh();

    void paint (juce::Graphics&) override;

private:
    LuthierAudioProcessor& processor;

    int lastBeat = -1;
    bool lastSilent = false;
};

//==============================================================================
/** One tab's contents. Each is a plain component so the drawer can show one at
    a time without them all being laid out at once. */
class PracticeTab : public juce::Component
{
public:
    explicit PracticeTab (LuthierAudioProcessor& p) : processor (p) {}

    /** Called by the drawer's timer while this tab is the visible one. */
    virtual void refresh() {}

protected:
    LuthierAudioProcessor& processor;
};

//==============================================================================
class MetronomeTab final : public PracticeTab
{
public:
    explicit MetronomeTab (LuthierAudioProcessor& processor);

    void refresh() override;
    void resized() override;

private:
    Metronome& metronome();

    std::unique_ptr<LuthierToggle> enableToggle;
    std::unique_ptr<LuthierToggle> mainOutToggle;   ///< practice-tools 0.2
    juce::Slider tempoSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ComboBox signatureBox, subdivisionBox, soundBox;
    juce::Slider levelSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider silentBarsSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    // Progressive tempo (practice-tools 1).
    juce::Slider fromSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider toSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider overBarsSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextButton rampButton { "Ramp" };

    /** One button per beat of the bar, cycling through the accent levels. */
    juce::OwnedArray<juce::TextButton> accentButtons;

    std::unique_ptr<BeatIndicator> indicator;

    bool updatingControls = false;
};

//==============================================================================
class LooperTab final : public PracticeTab
{
public:
    explicit LooperTab (LuthierAudioProcessor& processor);

    void refresh() override;
    void resized() override;

private:
    Looper& looper();

    juce::TextButton transportButton { "Record" }, stopButton { "Stop" }, clearButton { "Clear" };
    juce::Label statusLabel;

    /** One strip per layer: select, mute, mode, level, pan, and undo. */
    struct LayerStrip
    {
        std::unique_ptr<juce::TextButton> select, mute, reverse, halfSpeed, undo;
        std::unique_ptr<juce::ComboBox> mode;
        std::unique_ptr<juce::Slider> level, pan;
    };

    std::array<LayerStrip, Looper::kMaxLayers> layers;

    juce::TextButton exportMix { "Bounce" }, exportStems { "Stems" };
    juce::TextButton saveButton { "Save" }, loadButton { "Load" };

    std::unique_ptr<juce::FileChooser> chooser;
};

//==============================================================================
class TrackTab final : public PracticeTab
{
public:
    explicit TrackTab (LuthierAudioProcessor& processor);

    void refresh() override;
    void resized() override;

private:
    BackingTrackPlayer& track();

    juce::TextButton openButton { "Open..." }, playButton { "Play" }, stopButton { "Stop" };
    juce::Label titleLabel, positionLabel, tempoLabel;

    juce::Slider positionSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Slider levelSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider pitchSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider tempoSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    juce::TextButton loopButton { "Loop" }, monoButton { "Mono" };
    juce::TextButton setLoopStart { "Loop in" }, setLoopEnd { "Loop out" };
    juce::TextButton addMarker { "Mark" };
    juce::ComboBox markerBox;

    std::unique_ptr<juce::FileChooser> chooser;

    bool draggingPosition = false;
};

//==============================================================================
class ScaleTab final : public PracticeTab
{
public:
    explicit ScaleTab (LuthierAudioProcessor& processor);

    void refresh() override;
    void resized() override;

private:
    ScaleTrainer& trainer();

    juce::ComboBox keyBox, scaleBox, modeBox;
    juce::Label questionLabel, scoreLabel;
    juce::TextButton nextButton { "Ask" };

    /** The scale drawn on a fretboard, which is what "explore" means. */
    juce::Component scaleView;

    juce::Random random { 0x5ca1e5 };

    /** 11.2 question count: the press after the session's summary starts the
        next session, once the completed one is in the history. */
    bool summaryShown = false;
};

//==============================================================================
class EarTab final : public PracticeTab
{
public:
    explicit EarTab (LuthierAudioProcessor& processor);

    void refresh() override;
    void resized() override;

private:
    EarTrainer& trainer();

    void playCurrentQuestion();

    juce::ComboBox exerciseBox;
    juce::TextButton playButton { "Play" }, nextButton { "New" };
    juce::TextButton adaptiveButton { "Adaptive" };
    juce::Label questionLabel, scoreLabel, feedbackLabel;

    juce::OwnedArray<juce::TextButton> choiceButtons;

    juce::Random random { 0xea12 };

    int notes[EarTrainer::kMaxNotesInQuestion] {};
    double offsets[EarTrainer::kMaxNotesInQuestion] {};
    int numNotes = 0;

    bool summaryShown = false;
};

//==============================================================================
class TabReaderTab final : public PracticeTab
{
public:
    explicit TabReaderTab (LuthierAudioProcessor& processor);

    /** Reads a tab file into the view - what Open... does once a file is
        chosen - and notes it in the PRACTICE tab's recent list (11.2
        "Tab files opened recently"). False, with the reason in the status
        line, when it could not be read; an unreadable file is not "recent". */
    bool openTabFile (const juce::File& file);

    /** Where the recent list is kept: the user's library.json unless a test
        points it elsewhere. */
    void setLibraryFile (const juce::File& file) { libraryFile = file; }

    void refresh() override;
    void resized() override;

private:
    juce::TextButton openButton { "Open..." }, exportButton { "Export..." };
    juce::File libraryFile { PracticeLibrary::getLibraryFile() };
    juce::Label statusLabel;
    juce::TextEditor tabView;
    juce::ComboBox formatBox;
    juce::Slider barsSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    PerformanceScore score;
    NotationExporter exporter;
    NotationImporter importer;

    std::unique_ptr<juce::FileChooser> chooser;
};

//==============================================================================
class ProgressionTab final : public PracticeTab
{
public:
    explicit ProgressionTab (LuthierAudioProcessor& processor);

    void refresh() override;
    void resized() override;

private:
    juce::TextEditor entryBox;
    juce::TextButton parseButton { "Set" }, playButton { "Play" };
    juce::Label statusLabel, currentChordLabel;
    juce::ComboBox genreBox;
};

//==============================================================================
class SessionTab final : public PracticeTab
{
public:
    explicit SessionTab (LuthierAudioProcessor& processor);

    /** The transport: on sizes the ring from the PRACTICE tab's setup and
        starts; off stops and, with auto-save set there, writes the take. */
    juce::Button& getEnableButton() noexcept { return enableToggle->getButton(); }

    /** "Save last take": a click writes the take to the Sessions folder; a
        drag out of the window carries it as files (midi-export 4.2). */
    juce::Button& getSaveButton() noexcept;

    juce::String getStatusText() const { return statusLabel.getText(); }

    /** Where the PRACTICE tab's setup is read from: defaults.json unless a
        test points it elsewhere. */
    void setDefaultsFile (const juce::File& file) { defaultsFile = file; }

    void refresh() override;
    void resized() override;

private:
    class SaveButton;

    /** practice-tools 11.2: the ring length and what to record are set on the
        PRACTICE tab ("the settings, not the transport"), saved in
        defaults.json; the drawer applies them when the recorder goes on. */
    SessionRecorderSetup storedSetup() const;

    void setRecorderEnabled (bool on);

    std::unique_ptr<LuthierToggle> enableToggle;
    std::unique_ptr<SaveButton> saveButton;
    juce::TextButton openFolderButton { "Open folder" };
    juce::Label lengthLabel, statusLabel, warningLabel;
    juce::File defaultsFile { PracticeDefaults::getDefaultsFile() };
    double requestedMinutes = 0.0;
    double storedMinutes = SessionRecorder::kDefaultMinutes;   ///< read when shown, not per refresh

    void visibilityChanged() override;
};

//==============================================================================
class PracticePanel : public juce::Component,
                      private juce::Timer
{
public:
    /** practice-tools 9: 32 px collapsed, up to 360 open. */
    static constexpr int collapsedHeight = 32;
    static constexpr int defaultOpenHeight = 260;
    static constexpr int maximumHeight = 360;

    explicit PracticePanel (LuthierAudioProcessor& processor);
    ~PracticePanel() override;

    void setOpen (bool shouldBeOpen);
    bool isOpen() const noexcept { return open; }

    /** Shows a tool's tab (a routine entry, or the PRACTICE tab's START). */
    void showTool (PracticeTool tool) { showTab ((int) tool); }
    int getCurrentTab() const noexcept { return currentTab; }

    /** The TAB tab's Open..., without the chooser: reads the file and notes it
        in the recent list. */
    bool openTabFile (const juce::File& file);

    /** The SESSION tab's controls, for tests and the editor. */
    SessionTab& getSessionTab() noexcept;

    /** Points the drawer's own files - the recent-tab library and the session
        setup it reads - at a test folder instead of Documents/Luthier. */
    void setLibraryFile (const juce::File& file);
    void setDefaultsFile (const juce::File& file);

    /** The drawer's clock, for tests: counts `seconds` of practice as the
        20 Hz timer would - the routine advances, the tools in use gain
        minutes, and the drawer follows the routine's tool. */
    void tick (double seconds);

    /** The height the editor should give it. */
    int preferredHeight() const noexcept { return open ? openHeight : collapsedHeight; }

    std::function<void()> onHeightChanged;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    void showTab (int index);
    void refreshRoutineStrip();
    void saveStats();

    LuthierAudioProcessor& processor;

    /** practice-tools 10: a running routine's readout and its controls. */
    juce::Label routineReadout;
    juce::TextButton routinePause { "Pause" }, routineNext { "Next" }, routineStop { "Stop" };

    double lastTickMs = 0.0;
    double secondsSinceSave = 0.0;
    bool pausedByClosing = false;

    bool open = false;
    int openHeight = defaultOpenHeight;

    /** The collapsed strip's readouts. */
    juce::Label stripTempo, stripLoop, stripTrack;

    juce::TextButton toggleButton { "PRACTICE" };

    /** practice-tools 9: the drawer strip's own controls. */
    juce::Slider practiceLevel { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::TextButton tapButton { "Tap" }, panicButton { "Panic" };

    juce::OwnedArray<juce::TextButton> tabButtons;
    juce::OwnedArray<PracticeTab> tabs;

    int currentTab = 0;

    /** Where a resize drag started, so the drawer can be dragged taller. */
    int dragStartHeight = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PracticePanel)
};

} // namespace luthier
