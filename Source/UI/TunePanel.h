#pragma once

/*  The TUNE workspace tab (tune-builder.md 2 and 3, gui-integration.md 4.4).

    The three-minute tune on one surface (3.1), top to bottom:

      HEADER       title, New, Load, Save, Export, tempo, key and mode
      SECTIONS     one tab per section, coloured by role, with the playhead
                   on the one playing; right-click to rename, duplicate,
                   delete, set the repeat count, tag the role, link the rhythm
                   or mark a state boundary (3.3, 3.5, 8)
      PROGRESSION  the shorthand field, parsed as it is typed, with a malformed
                   token underlined where it sits (2.2, 3.2, 15), and the chord
                   pills coloured by diatonic function
      RHYTHM       genre kit, feel, strum, on (3.5)
      MELODY       the piano roll: draw with the mouse, snapped to the grid and
                   the key ('C' toggles chromatic), right-click a note to delete
                   or lock it; Auto, Draw, Record, Improvise (and Freeze), and
                   the quantise grid (3.4, 4)
      TRANSPORT    back, play/pause, forward, loop, count-in, metronome (3.6);
                   Space plays and pauses, Shift+Space plays the current
                   section from its start

    The panel is thin on purpose: the tune, its file and its undo history are
    TuneSession's, and playback is TunePlayer's; the processor owns both, so
    closing the editor stops nothing. Every control's action is a session
    edit, so every one is undoable and every one rebuilds what plays.

    With the panel focused, the tune takes the shortcuts that are tune-shaped:
    Ctrl+S saves the tune and Ctrl+E exports it (tune-builder 2; gui-integration
    17 makes Ctrl+E context-aware), Ctrl+Z / Ctrl+Shift+Z undo and redo the
    tune, Ctrl+T starts a new one.

    Colours are the Palette's roles only, so the accessibility palettes
    (accessibility.md 6) reach the pills and the piano roll too. The playhead
    is drained at 30 Hz (gui-engine-dataflow 24).
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Widgets.h"
#include "FirstEncounterHint.h"
#include "../Tune/TuneSession.h"

#include <cmath>

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** Where the playhead is, in the tune's own terms. */
struct TunePlayhead
{
    int section = -1;        ///< Index into the tune's sections, or -1 when stopped.
    double beat = 0.0;       ///< Beats into that section.

    bool operator== (const TunePlayhead& o) const noexcept { return section == o.section && std::abs (beat - o.beat) < 1.0e-3; }
    bool operator!= (const TunePlayhead& o) const noexcept { return ! (*this == o); }
};

//==============================================================================
/** The section tabs (3.3). */
class TuneSectionStrip : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    enum MenuItem
    {
        renameItem = 1, duplicateItem, deleteItem, addItem, customRepeatItem, stateBoundaryItem, unlinkItem,
        repeatBase = 100,   ///< + 1, 2, 3, 4, 8
        roleBase = 200,     ///< + SectionRole
        linkBase = 300      ///< + the section to link the rhythm to
    };

    explicit TuneSectionStrip (TuneSession& session);

    /** The right-click menu for a section, and what each item does. The
        panel's tests call performMenuItem directly: a popup menu cannot be
        clicked in the console test runner. */
    juce::PopupMenu buildMenu (int sectionIndex) const;
    void performMenuItem (int sectionIndex, int itemId);

    bool renameSection (int sectionIndex, const juce::String& newName);
    bool setRepeatCount (int sectionIndex, int repeats);

    /** Where a section's tab is drawn; the last slot is the "+" tab. */
    juce::Rectangle<int> getTabBounds (int index) const;
    int getTabAt (juce::Point<int> position) const;

    void setPlayhead (const TunePlayhead& playhead);

    static juce::Colour colourForRole (SectionRole role);
    static juce::String getRoleName (SectionRole role);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void promptRename (int sectionIndex);
    void promptRepeatCount (int sectionIndex);

    TuneSession& session;
    TunePlayhead playhead;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneSectionStrip)
};

//==============================================================================
/** The chord pills (3.2): one per cell of the selected section, as wide as
    it lasts, coloured by its function in the key, with the beat marker when
    the section is playing (gui-engine-dataflow 24). */
class TuneChordPills : public juce::Component
{
public:
    explicit TuneChordPills (TuneSession& session);

    /** The Palette role a degree is shown in; non-diatonic chords are neutral. */
    static juce::Colour colourForDegree (int degree);

    void setPlayhead (const TunePlayhead& playhead);

    void paint (juce::Graphics&) override;

private:
    TuneSession& session;
    TunePlayhead playhead;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneChordPills)
};

//==============================================================================
/** The melody piano roll (3.4) for the selected section. */
class TunePianoRoll : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    explicit TunePianoRoll (TuneSession& session);

    void setDrawEnabled (bool enabled) noexcept   { drawEnabled = enabled; }
    bool isDrawEnabled() const noexcept           { return drawEnabled; }

    /** 3.4: snap to key by default; 'C' toggles chromatic. */
    void setChromatic (bool chromatic)            { chromaticMode = chromatic; repaint(); }
    bool isChromatic() const noexcept             { return chromaticMode; }

    void setGridBeats (double beats)              { grid = juce::jmax (1.0 / 16.0, beats); repaint(); }
    double getGridBeats() const noexcept          { return grid; }

    /** Draws a note as a click-drag would: start snapped to the grid, pitch to
        the key unless chromatic, at least one grid step long. Locked, because
        it is a manual edit. */
    bool addNote (double beat, int pitch, double durationBeats);

    /** Deletes the note under (beat, pitch). */
    bool deleteNoteAt (double beat, int pitch);

    /** Locks or unlocks the note under (beat, pitch) (3.4's "unlock"). */
    bool toggleLockAt (double beat, int pitch);

    int findNoteAt (double beat, int pitch) const;

    /** The pitch and beat a point in the roll stands for, and back. */
    double beatAt (float x) const;
    int pitchAt (float y) const;
    juce::Rectangle<float> getNoteBounds (double beat, int pitch, double durationBeats) const;

    int getLowestPitch() const;
    int getHighestPitch() const;

    void setPlayhead (const TunePlayhead& playhead);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    double sectionBeats() const;

    TuneSession& session;
    TunePlayhead playhead;
    bool drawEnabled = true;
    bool chromaticMode = false;
    double grid = 0.5;

    // A note being drawn.
    bool dragging = false;
    double dragStart = 0.0, dragEnd = 0.0;
    int dragPitch = 60;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunePianoRoll)
};

//==============================================================================
class TunePanel : public juce::Component,
                  private juce::Timer
{
public:
    TunePanel (LuthierAudioProcessor& processor, TunePlayer& player, TuneSession& session);
    ~TunePanel() override;

    int getPreferredHeight() const;

    /** Re-reads the session into every control. */
    void refresh();

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    //==========================================================================
    // What the buttons do, without their file choosers (tests, and the shortcuts).
    bool saveTo (const juce::File& file, juce::String& error);
    bool loadFrom (const juce::File& file, juce::String& error);
    bool exportMidiTo (const juce::File& file, juce::String& error);
    void newFromTemplate (int templateIndex);

    /** onboarding 6 / 10 path C: opens a shipped example tune as a new, unsaved
        tune (so Save asks where, and the factory file is never overwritten). */
    bool openExample (int exampleIndex);

    /** The progression field's parse result. */
    juce::String getProgressionError() const   { return progressionError; }
    int getProgressionErrorPosition() const    { return progressionErrorPosition; }

    /** Applies the progression field's text now (its onTextChange does this). */
    void progressionTextChanged();

    /** The 30 Hz drain, callable from tests. */
    void updateTransport();

    /** onboarding.md 8 (TUNE-HELP-ONBOARDING): the one-time hint at the top of
        the tab, in the first session only. The timer calls this while the tab
        is on screen; the tab grows by the hint's height while it shows. */
    bool showFirstEncounterHintIfDue();
    FirstEncounterHint& getFirstEncounterHint() noexcept { return firstHint; }

    //==========================================================================
    // For tests: the controls, by what they do.
    TuneSession& getSession() noexcept               { return session; }
    juce::TextEditor& getTitleEditor() noexcept      { return titleEditor; }
    juce::TextButton& getNewButton() noexcept        { return newButton; }
    juce::TextButton& getLoadButton() noexcept       { return loadButton; }
    juce::TextButton& getSaveButton() noexcept       { return saveButton; }
    juce::TextButton& getExportButton() noexcept     { return exportButton; }
    juce::Slider& getTempoSlider() noexcept          { return tempoSlider; }
    juce::ComboBox& getKeyBox() noexcept             { return keyBox; }
    juce::ComboBox& getModeBox() noexcept            { return modeBox; }
    TuneSectionStrip& getSectionStrip() noexcept     { return sectionStrip; }
    juce::TextEditor& getProgressionEditor() noexcept { return progressionEditor; }
    TuneChordPills& getChordPills() noexcept         { return chordPills; }
    juce::ComboBox& getKitBox() noexcept             { return kitBox; }
    juce::Slider& getFeelSlider() noexcept           { return feelSlider; }
    juce::Slider& getStrumSlider() noexcept          { return strumSlider; }
    juce::TextButton& getRhythmOnButton() noexcept   { return rhythmOn.getButton(); }
    TunePianoRoll& getPianoRoll() noexcept           { return pianoRoll; }
    juce::TextButton& getAutoButton() noexcept       { return autoButton; }
    juce::TextButton& getDrawButton() noexcept       { return drawToggle.getButton(); }
    juce::TextButton& getRecordButton() noexcept     { return recordToggle.getButton(); }
    juce::TextButton& getImproviseButton() noexcept  { return improviseToggle.getButton(); }
    juce::TextButton& getFreezeButton() noexcept     { return freezeButton; }
    juce::ComboBox& getQuantiseBox() noexcept        { return quantiseBox; }
    juce::TextButton& getBackButton() noexcept       { return backButton; }
    juce::TextButton& getPlayButton() noexcept       { return playButton; }
    juce::TextButton& getForwardButton() noexcept    { return forwardButton; }
    juce::TextButton& getLoopButton() noexcept       { return loopToggle.getButton(); }
    juce::TextButton& getCountInButton() noexcept    { return countInToggle.getButton(); }
    juce::TextButton& getMetronomeButton() noexcept  { return metronomeToggle.getButton(); }
    juce::String getPositionText() const             { return positionText; }
    TunePlayhead getPlayhead() const noexcept        { return playhead; }

private:
    void timerCallback() override;

    void buildHeader();
    void buildProgression();
    void buildRhythm();
    void buildMelody();
    void buildTransport();

    void editSection (TuneEditClass editClass, const juce::String& description,
                      const std::function<bool (TuneSection&)>& change, int targetBase);

    void chooseAndLoad();
    void chooseAndSave();
    void chooseAndExport();
    void showTemplateMenu();
    void showError (const juce::String& title, const juce::String& message);

    LuthierAudioProcessor& processor;
    TunePlayer& player;
    TuneSession& session;

    bool updating = false;
    TunePlayhead playhead;

    FirstEncounterHint firstHint { FirstEncounterHint::kTuneKey, FirstEncounterHint::kTuneText };

    // --- header -------------------------------------------------------------------
    juce::TextEditor titleEditor;
    juce::TextButton newButton { "NEW" }, loadButton { "LOAD" }, saveButton { "SAVE" }, exportButton { "EXPORT" };
    juce::Slider tempoSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxLeft };
    juce::ComboBox keyBox, modeBox;

    // --- sections and progression ----------------------------------------------------
    TuneSectionStrip sectionStrip;
    juce::TextEditor progressionEditor;
    TuneChordPills chordPills;
    juce::String progressionError;
    int progressionErrorPosition = -1;
    int progressionErrorLength = 0;

    // --- rhythm -----------------------------------------------------------------------
    juce::ComboBox kitBox;
    juce::Slider feelSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Slider strumSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    LuthierToggle rhythmOn { "ON" };

    // --- melody -----------------------------------------------------------------------
    TunePianoRoll pianoRoll;
    juce::TextButton autoButton { "AUTO" }, freezeButton { "FREEZE" };
    LuthierToggle drawToggle { "DRAW" }, recordToggle { "RECORD" }, improviseToggle { "IMPROVISE" };
    juce::ComboBox quantiseBox;

    // --- transport ---------------------------------------------------------------------
    juce::TextButton backButton { "<<" }, playButton { "PLAY" }, forwardButton { ">>" };
    LuthierToggle loopToggle { "LOOP" }, countInToggle { "COUNT-IN" }, metronomeToggle { "CLICK" };
    juce::String positionText;

    // Laid out in resized(), drawn in paint().
    juce::Rectangle<int> titleLabelBounds, sectionsHeader, progressionHeader, rhythmHeader, melodyHeader, transportHeader;
    juce::Rectangle<int> positionBounds, feelLabelBounds, strumLabelBounds, tempoLabelBounds, quantiseLabelBounds;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunePanel)
};

} // namespace luthier
