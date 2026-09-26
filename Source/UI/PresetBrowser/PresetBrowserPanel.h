#pragma once

/*  preset-browser-previews.md 7: the preset browser.

    Moved here from Overlays.h and rebuilt inside; the editor's
    showOverlay (&presetBrowser), header.onOpenPresetBrowser and the
    presetBrowser shortcut are unchanged. One overlay whose layout follows the
    editor mode: Easy (780 x 560, chip row, "More like this") and Advanced
    (1040 x 680, filter sidebar, sort and columns, SOUNDS LIKE pane).

    It kept three pieces of the old browser: the Morph row, the "Uses
    Techniques" chip (gui-techniques-updates 7) and a guitar thumbnail.

    The model is the processor's PresetLibrary (index, render service,
    player); this class is view and interaction only. Browsing, filtering,
    favourites and previews never touch the undo stack (9); only a load does.
*/

#include "../Overlays.h"
#include "PresetBrowserWidgets.h"
#include "../../Presets/PresetLibrary.h"

namespace luthier
{

class PresetRow;
class PresetFilterSidebar;
class PresetDetailPane;

class PresetBrowserPanel : public OverlayPanel,
                           private juce::ChangeListener,
                           private juce::KeyListener,
                           private juce::Timer
{
public:
    explicit PresetBrowserPanel (LuthierAudioProcessor& processor);
    ~PresetBrowserPanel() override;

    /** 7.1 / 7.2: the size follows the editor mode (the host clamps it to the
        window minus its padding). */
    juce::Point<int> getPreferredSize() const override;

    /** Raised when the user asks to save the current sound as a new preset. */
    std::function<void()> saveAsPanelRequested;

    void overlayShown() override;
    void overlayHidden() override;

    bool keyPressed (const juce::KeyPress&) override;

    /** Keys arriving at the search box or the list (a KeyListener on both). */
    bool keyPressed (const juce::KeyPress&, juce::Component* originator) override;

    void mouseExit (const juce::MouseEvent&) override;

    //==========================================================================
    // The view model, public for the child components and the tests.

    LuthierAudioProcessor& getProcessor() noexcept { return processor; }
    PresetLibrary& getLibrary();

    bool isAdvancedLayout() const;

    /** Index entries in display order. */
    const std::vector<int>& getResults() const noexcept { return results; }

    int getSelectedEntry() const;
    void selectEntry (int entry, bool scrollTo = true);

    /** FEAT-SEARCH (global-search.md "open browser at entry"): selects a preset
        by name, clearing anything that would hide it. */
    bool selectPresetNamed (const juce::String& name);

    juce::String getQuery() const;
    void setQuery (const juce::String&);

    PresetSearch::Filters& getFilters() noexcept { return filters; }
    void filtersChanged();          ///< refilters and remembers the filters (7.2)
    void clearSearchAndFilters();

    PresetSearch::Sort getSort() const noexcept { return sort; }
    void setSort (PresetSearch::Sort);

    void refilter();

    //==========================================================================
    // Previews (4.4).

    /** Glyph click and Space: explicit. Hover and selection: not. */
    void startPreview (int entry, bool explicitTrigger);
    void togglePreview (int entry);
    void stopPreview();
    bool isPreviewing (int entry) const;

    /** Row hover (4.4's 300 ms dwell). */
    void rowHovered (int entry);
    void rowUnhovered (int entry);

    /** The glyph state an entry shows (7.3). */
    PlayGlyph::State glyphStateFor (int entry, float& progress, juce::String& failure) const;

    //==========================================================================
    // Library and loading.

    void loadEntry (int entry);
    void setFavourite (int entry, bool favourite);
    void setRating (int entry, int stars);

    /** 7.1 "More like this" / 7.2 SOUNDS LIKE, with its breadcrumb. */
    void showSoundsLike (int entry);
    void clearSoundsLike();
    bool isShowingSoundsLike() const noexcept { return similarTo.isNotEmpty(); }
    std::vector<int> getSoundsLike (int entry) const;

    /** Adds a chip's word to the search (7.3). */
    void addToSearch (const juce::String& word);

    //==========================================================================
    // Text the tests read.

    juce::String getFooterText() const;
    juce::String getEmptyText() const;
    juce::String getBreadcrumbText() const;
    juce::String describeRow (int entry) const;    ///< 10's row name
    bool isShowingCacheBanner() const noexcept { return cacheBannerShown; }
    juce::String getLastAnnouncement() const { return lastAnnouncement; }

    /** Children, for the tests. */
    juce::TextEditor& getSearchBox() noexcept { return searchBox; }
    juce::ListBox& getList() noexcept { return list; }
    PresetDetailPane& getDetailPane() noexcept { return *detail; }
    PresetFilterSidebar& getSidebar() noexcept { return *sidebar; }
    juce::Component& getChipRow() noexcept { return chipRow; }
    juce::Button& getMoreLikeThisButton() noexcept;
    juce::ComboBox& getSortBox() noexcept { return sortBox; }
    juce::Button& getSpeakerButton() noexcept { return speakerButton; }
    juce::Slider& getVolumeSlider() noexcept { return volumeSlider; }
    juce::Button& getClearFiltersButton() noexcept { return clearFiltersButton; }
    PresetRow* getRowComponentFor (int entry) const;

    /** Reads the presetPreview.* keys into the library and the footer (8). */
    void syncPreviewSettings();

    /** The Easy chip row's chips (tests click them). */
    ChipButton* findChip (const juce::String& text) const;

protected:
    void layoutContent (juce::Rectangle<int> content) override;

private:
    friend class PresetRow;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void rebuildChipRow();
    void updateFooter();
    void updateEmptyState();
    void refreshRowsAndDetail();
    void refreshMorph();
    void announce (const juce::String& text, bool alwaysAnnounce);
    void rememberView();
    void restoreView();
    bool handleBrowserKey (const juce::KeyPress&);

    LuthierAudioProcessor& processor;

    // ---- search row and chips ----------------------------------------------------
    juce::TextEditor searchBox;
    juce::ComboBox sortBox;
    juce::Label sortLabel;
    juce::Component chipRow;
    juce::OwnedArray<ChipButton> chips;
    juce::TextButton breadcrumbClear { "x" };
    juce::Label breadcrumb;

    // ---- list, sidebar, detail ---------------------------------------------------
    class ListModel : public juce::ListBoxModel
    {
    public:
        explicit ListModel (PresetBrowserPanel& o) : owner (o) {}
        int getNumRows() override;
        void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
        juce::Component* refreshComponentForRow (int row, bool selected, juce::Component* existing) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
        void selectedRowsChanged (int lastRow) override;
        void returnKeyPressed (int row) override;
        juce::String getNameForRow (int row) override;

    private:
        PresetBrowserPanel& owner;
    };

    ListModel listModel { *this };
    juce::ListBox list;
    juce::Label emptyLabel;
    juce::TextButton emptyClearButton { "Clear filters" };
    juce::Label columnHeader;

    std::unique_ptr<PresetFilterSidebar> sidebar;
    std::unique_ptr<juce::Viewport> sidebarViewport;
    std::unique_ptr<PresetDetailPane> detail;

    // ---- footer --------------------------------------------------------------------
    juce::TextButton speakerButton { "Previews" };
    juce::Slider volumeSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Label footerLabel;
    juce::TextButton clearFiltersButton { "Clear filters" };

    juce::TextButton loadButton { "Load" };
    juce::TextButton deleteButton { "Delete" };
    juce::TextButton saveAsButton { "Save As..." };

    // ambiguity-resolutions.md 5.2: Morph, its two slots and the slider.
    juce::TextButton morphToggle { "Morph" };
    juce::TextButton slotAButton { "A" }, slotBButton { "B" };
    juce::Slider morphSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttachment;

    // ---- state -----------------------------------------------------------------------
    std::vector<int> results;
    PresetSearch::Filters filters;
    PresetSearch::Sort sort = PresetSearch::Sort::relevance;
    juce::String similarTo;              ///< the key "Sounds like" is showing, or empty
    std::vector<int> similarResults;

    int hoverEntry = -1;
    double hoverSince = 0.0;
    bool hoverStartedPreview = false;
    int selectDwellEntry = -1;
    double selectDwellSince = 0.0;
    bool keyboardSelecting = false;
    juce::String previewingKey;
    bool previewExplicit = false;
    juce::String hint;
    bool cacheBannerShown = false;
    juce::String lastAnnouncement;
    bool advancedAtLastLayout = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowserPanel)
};

} // namespace luthier
