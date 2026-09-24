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
#include "TuneSetlistStrip.h"
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
        varyItem,           ///< 3.3 "Vary" (TUNE-HELP-ONBOARDING)
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

    //==========================================================================
    // 3.3, TUNE-HELP-ONBOARDING: "Drag sections to reorder", "Vary", and
    // dragging a tab into the setlist timeline.

    /** The slot (0 .. number of sections) a tab dropped at x lands in. */
    int dropSlotAt (int x) const;
    bool moveSectionTo (int fromIndex, int toSlot);
    int varySection (int sectionIndex);

    /** A tab dragged, while it moves and when it is let go, with the mouse in
        screen coordinates; returns true when the panel took the drop (the
        setlist), so the strip does not also reorder. */
    std::function<void (int sectionIndex, juce::Point<int> screen)> onTabDragged;
    std::function<bool (int sectionIndex, juce::Point<int> screen)> onTabDropped;

    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    static juce::Colour colourForRole (SectionRole role);
    static juce::String getRoleName (SectionRole role);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void promptRename (int sectionIndex);
    void promptRepeatCount (int sectionIndex);

    TuneSession& session;
    TunePlayhead playhead;

    int dragTab = -1, dropSlot = -1;
    bool draggingTab = false;
    int varySeed = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneSectionStrip)
};

//==============================================================================
/** The chord pills (3.2): one per cell of the selected section, as wide as
    it lasts, coloured by its function in the key, with the beat marker when
    the section is playing (gui-engine-dataflow 24). */
class TuneChordPills : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    /** 3.2's right-click menu. */
    enum MenuItem
    {
        editItem = 1, insertBeforeItem, insertAfterItem, duplicateItem, deleteItem, copyItem, pasteItem,
        substitutionBase = 100   ///< + the substitution's index in suggestSubstitutions
    };

    explicit TuneChordPills (TuneSession& session);

    /** The Palette role a degree is shown in; non-diatonic chords are neutral. */
    static juce::Colour colourForDegree (int degree);

    void setPlayhead (const TunePlayhead& playhead);

    /** The strum patterns the popover offers as overrides (the panel supplies them). */
    std::function<juce::StringArray()> getPatternNames;

    //==========================================================================
    // 3.2, TUNE-HELP-ONBOARDING: what the mouse does, callable from tests.

    /** Where a cell's pill is drawn, and the cell under a point (-1 for none). */
    juce::Rectangle<int> getCellBounds (int cellIndex) const;
    int getCellAt (juce::Point<int> position) const;
    bool isOnRightEdge (int cellIndex, juce::Point<int> position) const;

    juce::PopupMenu buildMenu (int cellIndex) const;
    void performMenuItem (int cellIndex, int itemId);

    /** "Drag a cell's right edge to change duration in beats": the beats the
        edge at `x` stands for, snapped to half a beat, at least half a beat. */
    double beatsForEdgeAt (int cellIndex, int x) const;
    bool resizeCell (int cellIndex, double beats);

    /** "Drag a cell left / right to reorder": the slot a drop at `x` lands in. */
    int dropIndexAt (int x) const;
    bool moveCell (int fromIndex, int toIndex);

    /** Click: the popover (TuneChordEditor). */
    void openEditor (int cellIndex);

    static bool hasClipboard() noexcept;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    double sectionBeats() const;

    TuneSession& session;
    TunePlayhead playhead;

    enum class Drag { none, pending, resize, move };
    Drag drag = Drag::none;
    int dragCell = -1;
    int dropIndex = -1;
    double dragBeats = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneChordPills)
};

//==============================================================================
/** The piano roll (3.4) for the selected section: the melody, or - tune-builder
    6 and 7's "same piano-roll editor" - the manual bass line or the
    countermelody layer. Implemented in TunePianoRoll.cpp (TUNE-HELP-ONBOARDING
    added the note menu, multi-select, clipboard, nudge and the targets).

    Mouse: click-drag on empty space draws a note (Draw on); Shift-drag, or any
    drag with Draw off, draws a selection box; click a note to select it,
    Shift-click to add or remove it; drag a selected note to move the selection;
    right-click a note for velocity, articulation, technique, lock and delete.
    Keys (with the roll focused): arrows nudge (left/right by the grid, up/down
    by a scale step, or a semitone in chromatic; Shift for a bar or an octave),
    Delete removes, Ctrl+X / C / V cut, copy and paste, Ctrl+A selects all,
    Escape deselects. Every edit is a manual one, so it locks the notes it
    touches (0.3). */
class TunePianoRoll : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    enum class Target { melody = 0, bass, countermelody };

    enum MenuItem
    {
        deleteItem = 1, lockItem, unlockItem, cutItem, copyItem, pasteItem, selectAllItem,
        velocityBase = 100,       ///< + velocity (1..127)
        articulationBase = 300,   ///< + NoteArticulation
        techniqueBase = 400       ///< + NoteTechnique
    };

    explicit TunePianoRoll (TuneSession& session);

    void setTarget (Target newTarget);
    Target getTarget() const noexcept             { return target; }

    void setDrawEnabled (bool enabled) noexcept   { drawEnabled = enabled; }
    bool isDrawEnabled() const noexcept           { return drawEnabled; }

    /** 3.4: snap to key by default; 'C' toggles chromatic. */
    void setChromatic (bool chromatic)            { chromaticMode = chromatic; repaint(); }
    bool isChromatic() const noexcept             { return chromaticMode; }

    void setGridBeats (double beats)              { grid = juce::jmax (1.0 / 16.0, beats); repaint(); }
    double getGridBeats() const noexcept          { return grid; }

    /** The notes the roll shows for the selected section and target: stored
        ones, or for a bass not yet manual, the line its mode generates. */
    std::vector<MelodyNote> getShownNotes() const;
    bool isShowingGeneratedBass() const;

    /** Draws a note as a click-drag would: start snapped to the grid, pitch to
        the key unless chromatic, at least one grid step long. Locked, because
        it is a manual edit. */
    bool addNote (double beat, int pitch, double durationBeats);

    /** Deletes the note under (beat, pitch). */
    bool deleteNoteAt (double beat, int pitch);

    /** Locks or unlocks the note under (beat, pitch) (3.4's "unlock"). */
    bool toggleLockAt (double beat, int pitch);

    int findNoteAt (double beat, int pitch) const;

    //==========================================================================
    // Selection, clipboard, nudge (3.4)
    void selectNote (int noteIndex, bool addToSelection);
    void selectInBox (juce::Rectangle<float> box, bool addToSelection);
    void selectAll();
    void clearSelection();
    const std::vector<int>& getSelection() const noexcept { return selection; }

    bool deleteSelected();
    bool copySelected();
    bool cutSelected();
    /** Pastes at `beat` (snapped), or at the paste point when negative: where
        the roll was last clicked, or straight after what was copied. */
    bool paste (double beat = -1.0);
    static bool hasClipboard() noexcept;

    /** Moves the selection by grid steps and scale steps (semitones in chromatic). */
    bool nudgeSelected (int gridSteps, int pitchSteps, bool large);

    bool setSelectedVelocity (int velocity);
    bool setSelectedArticulation (NoteArticulation articulation);
    bool setSelectedTechnique (NoteTechnique technique);
    bool setSelectedLocked (bool locked);

    juce::PopupMenu buildNoteMenu() const;
    void performNoteMenuItem (int itemId);

    //==========================================================================
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
    bool keyPressed (const juce::KeyPress&) override;

private:
    double sectionBeats() const;
    int pitchOf (const MelodyNote& note) const;
    int transposeStep (int pitch, int steps, bool large) const;

    /** One undoable change to the target's stored notes. `change` edits the
        vector; the target's track, manual bass or layer is created as needed.
        `keep` returns, after the change, the notes to leave selected. */
    bool editNotes (TuneEditClass editClass, const juce::String& description,
                    const std::function<bool (std::vector<MelodyNote>&, std::vector<int>& keep)>& change,
                    int groupTarget = -1);

    TuneSession& session;
    TunePlayhead playhead;
    Target target = Target::melody;
    bool drawEnabled = true;
    bool chromaticMode = false;
    double grid = 0.5;

    std::vector<int> selection;
    int selectionSection = -1;
    double pasteBeat = -1.0;

    // What the mouse is doing.
    enum class Drag { none, draw, box, move };
    Drag drag = Drag::none;
    double dragStart = 0.0, dragEnd = 0.0;
    int dragPitch = 60;
    juce::Point<float> boxStart, boxEnd;
    double moveBeats = 0.0;
    int movePitchRows = 0;
    bool boxAdds = false;

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
    TuneSetlistStrip& getSetlistStrip() noexcept     { return setlistStrip; }
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
    TuneSetlistStrip setlistStrip;   // 3.3's timeline, above the tabs
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
