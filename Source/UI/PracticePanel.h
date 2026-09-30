#pragma once

/*  The practice drawer (practice-tools.md section 9).

    A slide-out from the bottom of the window with eight tabs across it. Collapsed
    it is a 32-pixel strip showing the tempo, the loop status and the track title,
    which is the only part of it that exists when it is closed - practice-tools
    0.1 says a closed tool consumes no CPU, so a collapsed drawer stops its timer
    and the processor stops rendering the tools at all.
*/

#include "AnimationPolicy.h"   // cpu-quality-modes 6
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
class TuningEngine;   // SPEC-SWEEP PT-39

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

    /** SPEC-SWEEP PT-34: a note the player played while this tab was showing. */
    virtual void notePlayed (int midiNote) { juce::ignoreUnused (midiNote); }

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
    std::unique_ptr<LuthierToggle> followToggle;    ///< SPEC-SWEEP PT-6: host / tap tempo
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

    /** SPEC-SWEEP (GD-31, gui-engine-dataflow 21): the looper's status LED -
        red pulsing at 4 Hz while recording or overdubbing, green while playing,
        unlit (transparent) when stopped. */
    static juce::Colour ledColourFor (Looper::State state, double nowMs) noexcept;
    juce::Colour getLedColour() const noexcept { return statusLed.colour; }

private:
    /** action-and-undo.md 3.14: a layer setting change as one grouped entry. */
    void editLayer (int layer, const char* what, const std::function<void()>& change);
    Looper& looper();

    struct StatusLed : public juce::Component
    {
        juce::Colour colour { juce::Colours::transparentBlack };
        void paint (juce::Graphics& g) override;
    };

    juce::TextButton transportButton { "Record" }, stopButton { "Stop" }, clearButton { "Clear" };
    juce::Label statusLabel;
    StatusLed statusLed;   // SPEC-SWEEP GD-31

    /** One strip per layer: select, mute, mode, level, pan, and undo. */
    struct LayerStrip
    {
        std::unique_ptr<juce::TextButton> select, mute, reverse, halfSpeed, undo;
        std::unique_ptr<juce::ComboBox> mode;
        std::unique_ptr<juce::Slider> level, pan;
        std::unique_ptr<juce::Slider> lowCut, highCut;   // SPEC-SWEEP PT-20
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

    // SPEC-SWEEP PT-26 (practice-tools 3): pan, low-cut and high-cut.
    juce::Slider panSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider lowCutSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider highCutSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    // SPEC-SWEEP PT-30: the name the next marker gets.
    juce::TextEditor markerName;

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

    /** SPEC-SWEEP PT-34: a played note answers the quiz; a right one asks the next. */
    void notePlayed (int midiNote) override;

    /** SPEC-SWEEP PT-33: what the fretboard's dots say. */
    enum class Overlay { notes = 0, intervals, degrees, numOverlays };

    /** SPEC-SWEEP PT-33: the label a pitch class gets under the overlay, or
        empty when it is not in the scale. */
    juce::String labelFor (int pitchClass, Overlay overlay);

    /** SPEC-SWEEP PT-37: a custom scale from its steps ("2 1 2 2 1 2 2"). False
        when the text is not a step list inside an octave. */
    bool setCustomSteps (const juce::String& steps);

    void paint (juce::Graphics& g) override;   // SPEC-SWEEP PT-33: the scale on a fretboard

private:
    ScaleTrainer& trainer();
    juce::Label feedbackLabel;   // SPEC-SWEEP PT-34
    juce::ComboBox overlayBox;   // SPEC-SWEEP PT-33

    // SPEC-SWEEP PT-35: the interval answers, and when the question was asked.
    juce::OwnedArray<juce::TextButton> intervalButtons;
    double askedAtMs = 0.0;

    void playQuestion();

public:
    /** SPEC-SWEEP PT-35: answers the interval trainer as its button does. */
    void chooseInterval (int semitones);

    /** SPEC-SWEEP PT-35: poses the next question, as Ask does. */
    void ask();
    juce::TextEditor customSteps;   // SPEC-SWEEP PT-37

    juce::ComboBox keyBox, scaleBox, modeBox;
    juce::Label questionLabel, scoreLabel;
    juce::TextButton nextButton { "Ask" };

    /** The scale drawn on a fretboard, which is what "explore" means. */
    juce::Component scaleView;

    juce::Random random { 0x5ca1e5 };
};

//==============================================================================
class EarTab final : public PracticeTab
{
public:
    explicit EarTab (LuthierAudioProcessor& processor);

    void refresh() override;
    void resized() override;

    /** SPEC-SWEEP PT-39: where each note of a question is played - string and
        fret, the lowest free position that sounds it (an octave over when out
        of reach); -1 when no string is free. Returns `count`. */
    static int placeOnStrings (const TuningEngine& tuning, int numStrings, const int* midiNotes, int count,
                               int* stringsOut, int* fretsOut);

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
};

//==============================================================================
class TabReaderTab final : public PracticeTab
{
public:
    explicit TabReaderTab (LuthierAudioProcessor& processor);

    void refresh() override;
    void resized() override;

    /*  Opens a tab file as the Open button's chooser does, and notes it in the
        library's recent list (practice-tools 11.2). `libraryFile` is the
        library it is noted in. True when the file was read. MODEL-GAPS (TODO
        11: tested through here rather than the chooser). */
    bool openTab (const juce::File& file, const juce::File& libraryFile = PracticeLibrary::getLibraryFile());

    const PerformanceScore& getScore() const noexcept { return score; }

private:
    juce::TextButton openButton { "Open..." }, exportButton { "Export..." };
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

    void refresh() override;
    void resized() override;

private:
    /** practice-tools 11.2: the ring length and what to record are set on the
        PRACTICE tab ("the settings, not the transport"), saved in
        defaults.json; the drawer applies them when the recorder goes on. */
    static SessionRecorderSetup storedSetup();

    std::unique_ptr<LuthierToggle> enableToggle;

    /*  midi-export 4.2 / TODO 10 (MODEL-GAPS): the Save button is also a drag
        source. Dragging it saves the take if it has not been, then drags the
        saved MIDI (and the WAV) out to the host or the desktop. */
    class SaveButton final : public juce::TextButton
    {
    public:
        explicit SaveButton (SessionTab& t) : juce::TextButton ("Save last take"), tab (t) {}
        void mouseDown (const juce::MouseEvent& e) override { dragged = false; juce::TextButton::mouseDown (e); }
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;

        /*  What a drag hands out now, for the tests (saving first if nothing is
            saved): midi-export 4.2's MIDI file - the take's span of the
            performance capture, Luthier profile, or Generic with Alt - then
            the WAV. The recorder's raw MIDI stands in when nothing was captured. */
        juce::StringArray filesToDrag (bool forceGeneric = false);

    private:
        SessionTab& tab;
        bool dragged = false;
    };

public:
    SaveButton& getSaveButton() noexcept { return saveButton; }
    LuthierToggle& getEnableToggle() noexcept { return *enableToggle; }
    juce::String getStatusText() const { return statusLabel.getText(); }

    /** SPEC-SWEEP (GD-33, gui-engine-dataflow 23): recorded / capacity, drawn as
        a fill bar under the status line. */
    float getFillFraction() const noexcept { return fillFraction; }
    void paint (juce::Graphics&) override;

private:
    bool saveTake();
    float fillFraction = 0.0f;
    juce::Rectangle<int> fillBarBounds;

    SaveButton saveButton { *this };
    juce::TextButton openFolderButton { "Open folder" };
    juce::Label lengthLabel, statusLabel, warningLabel;
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

private:
    // cpu-quality-modes 6: the motion switch.
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "PracticePanel" };
};

} // namespace luthier
