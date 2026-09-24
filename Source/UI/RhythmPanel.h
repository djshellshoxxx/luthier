#pragma once

/*  The RHYTHM panel (rhythm-engine.md section 8).

    Everything this panel edits lives in the RhythmEngine, the PatternLibrary and
    the GenreKitLibrary rather than in the parameter tree, for the same reason the
    routing panel's controls do: a strum grid is a composition, not a knob a host
    should be automating, and giving each of its 32 steps a parameter would bury
    the parameters that matter. The engine's whole state travels with the preset
    through RhythmEngine::toVar, which is what rhythm-engine 9 asks for.

    The spec asks for this as a tab in Column 4's tab strip. Column 4 has no tab
    strip - it is a single scrolling list of sections - so the panel is built as
    one more section in that list, which is where the routing and mod-matrix
    panels already went.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Widgets.h"
#include "StrumGroup.h"
#include "BassGridGroup.h"   // bass-techniques 9 (MODEL-GAPS)
#include "PerformanceAssistUi.h"   // auto-articulation.md 7.2 (FEAT-ASSIST)
#include "../Rhythm/GenreKit.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** The strum grid (rhythm-engine 8.3): one cell per step, each cell a strum
    direction. Left-click cycles the direction, right-click opens the dynamic,
    mask and delete menu. */
class StrumGrid : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    explicit StrumGrid (LuthierAudioProcessor& processor);
    ~StrumGrid() override;

    /** Re-reads the pattern from the engine. */
    void refresh();

    /** Lights the cell the engine is currently playing. */
    void setPlayingStep (int step);

    std::function<void()> onPatternEdited;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int rowHeight = 30;
    static constexpr int preferredHeight = rowHeight * 2 + 6;

private:
    juce::Rectangle<int> cellBounds (int step) const;
    int stepAt (juce::Point<int> position) const;
    void showStepMenu (int step);
    void commit();

    LuthierAudioProcessor& processor;
    RhythmPattern pattern;

    int playingStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StrumGrid)
};

//==============================================================================
/** The fingerpick grid (rhythm-engine 8.4): five rows, one per finger, sixteen
    steps across. Clicking a cell assigns that step to that finger. */
class FingerpickGrid : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    explicit FingerpickGrid (LuthierAudioProcessor& processor);
    ~FingerpickGrid() override;

    void refresh();
    void setPlayingStep (int step);

    std::function<void()> onPatternEdited;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int rowHeight = 18;
    static constexpr int headerWidth = 22;
    static constexpr int preferredHeight = rowHeight * (int) Finger::numFingers + 18;

private:
    juce::Rectangle<int> cellBounds (int step, int finger) const;
    void commit();

    LuthierAudioProcessor& processor;
    RhythmPattern pattern;

    int playingStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FingerpickGrid)
};

//==============================================================================
/** Live indicators (rhythm-engine 8.7): the detected chord, the voicing as
    fretboard dots, and the direction of the next stroke. */
class RhythmIndicators : public juce::Component
{
public:
    explicit RhythmIndicators (LuthierAudioProcessor& processor);
    ~RhythmIndicators() override;

    void refresh();

    void paint (juce::Graphics&) override;

    static constexpr int preferredHeight = 78;

private:
    LuthierAudioProcessor& processor;

    juce::String chordText { "--" };
    std::array<int, kMaxStrings> voicedFrets {};
    int numStringsShown = 6;
    StrumType nextStroke = StrumType::rest;
    bool voicingIsValid = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RhythmIndicators)
};

//==============================================================================
class RhythmPanel : public juce::Component,
                    private juce::Timer
{
public:
    explicit RhythmPanel (LuthierAudioProcessor& processor);
    ~RhythmPanel() override;

    int preferredHeight() const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    void buildGenreControls();
    void buildVoicingControls();
    void buildFeelControls();
    void buildBrowser();

    void applySelectedKit();
    void randomiseWithinStyle();
    void refreshBrowserList();
    void loadSelectedPattern();
    void saveCurrentPattern();
    void exportCurrentPattern();
    void pushHumaniseToEngine();
    void refreshFromEngine();

    RhythmEngine& rhythm();

    LuthierAudioProcessor& processor;

    // --- header -------------------------------------------------------------------
    std::unique_ptr<LuthierToggle> enableToggle;
    juce::TextButton freeRunButton { "FREE-RUN" };
    juce::Label modeHintLabel;

    // --- genre kit ----------------------------------------------------------------
    juce::ComboBox genreBox;
    juce::TextButton diceButton { "*" };
    juce::Label rigHintLabel;

    // --- voicing ------------------------------------------------------------------
    juce::ComboBox styleBox;
    juce::Slider densitySlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider handPositionSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label capoLabel;
    juce::TextButton capoDown { "-" }, capoUp { "+" };

    // --- pattern editors ------------------------------------------------------------
    std::unique_ptr<StrumGrid> strumGrid;
    std::unique_ptr<FingerpickGrid> fingerpickGrid;

    // --- feel ---------------------------------------------------------------------
    juce::Slider swingSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider timingSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider velocitySlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider missSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider ghostSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    // gui-integration 4.4: the STRUM group (strum-dynamics 6.3). Its crossing
    // control and source line replace the old strum-duration slider.
    std::unique_ptr<StrumGroup> strumGroup;

    // bass-techniques 9 (MODEL-GAPS): the bass step grid, only on a bass.
    std::unique_ptr<BassGridGroup> bassGridGroup;

public:
    BassGridGroup* getBassGridGroup() const noexcept { return bassGridGroup.get(); }

    // auto-articulation.md 7.2 (FEAT-ASSIST): the PLAYING group, first in the tab.
    std::unique_ptr<PerformanceAssistGroup> playingGroup;
    PerformanceAssistGroup* getPlayingGroup() const noexcept { return playingGroup.get(); }

private:

    // --- browser ------------------------------------------------------------------
    juce::ComboBox tagFilterBox;
    juce::ListBox patternList;
    juce::TextButton loadButton { "LOAD" }, saveButton { "SAVE" }, exportButton { "EXPORT" };

    /** The pattern-library rows the tag filter currently admits. */
    juce::Array<int> visiblePatterns;

    class PatternListModel : public juce::ListBoxModel
    {
    public:
        explicit PatternListModel (RhythmPanel& o) : owner (o) {}

        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    private:
        RhythmPanel& owner;
    };

    PatternListModel listModel { *this };

    std::unique_ptr<RhythmIndicators> indicators;

    std::unique_ptr<juce::FileChooser> chooser;

    juce::Label genreHeading, voicingHeading, strumHeading, pickHeading,
                feelHeading, browserHeading;

    bool updatingControls = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RhythmPanel)
};

} // namespace luthier
