#pragma once

/*  Advanced mode, gui-integration.md section 4.

    A compressed guitar-and-fretboard strip across the top, then the four columns
    section 4 specifies:

      1  GUITAR, BODY, STRINGS, WHAMMY          - the instrument
      2  PICKUPS, CIRCUIT, PRE-EFFECTS RACK     - signal capture
      3  AMP, POST-EFFECTS RACK, CAB, ROOM, SUSTAIN
      4  a tabbed workspace, which is also where section 19 puts CONTROLLERS

    Columns 1 to 3 are fixed and independently scrollable; column 4 is chosen by
    a tab strip at its top. What each column holds that section 4 does not name,
    and what section 4 names that is not built, is in GAPS.md A1 and A2.

    Every control here is the same widget as in Easy mode, so right-click, MIDI
    Learn, lock and randomise work identically throughout.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "PianoRollStrip.h"
#include "RoutingPanel.h"
#include "ModMatrixPanel.h"
#include "RhythmPanel.h"
#include "ToneMatchPanel.h"
#include "CharacterPanel.h"
#include "LivePanel.h"
#include "CircuitPanel.h"
#include "WorkshopPanel.h"
#include "FretboardComponent.h"
#include "GuitarBodyComponent.h"
#include "PedalRack.h"
#include "AmpFacePanel.h"
#include "TunePanel.h"
#include "JamPanel.h"   // FEAT-JAM
#include "PracticeSetupPanel.h"
#include "HelpTab.h"
#include "RiffBrowser.h"         // riff-library 7.1
#include "WorkspaceTabStrip.h"   // FEAT-RIFFS: the strip scrolls when the tabs overflow
#include "PanelHelpButton.h"
#include "MicPlacementEditor.h"   // mic-placement.md 6 (FEAT-MIC)
#include "Techniques/TechniquesPanel.h"   // gui-techniques-updates.md 1 (TECHNIQUES)

namespace luthier
{

class LuthierAudioProcessor;
class ControllersPage;
class MidiOutPanel;
class NotationPanel;

//==============================================================================
/** One row of the string list. */
class StringRow : public juce::Component,
                  public juce::SettableTooltipClient,
                  private juce::Timer
{
public:
    StringRow (LuthierAudioProcessor& processor, int stringIndex);
    ~StringRow() override;

    void setSelected (bool selected);
    bool isSelected() const noexcept { return selected; }

    std::function<void (int)> onSelected;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int preferredHeight = 34;

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    int stringIndex;
    bool selected = false;

    juce::String noteText, tensionText;
    double tensionNewtons = 0.0;
    bool tensionPlayable = true;
    bool muted = false;

    juce::Rectangle<int> muteBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StringRow)
};

//==============================================================================
class AdvancedPanel : public juce::Component
{
public:
    explicit AdvancedPanel (LuthierAudioProcessor& processor);
    ~AdvancedPanel() override;

    void setSelectedString (int index);

    /** cpu-quality-modes 5: column 3's Master oversampling tooltip carries the
        "Running at 2x while quality is Medium." note ("" when not capped). */
    void setOversamplingNote (const juce::String& note);
    int getSelectedString() const noexcept { return selectedString; }

    /*  Section 4.5: below 1000 points wide, Advanced Mode is unavailable. Three
        220-point columns and a 480-point workspace do not fit, and shrinking
        them further produces a column too narrow to read. The editor asks this
        before it switches modes. */
    static constexpr int minimumUsableWidth = 1000;

    FretboardComponent& getFretboard() noexcept { return fretboard; }

    /** piano-roll-chord-display.md 1: the strip under the fretboard. */
    PianoRollStrip& getPianoRoll() noexcept { return *pianoRoll; }

    //==========================================================================
    /*  Column 4's tab strip (section 4.4).

        Public because two things outside the panel need it: the editor, which
        binds Ctrl+[ and Ctrl+] to step the tabs, and the test that walks them.
        Everything here is in tab order, which is section 4.4's order restricted
        to the tabs that have a panel behind them.
    */
    int getNumWorkspaceTabs() const noexcept { return workspacePanels.size(); }
    int getWorkspaceTab() const noexcept { return workspaceTab; }

    juce::String getWorkspaceTabName (int index) const;

    /** The panel behind a tab, or nullptr if the index is out of range. */
    juce::Component* getWorkspacePanel (int index) const;

    /** The WORKSHOP tab's bench (gui-integration.md 6). */
    WorkshopPanel* getWorkshopPanel() const noexcept { return workshopPanel.get(); }

    /** True while the WORKSHOP tab has taken over columns 3 and 4. */
    bool isWorkshopShowing() const noexcept;

    /*  mic-placement.md 6.1 / 6.2 (FEAT-MIC): the CAB section's placement view,
        and the expanded editor, which takes over Columns 3 and 4 the way the
        Workshop does while Column 4's tab strip stays visible. */
    MicPlacementView* getMicPlacementView() const noexcept { return micView.get(); }
    MicPlacementEditor* getMicPlacementEditor() const noexcept { return micEditor.get(); }
    void openMicEditor();
    void closeMicEditor();
    bool isMicEditorShowing() const noexcept { return micEditor != nullptr && micEditor->isVisible(); }

    /** The HELP tab (gui-integration 4.4). */
    HelpTab* getHelpTab() const noexcept { return helpTab.get(); }

    /** riff-library 7.1: the RIFFS tab, between TUNE and LIVE. */
    RiffBrowser* getRiffsPanel() const noexcept { return riffsPanel.get(); }

    /** The strip the tab buttons live in (it scrolls when they overflow). */
    WorkspaceTabStrip& getWorkspaceTabStrip() noexcept { return workspaceStrip; }
    /** The TECHNIQUES tab (gui-techniques-updates.md 1). */
    TechniquesPanel* getTechniquesPanel() const noexcept { return techniquesPanel.get(); }

    /** Opens the HELP tab pinned to a topic (a tab name, a column section, an
        Options page). */
    void showHelp (const juce::String& topic);

    /** accessibility 2 / gui-integration 17: what F1 should explain - the
        workspace tab or column section holding the focused component, else
        the tab on show; empty when that is HELP itself. */
    juce::String getHelpContextFor (const juce::Component* focused) const;

    void setWorkspaceTab (int index);

    //==========================================================================
    /*  TUNE-HELP-ONBOARDING (gui-integration 20, onboarding 3 and 4): the ?
        icons on every column section and on the workspace, and where the tour
        and the first-week hints find things. */
    std::vector<PanelHelpButton*> getHelpButtons() const;
    PanelHelpButton& getWorkspaceHelpButton() noexcept { return workspaceHelp; }
    juce::Component* getColumnViewport (int column) noexcept;
    juce::Button* getWorkspaceTabButton (const juce::String& tabName) const;
    juce::Rectangle<int> getWorkspaceTabStripBounds() const noexcept { return workspaceTabStrip; }

    /*  Selects a tab by the name on it. For callers that want a particular panel
        and should not have to know where it sits - a notification banner offering
        to show the user the IR slots, for one. Section 4.4 fixes the order, but
        the eight unbuilt tabs arriving would shift every index, and a banner that
        quietly opened the wrong panel is worse than one that did nothing.

        Returns false when there is no such tab, which is how a caller learns the
        panel it wanted has not been built yet. Case-insensitive. */
    bool setWorkspaceTabNamed (const juce::String& tabName);

    /** Steps by delta, wrapping. Section 17's Ctrl+[ and Ctrl+] - wrapping
        because a tab strip that stops at the end makes the last tab need two
        different keys to leave. */
    void stepWorkspaceTab (int delta);

    /** The key the last-used tab is stored under in UiPreferences (4.4), by
        name: tabs are added in the middle of the strip's fixed order, and an
        index would then reopen a different tab. The index key is read only
        when no name is stored (a file from an older build). */
    static constexpr const char* workspaceTabPreferenceKey = "advanced.workspaceTab";
    static constexpr const char* workspaceTabNamePreferenceKey = "advanced.workspaceTabName";

    //==========================================================================
    // global-search.md 3.2 (FEAT-SEARCH): column section headings are drawn,
    // not components, so the palette asks here.

    /** The section headings of column 1-3, in order. */
    juce::StringArray getColumnSections (int column) const;

    /** The column (1-3) and section heading holding `c`; empty if none. */
    juce::String getColumnSectionFor (const juce::Component* c, int& column) const;

    /** Brings a column section on screen (leaving WORKSHOP if it hides the
        column) and scrolls its heading into view. */
    bool revealColumnSection (const juce::String& heading);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Column : public juce::Component
    {
    public:
        explicit Column (const juce::String& title);

        void addSection (const juce::String& heading);
        void addControl (juce::Component* component, int height);
        void addGap (int height);

        /** Lays the accumulated content out in a single column and returns the
            total height, for the viewport. */
        int layout (int width);

        void paint (juce::Graphics&) override;
        void resized() override;

        /** The heading of the section holding `c`, or empty. */
        juce::String getSectionContaining (const juce::Component* c) const;

        /** FEAT-SEARCH: the headings, and where one starts (-1 if absent). */
        juce::StringArray getSections() const;
        int getSectionY (const juce::String& heading) const;
        /** Retitles a section in place (mic-placement.md 6.1: "Microphones"). */
        void renameSection (const juce::String& from, const juce::String& to);

        /** gui-integration 16 and 20: the section heading at a height, the ?
            on each heading, and Help on either. */
        juce::String getSectionAt (int y) const;
        std::function<void (const juce::String&)> onHelp;
        juce::OwnedArray<PanelHelpButton> helpButtons;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        struct Item
        {
            juce::Component* component = nullptr;
            juce::String heading;
            int height = 0;
            bool isGap = false;
            PanelHelpButton* help = nullptr;
        };

        juce::String title;
        juce::Array<Item> items;
        int contentHeight = 0;
    };

    void buildColumn1();
    void buildColumn2();
    void buildColumn3();

    /** Column 4: the tab strip and the panels behind it (section 4.4). */
    void buildWorkspace();

    /** @param remember  false while restoring, so reading the stored tab back
                         does not immediately write it out again. */
    void showWorkspaceTab (int index, bool remember = true);

    LuthierAudioProcessor& processor;

    GuitarBodyComponent guitarBody;
    FretboardComponent fretboard;

    // piano-roll-chord-display.md 1: under the fretboard, the strip grown by its height.
    std::unique_ptr<PianoRollStrip> pianoRoll;
    int getGuitarStripHeight (int boundsHeight) const;

    // Columns 1 to 3. Column 4 is the workspace below, which is not a Column:
    // it shows one panel at a time rather than stacking them.
    juce::Viewport viewports[3];
    std::unique_ptr<Column> columns[3];

    juce::OwnedArray<juce::TextButton> workspaceTabs;
    WorkspaceTabStrip workspaceStrip;
    juce::Array<juce::Component*> workspacePanels;
    juce::Viewport workspaceViewport;
    int workspaceTab = 0;

    // gui-integration 20: the workspace panel's ?, at the end of the tab strip.
    PanelHelpButton workspaceHelp;
    juce::Rectangle<int> workspaceTabStrip;

    /*  Where resized() put the column dividers, so paint() draws them in the
        same places. Below 1280 the layout stacks columns 2 and 3, and a paint
        that recomputed the geometry itself would eventually disagree with it. */
    juce::Array<int> dividerX;
    int workspaceLeft = 0;

    int selectedString = 0;

    // --- column 1 -----------------------------------------------------------------
    juce::OwnedArray<StringRow> stringRows;
    std::unique_ptr<LuthierChoice> stringMaterial, stringGauge;
    std::unique_ptr<LuthierKnob> stringAgeHours, bodyCoupling;   // REALISM-A
    std::unique_ptr<LuthierKnob> realismDetune, intonation, sustain;
    std::unique_ptr<DecayRow> decayRow;   // sustain-and-decay.md 8 (REALISM-C)
    std::unique_ptr<LuthierToggle> driftToggle;

    // --- column 2 -----------------------------------------------------------------
    std::unique_ptr<LuthierChoice> temperament, capo;
    std::unique_ptr<LuthierKnob> concertA, couplingAmount, fretAction, fretBuzz;
    std::unique_ptr<LuthierToggle> fretlessToggle, slideGuitarToggle;

    // Sustain (ambiguity-resolutions 2.3): two rows, two mechanisms.
    std::unique_ptr<LuthierToggle> freezeEnable, ebowToggle;
    std::unique_ptr<LuthierKnob> freezeCapture, freezeLevel, freezeAttack,
                                 freezeRelease, freezeLowPass, freezeHighPass;
    std::unique_ptr<juce::Label> stringInfoLabel;

    // --- column 3 -----------------------------------------------------------------
    std::unique_ptr<LuthierChoice> bodyMode, bracing, topWood, backWood;
    std::unique_ptr<LuthierKnob> bodyAmount, bodyWidth, bodyDepth, topThickness,
                                 soundhole, bodyAge, airGain;

    std::unique_ptr<LuthierChoice> pickupSelector;
    std::unique_ptr<LuthierKnob> pickupBlend;   // SPEC-SWEEP: SP-17
    std::unique_ptr<LuthierChoice> pickupType[3];
    std::unique_ptr<LuthierChoice> pickupMagnet[3];
    std::unique_ptr<LuthierKnob> pickupVolume[3];
    std::unique_ptr<LuthierToggle> coilTap;
    std::unique_ptr<LuthierKnob> piezoMicBlend, guitarTone, guitarVolume;

    std::unique_ptr<LuthierToggle> useFingers;
    std::unique_ptr<LuthierChoice> pickMaterial;
    std::unique_ptr<LuthierKnob> pickThickness, pickAngle, pluckPosition, nailVsFlesh;
    std::unique_ptr<LuthierKnob> slideNoise, fretNoise, releaseNoise, bodyKnock, pickNoise, ampBuzz;

    // --- column 4 -----------------------------------------------------------------
    std::unique_ptr<LuthierChoice> ampModel;

    /** visual-polish.md 2: the amp's face, carrying gain, bass, mid, treble,
        presence, master and the bright, mid boost and standby switches. */
    std::unique_ptr<AmpFacePanel> ampFace;

    std::unique_ptr<LuthierToggle> cabOn, dualMic;
    std::unique_ptr<MicPlacementView> micView;          // FEAT-MIC
    std::unique_ptr<MicPlacementEditor> micEditor;      // FEAT-MIC
    juce::String micSectionTitle;                       // FEAT-MIC
    std::unique_ptr<LuthierChoice> cabType, cabSpeaker, micType, micPosition, micDistance,
                                   micType2, micPosition2, micDistance2;
    std::unique_ptr<LuthierKnob> speakerAge, micBlend, micWidth, micPhase;

    std::unique_ptr<LuthierToggle> roomOn;
    std::unique_ptr<LuthierChoice> roomSize, roomMaterial;
    std::unique_ptr<LuthierKnob> roomBlend, roomDecay, roomWidth;

    // CIRCUIT (volume-knob-interaction.md 5), which replaced CABLE.
    std::unique_ptr<CircuitResponseView> circuitView;
    std::unique_ptr<StandardValueChoice> volumePotChoice, tonePotChoice, toneCapChoice;
    std::unique_ptr<LuthierKnob> volumePot, tonePot, toneCap, bleedR, bleedC, ampInput;
    std::unique_ptr<LuthierChoice> potTaper, trebleBleed, bleedMode, cableQuality;
    std::unique_ptr<LuthierToggle> circuitActive, cableOn;
    std::unique_ptr<LuthierKnob> cableLength;

    std::unique_ptr<RoutingPanel> routingPanel;
    std::unique_ptr<WorkshopPanel> workshopPanel;
    std::unique_ptr<ModMatrixPanel> modMatrixPanel;
    std::unique_ptr<RhythmPanel> rhythmPanel;
    std::unique_ptr<TunePanel> tunePanel;
    std::unique_ptr<JamPanel> jamPanel;   // FEAT-JAM: jam-mode 8.1
    std::unique_ptr<RiffBrowser> riffsPanel;   // riff-library 7.1
    std::unique_ptr<PracticeSetupPanel> practiceSetupPanel;
    std::unique_ptr<HelpTab> helpTab;
    std::unique_ptr<LivePanel> livePanel;
    std::unique_ptr<ToneMatchPanel> toneMatchPanel;
    std::unique_ptr<CharacterPanel> characterPanel;

    /*  Section 19's home for controller setup. It is declared in OptionsPages.h
        and still derives from OptionsPage, which is a Component that holds the
        processor and can be told to refresh - nothing about it is specific to
        the Options overlay. Held by pointer so this header does not have to pull
        in every other Options page. */
    std::unique_ptr<ControllersPage> controllersPage;
    std::unique_ptr<TechniquesPanel> techniquesPanel;   // gui-techniques-updates.md 1
    std::unique_ptr<MidiOutPanel> midiOutPanel;
    std::unique_ptr<NotationPanel> notationPanel;

    std::unique_ptr<LuthierChoice> bridgeType;
    std::unique_ptr<LuthierKnob> whammyPos, whammyDown, whammyUp, whammySprings, transposeLock;

    std::unique_ptr<PedalRack> preRack, postRack;

    std::unique_ptr<LuthierKnob> humTiming, humVelocity, humDetune, humAttack, humNoise, humStrum;
    std::unique_ptr<LuthierKnob> vibratoRate, vibratoDepth, strumSpeed, bendRange, legatoWindow,
                                 chordWindow;
    std::unique_ptr<LuthierChoice> vibratoShape, strumDirection;
    std::unique_ptr<LuthierToggle> mpeToggle;

    // ambiguity-resolutions.md 1.3: the SUSTAIN card's feedback row.
    std::unique_ptr<LuthierKnob> feedbackAmount, feedbackDistance, feedbackAngle,
                                 feedbackFocus, feedbackOctave;
    std::unique_ptr<FeedbackLed> feedbackLed;

    // ambiguity-resolutions.md 2.3: the E-Bow row's own controls.
    std::unique_ptr<LuthierKnob> ebowIntensity;
    std::unique_ptr<LuthierChoice> ebowHarmonic;
    std::unique_ptr<StringMaskSelector> ebowStrings;

    std::unique_ptr<LuthierKnob> masterGain;
    std::unique_ptr<LuthierToggle> limiterOn;
    std::unique_ptr<LuthierChoice> oversampling;

    juce::OwnedArray<juce::Component> ownedControls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdvancedPanel)
};

} // namespace luthier
