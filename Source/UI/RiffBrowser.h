#pragma once

/*  The riff library's browser (riff-library.md 7).

    One component, two layouts:
      - full: Column 4's RIFFS tab (7.2) - search, "Fits this instrument",
        "+ Save riff", genre chips, the filter row, a virtualised list, and the
        preview (tab view, transport, key / scale / tempo / level, Drag .mid,
        Add to Tune, Looper, Learn It, Drag as Luthier | Generic), with a
        status line; below 640 points the preview stacks under the list;
      - compact: the Easy mode Riff drawer (7.3) - search, genre, type and a
        difficulty chip row, eight visible rows, and the preview's name,
        Play / Loop, Key and Tempo, then Drag .mid, Looper and Learn It. Add
        to Tune is absent because Easy has no TUNE tab.

    Everything it sets is view or session state (riff-library 0.6, 8): the
    per-instance part is RiffUiState in the processor's uiState (saved with
    the host project, never in a preset); favourites, recents and play counts
    are the library's library.json. No parameter is touched.

    Audition goes through the engine's RiffPlayer; the browser compiles on the
    message thread and hands the result over.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "RiffTabView.h"
#include "AnimationPolicy.h"   // cpu-quality-modes 6 (INTEGRATE-2)
#include "../Riffs/RiffLibrary.h"
#include "../Riffs/RiffPlayer.h"
#include "../Riffs/RiffDestinations.h"
#include "../Practice/PracticeRoutineTempo.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** The browser's per-instance state (riff-library 8). */
struct RiffUiState
{
    juce::String selectedId;
    RiffQuery query;

    int keyChoice = 0;                ///< 0 the riff's own key, 1..12 C..B, 13 the tune's key
    bool mapScale = false;
    juce::String targetScale;         ///< empty keeps the riff's scale
    int clockMode = 0;                ///< RiffPlayer::ClockMode
    int tempoMode = 0;                ///< 0 factor of the riff's tempo, 1 absolute bpm
    double tempoFactor = 1.0;
    double absoluteBpm = 120.0;
    bool loop = true;
    int startQuantise = 0;            ///< RiffPlayer::StartQuantise
    double levelDb = -6.0;
    bool dragGeneric = false;         ///< "Drag as" Generic
    bool drawerOpen = false;
    double previewSplit = 0.5;

    RiffUiState() { query.fitsInstrument = true; }

    juce::var toVar() const;
    static RiffUiState fromVar (const juce::var& v, bool defaultDragGeneric = false);
};

//==============================================================================
class RiffBrowser : public juce::Component,
                    public juce::DragAndDropContainer,
                    private juce::Timer,
                    private juce::ListBoxModel
{
public:
    RiffBrowser (LuthierAudioProcessor& processor, bool compact);
    ~RiffBrowser() override;

    bool isCompact() const noexcept { return compact; }

    /** Loads the library index on the shared worker pool if it is not loaded
        (the first time the tab or the drawer opens). `now` loads on this thread. */
    void ensureLibraryLoaded (bool now = false);

    //==========================================================================
    // What the controls do, for the keyboard, the tests and the drawer.
    void refreshList();
    int getNumListRows() const noexcept { return (int) rows.size(); }
    const RiffIndexEntry* getRowEntry (int row) const;
    void selectRow (int row);
    void selectRiff (const juce::String& id);
    juce::String getSelectedId() const { return state.selectedId; }
    int getSelectedRow() const;

    void audition();
    void stopAudition();
    void toggleAudition();
    bool isAuditioning() const;

    /** The last polite announcement (accessibility 10). */
    juce::String getLastAnnouncement() const { return lastAnnouncement; }

    void clearFilters();
    RiffUiState& getState() noexcept { return state; }
    void stateChanged (bool recompile = true);

    /** The compiled selection at the current settings (what the tab view shows). */
    std::shared_ptr<const CompiledRiff> getPreviewCompiled() const { return previewCompiled; }
    std::shared_ptr<const Riff> getSelectedRiff();

    // Destinations.
    juce::File makeDragFile (bool forceGeneric);
    bool saveMidTo (const juce::File& file, bool generic);
    RiffDestinations::TuneInsert addToTune();
    RiffDestinations::LooperSend sendToLooper();
    bool learnIt();
    juce::Result saveRiffFromCapture (const juce::String& name, const juce::String& type,
                                      const juce::String& genre, const juce::StringArray& tags);
    juce::Result importMidiAsRiff (const juce::File& file);
    juce::Result deleteSelectedUserRiff();   ///< no confirmation: the dialog asks first

    /** 7.4's Save as riff dialog (from "+ Save riff" or NOTATION's capture). */
    void openSaveDialog() { showSaveDialog(); }

    /** The tempo the preview shows and drag-out writes: the host's in Auto
        while it plays, otherwise the riff's own times the factor, or the bpm. */
    double getShownTempo();

    //==========================================================================
    // Controls, for tests.
    juce::TextEditor& getSearchBox() noexcept         { return searchBox; }
    juce::ListBox& getList() noexcept                 { return list; }
    juce::Button& getPlayButton() noexcept            { return playButton; }
    juce::Button& getLoopToggle() noexcept            { return loopToggle; }
    juce::Button& getAddToTuneButton() noexcept       { return addToTuneButton; }
    juce::Button& getLooperButton() noexcept          { return looperButton; }
    juce::Button& getLearnButton() noexcept           { return learnButton; }
    juce::Button& getSaveRiffButton() noexcept        { return saveButton; }
    juce::Button& getClearFiltersButton() noexcept    { return clearFiltersButton; }
    juce::Button& getFitsToggle() noexcept            { return fitsToggle; }
    juce::Button& getDragTile() noexcept              { return dragTile; }
    juce::ComboBox& getKeyBox() noexcept              { return keyBox; }
    juce::ComboBox& getTypeBox() noexcept             { return typeBox; }
    juce::ComboBox& getTempoModeBox() noexcept        { return tempoModeBox; }
    juce::ComboBox& getClockBox() noexcept            { return clockBox; }
    juce::Button* getGenreChip (int genre) noexcept   { return genreChips[genre]; }
    RiffTabView& getTabView() noexcept                { return tabView; }
    juce::String getStatusText() const                { return statusLabel.getText(); }
    juce::String getCountText() const                 { return countLabel.getText(); }
    juce::String getHintText() const                  { return hintLabel.isVisible() ? hintLabel.getText() : juce::String(); }
    juce::String getBannerText() const                { return bannerLabel.isVisible() ? bannerLabel.getText() : juce::String(); }
    juce::Array<juce::Component*> getFocusOrder();

    /** The row's accessible name (accessibility 10). */
    juce::String getRowName (int row) const;

    //==========================================================================
    void resized() override;
    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

    int getPreferredHeight() const;

    std::function<void()> onCloseRequested;   ///< the drawer's Escape

    bool shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails&,
                                               juce::StringArray& files, bool& canMoveFiles) override;

private:
    class DragTile;

    // ListBoxModel
    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int lastRowSelected) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override;
    juce::String getNameForRow (int row) override { return getRowName (row); }
    juce::String getTooltipForRow (int row) override;

    void timerCallback() override;
    bool pollLibrary();   ///< applies a newly loaded index; false while none is loaded

    void buildControls();
    void loadState();
    void saveState();
    RiffPlaySettings playSettings();
    void applyPlayerSettings();
    void recompilePreview (bool toPlayer, bool immediate);
    void updateStatus();
    void announce (const juce::String& text);
    void postBanner (const juce::String& id, const juce::String& message, bool warning);
    void showSaveDialog (std::shared_ptr<const Riff> editing = nullptr);
    void showPlusMenu();
    void updateUserRiffButtons();
    void tickSpeedTrainer();

    LuthierAudioProcessor& processor;
    RiffLibrary& library;
    RiffPlayer& player;
    const bool compact;

    RiffUiState state;
    std::vector<int> rows;
    bool loadingShown = false;
    bool indexSeen = false;
    bool suppressCallbacks = false;

    std::shared_ptr<const CompiledRiff> previewCompiled;
    GuitarSpecSummary lastGuitar;
    bool auditionLatched = false;     ///< this browser started what is playing
    juce::String playingId;
    juce::String lastAnnouncement;

    SpeedTrainer speedTrainer;
    int learnLoopsSeen = 0;
    bool learning = false;

    // ---- the header rows -------------------------------------------------------------
    juce::Label titleLabel { {}, "RIFFS" };
    juce::TextEditor searchBox;
    juce::ToggleButton fitsToggle { "Fits this instrument" };
    juce::TextButton saveButton { "+ Save riff" };
    juce::OwnedArray<juce::TextButton> genreChips;
    juce::ComboBox genreBox;          ///< compact
    juce::ComboBox typeBox, keyFilterBox, sortBox;
    juce::TextButton techButton { "Tech +" }, userChip { "User" };
    juce::Slider difficultySlider { juce::Slider::TwoValueHorizontal, juce::Slider::NoTextBox };
    juce::Slider tempoFilterSlider { juce::Slider::TwoValueHorizontal, juce::Slider::NoTextBox };
    juce::Label difficultyLabel, tempoFilterLabel;
    juce::OwnedArray<juce::TextButton> difficultyChips;   ///< compact
    juce::TextButton favFilter { "* Fav" };

    // ---- list ----------------------------------------------------------------------
    juce::ListBox list { "Riffs", this };

    // ---- preview ---------------------------------------------------------------------
    juce::Label nameLabel, infoLabel;
    juce::TextButton favButton { "*" };
    RiffTabView tabView;
    juce::TextButton playButton { "Play" };
    juce::ToggleButton loopToggle { "Loop" };
    juce::ComboBox startBox, keyBox, scaleBox, clockBox, tempoModeBox;
    juce::ToggleButton mapScaleToggle { "Map scale" };
    juce::Slider tempoSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider levelSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label levelLabel { {}, "Level" };
    std::unique_ptr<DragTile> dragTileOwner;
    juce::Button& dragTile;
    juce::TextButton addToTuneButton { "Add to Tune" }, looperButton { "Looper" }, learnButton { "Learn It" };
    juce::TextButton dragAsLuthier { "Luthier" }, dragAsGeneric { "Generic" };
    juce::Label dragAsLabel { {}, "Drag as" };
    juce::TextButton editInfoButton { "Edit info" }, duplicateButton { "Duplicate" },
                     revealButton { "Reveal" }, deleteButton { "Delete" };

    // ---- status ----------------------------------------------------------------------
    juce::Label countLabel, statusLabel, hintLabel, bannerLabel;
    juce::TextButton clearFiltersButton { "Clear filters" }, bannerRevealButton { "Reveal" };

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_WEAK_REFERENCEABLE (RiffBrowser)
    // cpu-quality-modes 6: the playhead is a live readout; the waiting pulse is decorative.
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "RiffBrowser" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiffBrowser)
};

} // namespace luthier
