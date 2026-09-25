#pragma once

/*  The TUNE workspace tab (tune-builder.md 2 and 3, gui-integration.md 4.4).

    The three-minute tune on one surface (3.1), top to bottom:

      HEADER       title, New, Load, Save, Export, tempo, key and mode
      SECTIONS     one tab per section, coloured by role, with the playhead
                   on the one playing; drag a tab to reorder; right-click to
                   rename, duplicate, delete, vary, set the repeat count, tag
                   the role, link the rhythm or mark a state boundary (3.3,
                   3.5, 8)
      PROGRESSION  the shorthand field, parsed as it is typed, with a malformed
                   token underlined where it sits (2.2, 3.2, 15), and the chord
                   pills coloured by diatonic function: click one to edit it in
                   a popover, drag it to reorder, drag its right edge to change
                   its length, right-click for insert / duplicate / delete /
                   lock / copy / paste / substitutions (3.2)
      RHYTHM       genre kit, feel, strum, on (3.5); bass mode and the four
                   layers (6, 7)
      MELODY       the piano roll with its keyboard column: MELODY, BASS or
                   LAYER part (6, 7), draw with the mouse, snapped to the grid
                   and the key ('C' toggles chromatic), drag-box and Shift-click
                   to select, Ctrl+C / X / V, arrow keys to nudge (Shift for an
                   octave or a bar), right-click a note for velocity,
                   articulation, technique, lock and delete; scroll and zoom
                   along the bars; Auto, Draw, Record, Improvise (and Freeze),
                   and the quantise grid (3.4, 4)
      TRANSPORT    back, play/pause, stop, forward, loop, count-in, metronome
                   (3.6); Space plays and pauses, Shift+Space plays the current
                   section from its start

    The panel is thin on purpose: the tune, its file and its undo history are
    TuneSession's, and playback is TunePlayer's; the processor owns both, so
    closing the editor stops nothing. Every control's action is a session
    edit, so every one is undoable and every one rebuilds what plays.

    With the panel focused, the tune takes the shortcuts that are tune-shaped:
    Ctrl+S saves the tune and Ctrl+E opens the one-screen export dialog
    (tune-builder 2, 9; gui-integration 17 makes Ctrl+E context-aware), Ctrl+Z /
    Ctrl+Shift+Z undo and redo the tune, Ctrl+T starts a new one.

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
#include <optional>
#include <set>

namespace luthier
{

class LuthierAudioProcessor;
class OverlayPanel;
class TuneExportPanel;

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
        renameItem = 1, duplicateItem, deleteItem, addItem, customRepeatItem, stateBoundaryItem, unlinkItem, varyItem,
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

    /** 3.3 "Drag sections to reorder": what a drop does. The setlist, when
        there is one, follows the new order so the drag is heard, not just
        seen. */
    bool reorder (int fromIndex, int toIndex);

    /** 3.3 "Vary": a subtle variation as a new sibling (TuneMelody's
        varySection). Returns the new section's index, or -1. */
    int vary (int sectionIndex);

    /** Where a section's tab is drawn; the last slot is the "+" tab. */
    juce::Rectangle<int> getTabBounds (int index) const;
    int getTabAt (juce::Point<int> position) const;

    /** The tab being dragged and where it would land, or -1 when none is. */
    int getDraggedTab() const noexcept  { return dragging ? dragTab : -1; }
    int getDropTarget() const noexcept  { return dragging ? dropTarget : -1; }

    void setPlayhead (const TunePlayhead& playhead);

    static juce::Colour colourForRole (SectionRole role);
    static juce::String getRoleName (SectionRole role);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void promptRename (int sectionIndex);
    void promptRepeatCount (int sectionIndex);

    TuneSession& session;
    TunePlayhead playhead;

    int dragTab = -1;
    int dropTarget = -1;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneSectionStrip)
};

//==============================================================================
/** The chord popover (3.2 "Click a cell to open a popover: root, quality,
    bass, extensions, duration, strum override, emphasis"). Every change is a
    `tune-chord-edit` on the cell, grouped within 200 ms as the spec asks. */
class TuneChordEditor : public juce::Component
{
public:
    TuneChordEditor (TuneSession& session, int sectionIndex, int cellIndex);

    /** Re-reads the cell into the controls. */
    void refresh();

    /** The canonical qualities the popover offers, all of them chords the
        rhythm engine can detect (TuneTheory). */
    static juce::StringArray getQualityChoices();

    juce::ComboBox& getRootBox() noexcept          { return rootBox; }
    juce::ComboBox& getQualityBox() noexcept       { return qualityBox; }
    juce::ComboBox& getBassBox() noexcept          { return bassBox; }
    juce::ComboBox& getEmphasisBox() noexcept      { return emphasisBox; }
    juce::Slider& getBeatsSlider() noexcept        { return beatsSlider; }
    juce::TextEditor& getExtensionsEditor() noexcept { return extensionsEditor; }
    juce::ToggleButton& getFillToggle() noexcept   { return fillToggle; }
    juce::ToggleButton& getLockToggle() noexcept   { return lockToggle; }

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kWidth = 300;
    static constexpr int kHeight = 214;

private:
    void apply (const juce::String& description, const std::function<bool (ChordCell&)>& change);
    const ChordCell* cell() const;

    TuneSession& session;
    int sectionIndex, cellIndex;
    bool updating = false;

    juce::ComboBox rootBox, qualityBox, bassBox, emphasisBox;
    juce::Slider beatsSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextEditor extensionsEditor;
    juce::ToggleButton fillToggle, lockToggle;
    juce::Rectangle<int> rootLabel, qualityLabel, bassLabel, beatsLabel, extensionsLabel, emphasisLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneChordEditor)
};

//==============================================================================
/** The chord pills (3.2): one per cell of the selected section, as wide as
    it lasts, coloured by its function in the key, with the beat marker when
    the section is playing (gui-engine-dataflow 24). Click to edit, drag to
    reorder, drag the right edge for the duration, right-click for the menu. */
class TuneChordPills : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    enum MenuItem
    {
        editItem = 1, insertBeforeItem, insertAfterItem, duplicateItem, deleteItem, lockItem, copyItem, pasteItem,
        substitutionBase = 500,   ///< + index into suggestSubstitutions
        suggestBase = 600         ///< + index into suggestNextChords (appended after the cell)
    };

    explicit TuneChordPills (TuneSession& session);

    /** The Palette role a degree is shown in; non-diatonic chords are neutral. */
    static juce::Colour colourForDegree (int degree);

    /** The cell whose pill is under a point, or -1. A short progression
        repeats to fill the section, so a cell may have several pills. */
    int getCellAt (juce::Point<int> position) const;

    /** The first pill of a cell, or an empty rectangle. */
    juce::Rectangle<float> getPillBounds (int cellIndex) const;

    /** True over the last few pixels of a pill, where a drag changes its
        duration (3.2 "Drag a cell's right edge to change duration"). */
    bool isOnRightEdge (juce::Point<int> position) const;

    /** The right-click menu and what its items do. */
    juce::PopupMenu buildMenu (int cellIndex) const;
    void performMenuItem (int cellIndex, int itemId);

    /** What a drag does when dropped: `tune-chord-edit`. */
    bool reorder (int fromIndex, int toIndex);
    bool setDuration (int cellIndex, double beats);

    /** Opens the popover on a cell (a click does this). */
    void openEditor (int cellIndex);

    /** The Copy / Paste clipboard, shared by every pill strip. */
    static const std::optional<ChordCell>& getClipboard() noexcept { return clipboard; }

    int getDraggedCell() const noexcept { return dragMode == DragMode::reorder ? dragCell : -1; }
    int getDropTarget() const noexcept  { return dragMode == DragMode::reorder ? dropTarget : -1; }

    void setPlayhead (const TunePlayhead& playhead);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    enum class DragMode { none, pending, reorder, duration };

    struct Pill
    {
        int cellIndex = 0;
        juce::Rectangle<float> bounds;
    };

    std::vector<Pill> layoutPills() const;
    double beatsPerPixel() const;
    ChordCell newCellLike (const ChordCell& cell) const;

    TuneSession& session;
    TunePlayhead playhead;

    DragMode dragMode = DragMode::none;
    int dragCell = -1;
    int dropTarget = -1;
    double dragBeats = 0.0;       ///< The duration being dragged to.
    juce::Point<int> dragOrigin;

    static std::optional<ChordCell> clipboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneChordPills)
};

//==============================================================================
/** The piano roll (3.4) for the selected section: the melody, the bass line
    (6, "Manual: same piano-roll editor as melody") or the countermelody
    layer (7), one part at a time. A keyboard column on the left, the bars
    scrolling and zooming to the right of it. */
class TunePianoRoll : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    enum NoteMenuItem
    {
        deleteItem = 1, lockItem, selectAllItem, pasteItem,
        velocityBase = 100,       ///< + index into kVelocityPresets
        articulationBase = 200,   ///< + NoteArticulation
        techniqueBase = 300       ///< + NoteTechnique
    };

    static constexpr int kKeyboardWidth = 30;
    static constexpr int kNumVelocityPresets = 5;
    static const int kVelocityPresets[kNumVelocityPresets];   ///< 40, 64, 80, 100, 127

    explicit TunePianoRoll (TuneSession& session);

    //==========================================================================
    // Which notes

    /** 6 and 7: the part the roll edits. */
    void setPart (TuneNotePart part);
    TuneNotePart getPart() const noexcept         { return part; }

    /** The part's stored notes, or null when the section has none of that
        part yet (TuneModel's getPartNotes). */
    const std::vector<MelodyNote>* getNotes() const;

    /** What the roll shows: the stored notes, or, for a bass in a derived
        mode, the line that mode generates (which a first edit turns into a
        Manual line). */
    std::vector<MelodyNote> getShownNotes() const;

    /** True while a derived bass line is shown (grey, editable). */
    bool isShowingDerivedBass() const;

    //==========================================================================
    // Draw

    void setDrawEnabled (bool enabled) noexcept   { drawEnabled = enabled; }
    bool isDrawEnabled() const noexcept           { return drawEnabled; }

    /** 3.4: snap to key by default; 'C' toggles chromatic. */
    void setChromatic (bool chromatic)            { chromaticMode = chromatic; repaint(); }
    bool isChromatic() const noexcept             { return chromaticMode; }

    void setGridBeats (double beats)              { grid = juce::jmax (1.0 / 16.0, beats); repaint(); }
    double getGridBeats() const noexcept          { return grid; }

    /** Draws a note as a click-drag would: start snapped to the grid, pitch to
        the key unless chromatic, at least one grid step long. Locked, because
        it is a manual edit. Returns true when a note was added; it becomes
        the selection. */
    bool addNote (double beat, int pitch, double durationBeats);

    /** Deletes the note under (beat, pitch). */
    bool deleteNoteAt (double beat, int pitch);

    /** Locks or unlocks the note under (beat, pitch) (3.4's "unlock"). */
    bool toggleLockAt (double beat, int pitch);

    int findNoteAt (double beat, int pitch) const;

    //==========================================================================
    // Selection (3.4 "Multi-select with drag-box; shift-click adds")

    const std::set<int>& getSelection() const noexcept { return selection; }
    void select (int noteIndex, bool addToSelection);
    void selectAll();
    void clearSelection();

    /** Selects every note starting in the box (beats and pitches inclusive). */
    void selectInBox (double fromBeat, double toBeat, int lowPitch, int highPitch, bool addToSelection);

    bool deleteSelected();
    bool setSelectedLocked (bool locked);

    /** 3.4 "Nudge with arrow keys": `deltaSteps` is semitones when chromatic,
        scale steps otherwise; `deltaBeats` is in beats. Nudged notes lock. */
    bool nudgeSelected (double deltaBeats, int deltaSteps);

    /** 3.4 "velocity": the selected notes' velocity, or a change to it. */
    bool setSelectedVelocity (int velocity);
    bool adjustSelectedVelocity (int delta);

    bool setSelectedArticulation (NoteArticulation articulation);
    bool setSelectedTechnique (NoteTechnique technique);

    //==========================================================================
    // Clipboard (3.4 "Cut / copy / paste (standard shortcuts)")

    bool copySelected();
    bool cutSelected();

    /** Pastes at the cursor (where the roll was last clicked, snapped to the
        grid, or the section's start), keeping the copied notes' spacing. The
        pasted notes become the selection. */
    bool paste();

    void setCursorBeat (double beat)              { cursorBeat = juce::jmax (0.0, beat); }
    double getCursorBeat() const noexcept         { return cursorBeat; }

    static const std::vector<MelodyNote>& getClipboard() noexcept { return clipboard; }

    //==========================================================================
    // The note menu (3.4 "Right-click a note: velocity, articulation,
    // technique, unlock, delete")

    juce::PopupMenu buildNoteMenu (int noteIndex) const;
    void performNoteMenuItem (int noteIndex, int itemId);

    //==========================================================================
    // View: "1-4 bars visible, scrollable" (3.1)

    /** The visible window, in section beats. */
    void setView (double startBeat, double visibleBeats);
    double getViewStartBeat() const noexcept      { return viewStart; }
    double getVisibleBeats() const noexcept       { return visibleBeats; }

    /** Zooms about the view's centre: factor > 1 shows fewer beats. */
    void zoom (double factor);
    void scrollBy (double beats);

    /** Scrolls so that `beat` is visible. */
    void showBeat (double beat);

    //==========================================================================
    /** The pitch and beat a point in the roll stands for, and back. Beats
        are section beats; x is to the right of the keyboard column. */
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
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    enum class DragMode { none, draw, box, move };

    double sectionBeats() const;
    juce::Rectangle<float> rollArea() const;

    /** Every edit: the part's notes, changed, written back with setPartNotes
        as one undo entry. `target` groups (a note index) or -1 never groups. */
    bool editNotes (const juce::String& description,
                    const std::function<bool (std::vector<MelodyNote>&)>& change, int target = -1);

    int stepPitch (int pitch, int steps) const;
    void clampSelection();

    TuneSession& session;
    TunePlayhead playhead;
    TuneNotePart part = TuneNotePart::melody;
    bool drawEnabled = true;
    bool chromaticMode = false;
    double grid = 0.5;

    double viewStart = 0.0;
    double visibleBeats = 16.0;

    std::set<int> selection;
    double cursorBeat = 0.0;

    DragMode dragMode = DragMode::none;
    double dragStart = 0.0, dragEnd = 0.0;     // draw: the note; box: beats
    int dragPitch = 60, dragPitchEnd = 60;     // draw: the pitch; box: pitches
    double moveBeats = 0.0;                    // move: the offset being dragged
    int moveSteps = 0;
    juce::Point<int> dragOrigin;

    static std::vector<MelodyNote> clipboard;

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

    /** midi-export 5: a `.mid` as a new tune (TuneImport). Stops the player,
        replaces the tune as a state boundary, and reports what the importer
        guessed through onNotification. False, with `error`, leaves the tune. */
    bool importMidiFrom (const juce::File& file, juce::String& error);

    /** The NEW button's template picker; the header's "New Tune..." too. */
    void showTemplateMenu();

    /** 9: the one-screen export dialog (EXPORT, Ctrl+E). Shown through
        onShowOverlay when the editor wires it, else in a window of its own. */
    void showExportDialog();
    TuneExportPanel& getExportPanel();

    /** The editor's overlay host, when wired: only the editor can put an
        overlay on screen (Overlays.h). */
    std::function<void (OverlayPanel*)> onShowOverlay;

    /** Where an import's report goes: the editor's banner strip when wired,
        else a message box. `warning` marks news the user should act on. */
    std::function<void (const juce::String& message, bool warning)> onNotification;

    /** The progression field's parse result. */
    juce::String getProgressionError() const   { return progressionError; }
    int getProgressionErrorPosition() const    { return progressionErrorPosition; }

    /** Applies the progression field's text now (its onTextChange does this). */
    void progressionTextChanged();

    /** The 30 Hz drain, callable from tests. */
    void updateTransport();

    /** The roll's part (6, 7): what the MELODY | BASS | LAYER buttons do. */
    void setPart (TuneNotePart part);
    TuneNotePart getPart() const noexcept { return pianoRoll.getPart(); }

    /*  onboarding.md 8: the one-time hint at the top of the tab, in the first
        session. The timer calls this while the tab is on screen; showing it
        makes room above the header, dismissing it gives the room back. */
    void showFirstEncounterHintIfDue();
    FirstEncounterHint& getFirstEncounterHint() noexcept { return firstEncounterHint; }

    //==========================================================================
    // For tests: the controls, by what they do.
    TuneSession& getSession() noexcept               { return session; }
    juce::TextEditor& getTitleEditor() noexcept      { return titleEditor; }
    juce::TextButton& getNewButton() noexcept        { return newButton; }
    juce::TextButton& getLoadButton() noexcept       { return loadButton; }
    juce::TextButton& getImportButton() noexcept     { return importButton; }
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
    juce::ComboBox& getBassModeBox() noexcept        { return bassModeBox; }
    juce::TextButton& getLayerButton (LayerType type) noexcept { return layerToggles[(size_t) type]->getButton(); }
    TunePianoRoll& getPianoRoll() noexcept           { return pianoRoll; }
    juce::TextButton& getPartButton (TuneNotePart p) noexcept { return *partButtons[(size_t) p]; }
    juce::TextButton& getZoomInButton() noexcept     { return zoomInButton; }
    juce::TextButton& getZoomOutButton() noexcept    { return zoomOutButton; }
    juce::TextButton& getAutoButton() noexcept       { return autoButton; }
    juce::TextButton& getDrawButton() noexcept       { return drawToggle.getButton(); }
    juce::TextButton& getRecordButton() noexcept     { return recordToggle.getButton(); }
    juce::TextButton& getImproviseButton() noexcept  { return improviseToggle.getButton(); }
    juce::TextButton& getFreezeButton() noexcept     { return freezeButton; }
    juce::ComboBox& getQuantiseBox() noexcept        { return quantiseBox; }
    juce::TextButton& getBackButton() noexcept       { return backButton; }
    juce::TextButton& getPlayButton() noexcept       { return playButton; }
    juce::TextButton& getStopButton() noexcept       { return stopButton; }
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
    void chooseAndImportMidi();
    void showError (const juce::String& title, const juce::String& message);
    void notify (const juce::String& message, bool warning);

    LuthierAudioProcessor& processor;
    TunePlayer& player;
    TuneSession& session;

    bool updating = false;
    TunePlayhead playhead;

    FirstEncounterHint firstEncounterHint { FirstEncounterHint::kTuneKey, FirstEncounterHint::kTuneText };

    // --- header -------------------------------------------------------------------
    juce::TextEditor titleEditor;
    juce::TextButton newButton { "NEW" }, loadButton { "LOAD" }, importButton { "IMPORT" },
                     saveButton { "SAVE" }, exportButton { "EXPORT" };
    juce::Slider tempoSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxLeft };
    juce::ComboBox keyBox, modeBox;

    // --- sections and progression ----------------------------------------------------
    TuneSectionStrip sectionStrip;
    juce::TextEditor progressionEditor;
    TuneChordPills chordPills;
    juce::String progressionError;
    int progressionErrorPosition = -1;
    int progressionErrorLength = 0;

    // --- rhythm, bass and layers ---------------------------------------------------------
    juce::ComboBox kitBox;
    juce::Slider feelSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Slider strumSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    LuthierToggle rhythmOn { "ON" };
    juce::ComboBox bassModeBox;
    std::array<std::unique_ptr<LuthierToggle>, (size_t) LayerType::numTypes> layerToggles;

    // --- melody -----------------------------------------------------------------------
    TunePianoRoll pianoRoll;
    std::array<std::unique_ptr<juce::TextButton>, (size_t) TuneNotePart::numParts> partButtons;
    juce::TextButton zoomInButton { "+" }, zoomOutButton { "-" };
    juce::TextButton autoButton { "AUTO" }, freezeButton { "FREEZE" };
    LuthierToggle drawToggle { "DRAW" }, recordToggle { "RECORD" }, improviseToggle { "IMPROVISE" };
    juce::ComboBox quantiseBox;

    // --- transport ---------------------------------------------------------------------
    juce::TextButton backButton { "<<" }, playButton { "PLAY" }, stopButton { "STOP" }, forwardButton { ">>" };
    LuthierToggle loopToggle { "LOOP" }, countInToggle { "COUNT-IN" }, metronomeToggle { "CLICK" };
    juce::String positionText;

    // --- export ----------------------------------------------------------------------
    std::unique_ptr<TuneExportPanel> exportPanel;
    std::unique_ptr<juce::DialogWindow> exportWindow;

    // Laid out in resized(), drawn in paint().
    juce::Rectangle<int> titleLabelBounds, sectionsHeader, progressionHeader, rhythmHeader, melodyHeader, transportHeader;
    juce::Rectangle<int> positionBounds, feelLabelBounds, strumLabelBounds, tempoLabelBounds, quantiseLabelBounds,
                         bassLabelBounds, layersLabelBounds, partLabelBounds, viewLabelBounds;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunePanel)
};

} // namespace luthier
