#include "PresetBrowserPanel.h"
#include "PresetRow.h"
#include "PresetFilterSidebar.h"
#include "PresetDetailPane.h"
#include "PresetBrowserKeys.h"
#include "../UiPreferences.h"
#include "../../PluginProcessor.h"

namespace luthier
{

namespace
{
    constexpr int kHoverDwellMs = 300;       // 4.4
    constexpr int kSelectDwellMs = 150;      // 4.4
    constexpr int kRowHeight = 28;

    const char* const kFamilyChips[] = { "All", "Electric", "Acoustic", "Classical", "Bass" };

    double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

    const std::vector<PresetSearch::Sort>& sortChoices()
    {
        using S = PresetSearch::Sort;
        static const std::vector<S> sorts { S::relevance, S::name, S::category, S::rating, S::recentlyLoaded,
                                            S::mostLoaded, S::dateModified, S::similarity };
        return sorts;
    }
}

//==============================================================================
int PresetBrowserPanel::ListModel::getNumRows()
{
    return (int) owner.results.size();
}

juce::Component* PresetBrowserPanel::ListModel::refreshComponentForRow (int row, bool selected, juce::Component* existing)
{
    auto* rowComponent = dynamic_cast<PresetRow*> (existing);

    if (! juce::isPositiveAndBelow (row, (int) owner.results.size()))
    {
        delete existing;
        return nullptr;
    }

    if (rowComponent == nullptr)
    {
        delete existing;
        rowComponent = new PresetRow (owner);
    }

    rowComponent->setEntry (owner.results[(size_t) row], selected);
    return rowComponent;
}

void PresetBrowserPanel::ListModel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (juce::isPositiveAndBelow (row, (int) owner.results.size()))
        owner.loadEntry (owner.results[(size_t) row]);
}

void PresetBrowserPanel::ListModel::returnKeyPressed (int row)
{
    if (juce::isPositiveAndBelow (row, (int) owner.results.size()))
        owner.loadEntry (owner.results[(size_t) row]);
}

void PresetBrowserPanel::ListModel::selectedRowsChanged (int lastRow)
{
    const int entry = juce::isPositiveAndBelow (lastRow, (int) owner.results.size()) ? owner.results[(size_t) lastRow] : -1;
    owner.detail->setEntry (entry);

    const auto& index = owner.getLibrary().getIndex();
    owner.deleteButton.setEnabled (entry >= 0 && ! index[entry].info.isFactory);

    // 4.4: an arrow key starts a preview after 150 ms (when "Preview on
    // selection" is on); a mouse click never does.
    if (owner.keyboardSelecting && entry >= 0)
    {
        owner.selectDwellEntry = entry;
        owner.selectDwellSince = nowMs();

        if (owner.previewingKey.isNotEmpty() && owner.previewingKey != index[entry].key)
            owner.stopPreview();
    }

    // The two neighbours are prepared ahead, so arrowing is instant (4.4).
    for (int neighbour : { lastRow - 1, lastRow + 1 })
        if (juce::isPositiveAndBelow (neighbour, (int) owner.results.size()))
            owner.getLibrary().request (owner.results[(size_t) neighbour], PreviewRenderService::Priority::prefetch);

    if (entry >= 0)
        owner.getLibrary().request (entry, PreviewRenderService::Priority::prefetch);
}

juce::String PresetBrowserPanel::ListModel::getNameForRow (int row)
{
    return juce::isPositiveAndBelow (row, (int) owner.results.size()) ? owner.describeRow (owner.results[(size_t) row])
                                                                        : juce::String();
}

//==============================================================================
PresetBrowserPanel::PresetBrowserPanel (LuthierAudioProcessor& p)
    : OverlayPanel ("Presets"), processor (p)
{
    // ---- search, sort and chips -------------------------------------------------------
    addAndMakeVisible (searchBox);
    searchBox.setTextToShowWhenEmpty ("Search: a name, a tag or a sound (\"warm clean\")", Palette::textDisabled);
    searchBox.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);
    searchBox.setTitle ("Search presets");
    searchBox.onTextChange = [this] { refilter(); };   // 6.4: every keystroke, no debounce
    searchBox.onEscapeKey = [this] { handleBrowserKey (juce::KeyPress (juce::KeyPress::escapeKey)); };
    searchBox.addKeyListener (this);

    addChildComponent (sortLabel);
    sortLabel.setText ("Sort", juce::dontSendNotification);
    sortLabel.setColour (juce::Label::textColourId, Palette::textMuted);

    addChildComponent (sortBox);
    sortBox.setTitle ("Sort presets");

    for (size_t i = 0; i < sortChoices().size(); ++i)
        sortBox.addItem (PresetSearch::sortName (sortChoices()[i]), (int) i + 1);

    sortBox.onChange = [this]
    {
        const int i = sortBox.getSelectedId() - 1;

        if (juce::isPositiveAndBelow (i, (int) sortChoices().size()))
            setSort (sortChoices()[(size_t) i]);
    };

    addAndMakeVisible (chipRow);
    rebuildChipRow();

    addChildComponent (breadcrumb);
    breadcrumb.setColour (juce::Label::textColourId, Palette::accent);
    addChildComponent (breadcrumbClear);
    breadcrumbClear.setTitle ("Back from Sounds like");
    breadcrumbClear.onClick = [this] { clearSoundsLike(); };

    // ---- list -------------------------------------------------------------------------
    addAndMakeVisible (list);
    list.setModel (&listModel);
    list.setRowHeight (kRowHeight);
    list.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);
    list.setTitle ("Presets");
    list.addKeyListener (this);
    list.setWantsKeyboardFocus (true);

    addChildComponent (emptyLabel);
    emptyLabel.setJustificationType (juce::Justification::centred);
    emptyLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    addChildComponent (emptyClearButton);
    emptyClearButton.onClick = [this] { clearSearchAndFilters(); };

    addChildComponent (columnHeader);
    columnHeader.setFont (Fonts::label());
    columnHeader.setColour (juce::Label::textColourId, Palette::textMuted);
    columnHeader.setText ("      NAME                         CATEGORY        GUITAR              AMP                TAGS",
                          juce::dontSendNotification);

    sidebar = std::make_unique<PresetFilterSidebar> (*this);
    sidebarViewport = std::make_unique<juce::Viewport>();
    sidebarViewport->setViewedComponent (sidebar.get(), false);
    sidebarViewport->setScrollBarsShown (true, false);
    addChildComponent (*sidebarViewport);

    detail = std::make_unique<PresetDetailPane> (*this);
    addAndMakeVisible (*detail);

    // ---- footer -----------------------------------------------------------------------
    addAndMakeVisible (speakerButton);
    speakerButton.setClickingTogglesState (true);
    speakerButton.setTitle ("Previews on or off");
    speakerButton.setTooltip ("Previews on or off (Options -> Appearance -> PRESET BROWSER)");
    speakerButton.onClick = [this]
    {
        UiPreferences::get().setBool ("presetPreview.enabled", speakerButton.getToggleState());
        syncPreviewSettings();
    };

    addAndMakeVisible (volumeSlider);
    volumeSlider.setRange (-40.0, 0.0, 1.0);
    volumeSlider.setTextValueSuffix (" dB");
    volumeSlider.setTitle ("Preview volume");
    volumeSlider.setTooltip ("Preview volume");
    volumeSlider.onValueChange = [this]
    {
        UiPreferences::get().setInt ("presetPreview.volumeDb", (int) std::round (volumeSlider.getValue()));
        syncPreviewSettings();
    };

    addAndMakeVisible (footerLabel);
    footerLabel.setFont (Fonts::ui (11.0f));
    footerLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    footerLabel.setTitle ("Browser status");

    addChildComponent (clearFiltersButton);
    clearFiltersButton.onClick = [this] { clearSearchAndFilters(); };

    addAndMakeVisible (loadButton);
    loadButton.setColour (juce::TextButton::textColourOffId, Palette::accent);
    loadButton.onClick = [this] { loadEntry (getSelectedEntry()); };

    addAndMakeVisible (deleteButton);
    deleteButton.setColour (juce::TextButton::textColourOffId, Palette::clip);
    deleteButton.setEnabled (false);
    deleteButton.onClick = [this]
    {
        const int entry = getSelectedEntry();
        auto& index = getLibrary().getIndex();

        if (entry < 0 || index[entry].info.isFactory)
            return;

        const auto file = index[entry].info.file;
        const auto name = index[entry].info.name;
        juce::Component::SafePointer<PresetBrowserPanel> self (this);

        juce::NativeMessageBox::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::WarningIcon)
                .withTitle ("Delete this preset?")
                .withMessage ("\"" + name + "\" will be deleted from disk. This cannot be undone.")
                .withButton ("Delete")
                .withButton ("Cancel"),
            [safe = self, file] (int result)
            {
                if (safe == nullptr || result != 0)
                    return;

                auto& manager = safe->processor.getPresetManager();

                for (int i = 0; i < manager.getNumPresets(); ++i)
                    if (manager.getPreset (i)->file == file)
                    {
                        manager.deletePreset (i);
                        break;
                    }

                safe->getLibrary().refresh();
                safe->refilter();
            });
    };

    addAndMakeVisible (saveAsButton);
    saveAsButton.onClick = [this]
    {
        if (saveAsPanelRequested)
            saveAsPanelRequested();
        else if (onDismiss)
            onDismiss();
    };

    // ambiguity-resolutions 5.2: Morph, kept from the old browser.
    addAndMakeVisible (morphToggle);
    morphToggle.setClickingTogglesState (true);
    morphToggle.setTooltip ("Morph between two presets: continuous settings glide, switches change at the midpoint.");
    morphToggle.onClick = [this]
    {
        processor.getPresetMorph().setEnabled (morphToggle.getToggleState());
        processor.updatePresetMorph();
        refreshMorph();
    };

    for (auto* slot : { &slotAButton, &slotBButton })
    {
        addChildComponent (*slot);
        slot->setClickingTogglesState (true);
        slot->setRadioGroupId (0x4d52);
        slot->setTooltip ("Load presets into this slot");
    }

    slotAButton.onClick = [this] { processor.getPresetMorph().setCurrentSlot (PresetMorph::slotA); };
    slotBButton.onClick = [this] { processor.getPresetMorph().setCurrentSlot (PresetMorph::slotB); };

    addChildComponent (morphSlider);
    morphSlider.setTooltip ("From A to B. Automatable as Preset Morph.");
    morphAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getState(), ParamIDs::presetMorphPosition, morphSlider);
    morphSlider.onValueChange = [this] { processor.updatePresetMorph(); };

    // 10: search, chips or sidebar, list, detail pane, footer.
    int order = 1;
    searchBox.setExplicitFocusOrder (order++);
    sortBox.setExplicitFocusOrder (order++);
    chipRow.setExplicitFocusOrder (order++);
    sidebarViewport->setExplicitFocusOrder (order++);
    list.setExplicitFocusOrder (order++);
    detail->setExplicitFocusOrder (order++);

    for (auto* c : std::initializer_list<juce::Component*> { &speakerButton, &volumeSlider, &clearFiltersButton,
                                                             &saveAsButton, &morphToggle, &slotAButton, &morphSlider,
                                                             &slotBButton, &deleteButton, &loadButton })
        c->setExplicitFocusOrder (order++);

    setFocusContainerType (juce::Component::FocusContainerType::keyboardFocusContainer);
}

PresetBrowserPanel::~PresetBrowserPanel()
{
    motion.stopTimer();

    if (processor.hasPresetLibrary())
    {
        auto& library = processor.getPresetLibrary();
        library.removeChangeListener (this);
        library.onPreviewReady = nullptr;
        library.onPreviewStarted = nullptr;
    }

    list.setModel (nullptr);
}

PresetLibrary& PresetBrowserPanel::getLibrary()
{
    return processor.getPresetLibrary();
}

bool PresetBrowserPanel::isAdvancedLayout() const
{
    return processor.getUiState().advancedMode;
}

juce::Point<int> PresetBrowserPanel::getPreferredSize() const
{
    // 7.2: Advanced is 1040 x 680, clamped to the window minus 40 by the host's
    // padding; Easy keeps the old 780 x 560.
    return isAdvancedLayout() ? juce::Point<int> { 1040, 680 } : juce::Point<int> { 780, 560 };
}

//==============================================================================
void PresetBrowserPanel::overlayShown()
{
    auto& library = getLibrary();
    library.removeChangeListener (this);
    library.addChangeListener (this);

    library.onPreviewStarted = [this] (const juce::String& name)
    {
        announce ("Previewing " + name, false);
    };

    library.onPreviewReady = [this] (const PreviewRenderService::Ready& ready)
    {
        const int selected = getSelectedEntry();

        if (selected < 0 || getLibrary().getIndex()[selected].soundHash != ready.soundHash)
            return;

        if (ready.ok)
            announce ("Preview ready", false);
        else if (ready.error != "cancelled")
            announce ("Preview could not be rendered: " + ready.error, true);
    };

    library.refresh();
    syncPreviewSettings();
    restoreView();

    if (! advancedAtLastLayout != ! isAdvancedLayout())
        resized();

    refilter();

    // 2: every row without a valid preview, visible rows first.
    juce::Array<int> visible;
    const int first = list.getRowContainingPosition (0, 0);

    for (int row = juce::jmax (0, first); row < juce::jmin ((int) results.size(), juce::jmax (0, first) + 20); ++row)
        visible.add (results[(size_t) row]);

    library.queueUnrendered (visible);
    refreshMorph();

    // Select the current preset, if it is listed.
    const auto current = processor.getPresetManager().getCurrentPresetFile();
    const int currentEntry = library.getIndex().indexOfFile (current);

    if (currentEntry >= 0)
    {
        keyboardSelecting = false;
        selectEntry (currentEntry);
    }

    motion.startTimerHz (*this, 30);
    AccessibleSetup::announceOverlayOpened (*this, "Presets");
    searchBox.grabKeyboardFocus();
}

void PresetBrowserPanel::overlayHidden()
{
    // 15: closing the browser fades the preview out; on-save renders continue.
    stopPreview();
    motion.stopTimer();
    rememberView();
}

void PresetBrowserPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // The index changed: names arrived, features parsed, a preview rendered.
    const int selected = getSelectedEntry();
    const auto key = selected >= 0 ? getLibrary().getIndex()[selected].key : juce::String();

    refilter();

    if (key.isNotEmpty())
    {
        const int again = getLibrary().getIndex().indexOfKey (key);

        if (again >= 0)
        {
            const bool wasKeyboard = keyboardSelecting;
            keyboardSelecting = false;
            selectEntry (again, false);
            keyboardSelecting = wasKeyboard;
        }
    }

    if (getLibrary().getService().isCacheUnwritable())
        cacheBannerShown = true;   // 7.5: one banner, shown in the footer

    updateFooter();
}

//==============================================================================
void PresetBrowserPanel::rebuildChipRow()
{
    chips.clear();

    // 7.1: family chips are single-select; Fav, Recent and Uses Techniques toggle.
    for (auto* name : kFamilyChips)
    {
        auto* chip = chips.add (new ChipButton (name));
        chip->setRadioGroupId (0x5046);
        chip->setTitle (juce::String ("Family ") + name);
        chip->onClick = [this, chip]
        {
            const auto text = chip->getButtonText();
            filters.families.clear();
            filters.family = -1;

            for (int f = 0; f < PresetFeatures::numFamilies; ++f)
                if (text == PresetFeatures::getFamilyName (f))
                    filters.family = f;

            filtersChanged();
        };
        chipRow.addAndMakeVisible (chip);
    }

    for (auto* name : { "Fav", "Recent", "Uses Techniques" })
    {
        auto* chip = chips.add (new ChipButton (name));
        chip->setTitle (juce::String (name) == "Fav" ? "Favourites" : juce::String (name));
        chip->onClick = [this, chip]
        {
            const auto text = chip->getButtonText();
            const bool on = chip->getToggleState();

            if (text == "Fav")          filters.favourites = on;
            else if (text == "Recent")  filters.recent = on;
            else                        filters.usesTechniques = on;

            filtersChanged();
        };
        chipRow.addAndMakeVisible (chip);
    }
}

ChipButton* PresetBrowserPanel::findChip (const juce::String& text) const
{
    for (auto* chip : chips)
        if (chip->getButtonText() == text)
            return chip;

    return nullptr;
}

juce::Button& PresetBrowserPanel::getMoreLikeThisButton() noexcept
{
    return detail->getMoreLikeThis();
}

PresetRow* PresetBrowserPanel::getRowComponentFor (int entry) const
{
    for (int row = 0; row < (int) results.size(); ++row)
        if (results[(size_t) row] == entry)
            return dynamic_cast<PresetRow*> (list.getComponentForRowNumber (row));

    return nullptr;
}

//==============================================================================
juce::String PresetBrowserPanel::getQuery() const
{
    return searchBox.getText();
}

void PresetBrowserPanel::setQuery (const juce::String& q)
{
    searchBox.setText (q, juce::sendNotificationSync);
    refilter();
}

void PresetBrowserPanel::addToSearch (const juce::String& word)
{
    auto text = searchBox.getText().trim();
    const auto token = word.containsChar (' ') ? "\"" + word + "\"" : word;

    if (! PresetSearch::tokenise (text).contains (word.toLowerCase()))
        setQuery ((text.isEmpty() ? juce::String() : text + " ") + token);
}

void PresetBrowserPanel::filtersChanged()
{
    rememberView();
    sidebar->refresh();

    // Keep the Easy chips in step with the filters.
    for (auto* chip : chips)
    {
        const auto text = chip->getButtonText();
        bool on = false;

        if (text == "All")                  on = filters.family < 0 && filters.families.isEmpty();
        else if (text == "Fav")             on = filters.favourites;
        else if (text == "Recent")          on = filters.recent;
        else if (text == "Uses Techniques") on = filters.usesTechniques;
        else                                on = filters.family >= 0 && text == PresetFeatures::getFamilyName (filters.family);

        chip->setToggleState (on, juce::dontSendNotification);
    }

    refilter();
}

void PresetBrowserPanel::clearSearchAndFilters()
{
    filters = {};
    similarTo.clear();
    searchBox.setText ({}, juce::dontSendNotification);
    filtersChanged();
}

void PresetBrowserPanel::setSort (PresetSearch::Sort s)
{
    sort = s;
    rememberView();
    refilter();
}

void PresetBrowserPanel::refilter()
{
    auto& library = getLibrary();
    auto& index = library.getIndex();

    results.clear();

    if (similarTo.isNotEmpty())
    {
        const int self = index.indexOfKey (similarTo);

        if (self < 0)
            similarTo.clear();
        else
            for (const auto& r : PresetSearch::similar (index, self, filters))
                results.push_back (r.entry);
    }

    if (similarTo.isEmpty())
    {
        // 7.1: relevance while there is a query, otherwise by name.
        auto effective = sort;

        if (effective == PresetSearch::Sort::similarity)
            effective = PresetSearch::Sort::relevance;

        for (const auto& r : PresetSearch::run (index, searchBox.getText(), filters, effective, PresetLibraryPrefs::get()))
            results.push_back (r.entry);
    }

    list.updateContent();
    list.repaint();

    // Keep the sort box honest: Relevance only with a query, Similarity only in
    // the "Sounds like" view (7.2).
    const bool hasQuery = searchBox.getText().trim().isNotEmpty();
    sortBox.setItemEnabled (1, hasQuery);
    sortBox.setItemEnabled ((int) sortChoices().size(), similarTo.isNotEmpty());
    const auto shown = similarTo.isNotEmpty() ? PresetSearch::Sort::similarity
                     : (sort == PresetSearch::Sort::relevance && ! hasQuery ? PresetSearch::Sort::name : sort);

    for (size_t i = 0; i < sortChoices().size(); ++i)
        if (sortChoices()[i] == shown)
            sortBox.setSelectedId ((int) i + 1, juce::dontSendNotification);

    breadcrumb.setText (getBreadcrumbText(), juce::dontSendNotification);
    breadcrumb.setVisible (similarTo.isNotEmpty());
    breadcrumbClear.setVisible (similarTo.isNotEmpty());

    updateEmptyState();
    updateFooter();
    detail->refresh();
}

void PresetBrowserPanel::updateEmptyState()
{
    const auto text = getEmptyText();
    emptyLabel.setText (text, juce::dontSendNotification);
    emptyLabel.setVisible (text.isNotEmpty());
    emptyClearButton.setVisible (results.empty() && (! filters.isEmpty() || searchBox.getText().trim().isNotEmpty()));
    clearFiltersButton.setVisible (isAdvancedLayout() && (! filters.isEmpty() || searchBox.getText().isNotEmpty()));
}

juce::String PresetBrowserPanel::getEmptyText() const
{
    if (! results.empty())
        return {};

    const auto query = searchBox.getText().trim();

    if (query.isNotEmpty())
        return "No presets match \"" + query + "\". Try fewer words or clear filters.";

    if (filters.favourites)
        return "Tap the heart on a preset to keep it here.";

    if (filters.recent)
        return "Presets you load appear here.";

    if (! filters.isEmpty())
        return "No presets match these filters. Try fewer, or clear filters.";

    return "No presets yet.";
}

juce::String PresetBrowserPanel::getBreadcrumbText() const
{
    if (similarTo.isEmpty())
        return {};

    auto& index = const_cast<PresetBrowserPanel*> (this)->getLibrary().getIndex();
    const int self = index.indexOfKey (similarTo);
    return "Like: " + (self >= 0 ? index[self].info.name : similarTo);
}

//==============================================================================
int PresetBrowserPanel::getSelectedEntry() const
{
    const int row = list.getSelectedRow();
    return juce::isPositiveAndBelow (row, (int) results.size()) ? results[(size_t) row] : -1;
}

void PresetBrowserPanel::selectEntry (int entry, bool scrollTo)
{
    for (int row = 0; row < (int) results.size(); ++row)
        if (results[(size_t) row] == entry)
        {
            list.selectRow (row, ! scrollTo, true);
            return;
        }
}

bool PresetBrowserPanel::selectPresetNamed (const juce::String& name)
{
    int entry = getLibrary().getIndex().indexOfName (name);

    if (entry < 0)
        return false;

    if (std::find (results.begin(), results.end(), entry) == results.end())
        clearSearchAndFilters();

    keyboardSelecting = false;
    selectEntry (entry);
    return getSelectedEntry() == entry;
}

juce::String PresetBrowserPanel::describeRow (int entry) const
{
    auto& index = const_cast<PresetBrowserPanel*> (this)->getLibrary().getIndex();

    if (! juce::isPositiveAndBelow (entry, index.size()))
        return {};

    const auto& e = index[entry];
    auto& prefs = PresetLibraryPrefs::get();

    // 10: "Jazz Hollowbody, Electric Jazz, factory, favourite, 4 stars, warm, clean, jazz".
    juce::StringArray parts;
    parts.add (e.info.name);

    auto categoryAndGenre = e.info.category;

    if (e.genres.size() > 0)
        categoryAndGenre << " " << e.genres[0].substring (0, 1).toUpperCase() << e.genres[0].substring (1);

    parts.add (categoryAndGenre);
    parts.add (e.source == PresetIndex::Source::factory ? "factory"
             : e.source == PresetIndex::Source::user ? "user"
             : e.source == PresetIndex::Source::pack ? "content pack" : "extra folder");

    if (e.locked)
        parts.add ("locked");

    if (prefs.isFavourite (e.key))
        parts.add ("favourite");

    if (const int stars = prefs.getRating (e.key); stars > 0)
        parts.add (juce::String (stars) + (stars == 1 ? " star" : " stars"));

    for (const auto& d : e.descriptors)
        parts.add (d);

    return parts.joinIntoString (", ");
}

//==============================================================================
PlayGlyph::State PresetBrowserPanel::glyphStateFor (int entry, float& progress, juce::String& failure) const
{
    auto& library = const_cast<PresetBrowserPanel*> (this)->getLibrary();
    auto& index = library.getIndex();
    progress = 0.0f;

    if (! library.getSettings().enabled)
        return PlayGlyph::State::hidden;

    if (! juce::isPositiveAndBelow (entry, index.size()))
        return PlayGlyph::State::idle;

    const auto& e = index[entry];

    if (e.corrupt || e.preview == PresetIndex::PreviewState::failed)
    {
        failure = e.previewError.isNotEmpty() ? e.previewError : juce::String ("unknown error");
        return PlayGlyph::State::failed;
    }

    if (library.isWaitingFor (entry))
        return PlayGlyph::State::preparing;

    if (isPreviewing (entry))
    {
        progress = processor.getPreviewPlayer().getProgress();
        return PlayGlyph::State::playing;
    }

    return PlayGlyph::State::idle;
}

bool PresetBrowserPanel::isPreviewing (int entry) const
{
    auto& library = const_cast<PresetBrowserPanel*> (this)->getLibrary();
    return entry >= 0 && library.getPlayingEntry() == entry && processor.getPreviewPlayer().isActive();
}

void PresetBrowserPanel::startPreview (int entry, bool explicitTrigger)
{
    auto& library = getLibrary();

    if (! juce::isPositiveAndBelow (entry, library.getIndex().size()))
        return;

    if (library.play (entry, explicitTrigger, hint))
    {
        previewingKey = library.getIndex()[entry].key;
        previewExplicit = explicitTrigger;
        hint.clear();
    }
    else if (! explicitTrigger)
    {
        // Hover and selection are quietly suppressed; the footer still says why.
    }
    else if (hint.containsIgnoreCase ("could not be rendered"))
    {
        announce (hint, true);
    }

    updateFooter();

    if (auto* row = getRowComponentFor (entry))
        row->refreshPlayState();
}

void PresetBrowserPanel::togglePreview (int entry)
{
    if (isPreviewing (entry) || getLibrary().isWaitingFor (entry))
        stopPreview();
    else
        startPreview (entry, true);
}

void PresetBrowserPanel::stopPreview()
{
    if (processor.hasPresetLibrary())
        getLibrary().stop();

    previewingKey.clear();
    hoverStartedPreview = false;
    selectDwellEntry = -1;
}

void PresetBrowserPanel::rowHovered (int entry)
{
    if (entry != hoverEntry)
    {
        hoverEntry = entry;
        hoverSince = nowMs();
    }
}

void PresetBrowserPanel::rowUnhovered (int entry)
{
    if (entry != hoverEntry)
        return;

    hoverEntry = -1;

    // 4.4: leaving the row stops a preview the hover started.
    if (hoverStartedPreview)
        stopPreview();
}

void PresetBrowserPanel::mouseExit (const juce::MouseEvent& e)
{
    OverlayPanel::mouseExit (e);

    if (hoverStartedPreview && ! getLocalBounds().contains (e.getEventRelativeTo (this).getPosition()))
        stopPreview();
}

//==============================================================================
void PresetBrowserPanel::loadEntry (int entry)
{
    auto& index = getLibrary().getIndex();

    if (! juce::isPositiveAndBelow (entry, index.size()))
        return;

    const auto file = index[entry].info.file;

    // 12: a locked Pro preset opens the upsell instead (Free build only).
    if (index[entry].locked)
    {
        hint = "This preset is part of Luthier Pro. You can preview it here.";
        updateFooter();
        return;
    }

    // 4.4: loading stops the preview.
    stopPreview();

    auto& presets = processor.getPresetManager();
    int managerIndex = -1;

    for (int i = 0; i < presets.getNumPresets(); ++i)
        if (presets.getPreset (i)->file == file)
            managerIndex = i;

    // 9: the load is the one state boundary here.
    processor.pushUndoState ("Load preset");

    const bool ok = managerIndex >= 0 ? presets.loadPreset (managerIndex) : presets.loadPreset (file);
    processor.getParameterBridge().applyAllNow();

    if (! ok)
        return;

    getLibrary().noteLoaded (entry);

    // 13 / ambiguity-resolutions 5.2: while morphing, a load fills the selected slot.
    auto& morph = processor.getPresetMorph();

    if (morph.isEnabled())
    {
        morph.setSlot (morph.getCurrentSlot(), presets.toVar (presets.getCurrentPresetName()),
                       presets.getCurrentPresetName());
        refreshMorph();
    }

    if (filters.recent)
        refilter();
}

void PresetBrowserPanel::setFavourite (int entry, bool favourite)
{
    auto& index = getLibrary().getIndex();

    if (! juce::isPositiveAndBelow (entry, index.size()))
        return;

    PresetLibraryPrefs::get().setFavourite (index[entry].key, favourite);

    if (filters.favourites)
        refilter();
    else
        refreshRowsAndDetail();
}

void PresetBrowserPanel::setRating (int entry, int stars)
{
    auto& index = getLibrary().getIndex();

    if (! juce::isPositiveAndBelow (entry, index.size()))
        return;

    PresetLibraryPrefs::get().setRating (index[entry].key, stars);

    if (filters.minRating > 0 || sort == PresetSearch::Sort::rating)
        refilter();
    else
        refreshRowsAndDetail();
}

void PresetBrowserPanel::refreshRowsAndDetail()
{
    list.updateContent();
    list.repaint();
    detail->refresh();
}

std::vector<int> PresetBrowserPanel::getSoundsLike (int entry) const
{
    std::vector<int> out;
    auto& index = const_cast<PresetBrowserPanel*> (this)->getLibrary().getIndex();

    for (const auto& r : PresetSearch::similar (index, entry, filters))
        out.push_back (r.entry);

    return out;
}

void PresetBrowserPanel::showSoundsLike (int entry)
{
    auto& index = getLibrary().getIndex();

    if (! juce::isPositiveAndBelow (entry, index.size()))
        return;

    similarTo = index[entry].key;
    rememberView();
    refilter();
}

void PresetBrowserPanel::clearSoundsLike()
{
    similarTo.clear();
    rememberView();
    refilter();
}

//==============================================================================
void PresetBrowserPanel::syncPreviewSettings()
{
    auto& prefs = UiPreferences::get();
    PresetLibrary::Settings s;
    s.enabled = prefs.getBool ("presetPreview.enabled", true);
    s.hoverTrigger = prefs.getString ("presetPreview.trigger", "hover") != "click";
    s.onSelect = prefs.getBool ("presetPreview.onSelect", true);
    s.volumeDb = juce::jlimit (-40, 0, prefs.getInt ("presetPreview.volumeDb", -6));
    s.whileTransport = prefs.getBool ("presetPreview.whileTransport", false);

    getLibrary().setSettings (s);

    // 8: the footer mirrors the same keys.
    speakerButton.setToggleState (s.enabled, juce::dontSendNotification);
    speakerButton.setButtonText (s.enabled ? "Previews: On" : "Previews: Off");
    volumeSlider.setValue (s.volumeDb, juce::dontSendNotification);
    volumeSlider.setEnabled (s.enabled);

    if (! s.enabled)
        previewingKey.clear();

    list.repaint();
    updateFooter();
}

void PresetBrowserPanel::updateFooter()
{
    footerLabel.setText (getFooterText(), juce::dontSendNotification);
}

juce::String PresetBrowserPanel::getFooterText() const
{
    auto& library = const_cast<PresetBrowserPanel*> (this)->getLibrary();

    if (! library.getSettings().enabled)
        return "Previews are off - Options -> Appearance";

    if (cacheBannerShown || library.getService().isCacheUnwritable())
        return "Preview cache folder is not writable; previews will be re-rendered next time.";

    if (hint.isNotEmpty())
        return hint;

    // 4.3: the transport and host hints.
    const auto blocked = library.whyBlocked (true);

    if (blocked.isNotEmpty() && ! blocked.startsWith ("Previews wait"))
        return blocked;

    if (similarTo.isNotEmpty())
        if (const int pending = library.getIndex().numUnanalysed(); pending > 0)
            return juce::String (pending) + (pending == 1 ? " preset is" : " presets are") + " still being analysed";

    return juce::String ((int) results.size()) + " of " + juce::String (library.getIndex().size()) + " presets";
}

void PresetBrowserPanel::announce (const juce::String& text, bool alwaysAnnounce)
{
    // 10: previews are polite and need Normal verbosity or higher; failures always.
    const auto verbosity = AccessibilitySettings::get().getVerbosity();

    if (! alwaysAnnounce && verbosity == AccessibilitySettings::Verbosity::minimal)
        return;

    juce::AccessibilityHandler::postAnnouncement (
        text, alwaysAnnounce ? juce::AccessibilityHandler::AnnouncementPriority::high
                             : juce::AccessibilityHandler::AnnouncementPriority::low);
    lastAnnouncement = text;
}

void PresetBrowserPanel::rememberView()
{
    auto& prefs = UiPreferences::get();
    prefs.setInt ("presetBrowser.sort", (int) sort);
    prefs.setString ("presetBrowser.filters", juce::JSON::toString (filters.toVar(), true));
    prefs.setString ("presetBrowser.viewSimilar", similarTo);
}

void PresetBrowserPanel::restoreView()
{
    auto& prefs = UiPreferences::get();
    sort = (PresetSearch::Sort) juce::jlimit (0, (int) PresetSearch::Sort::similarity,
                                              prefs.getInt ("presetBrowser.sort", (int) PresetSearch::Sort::relevance));
    filters = PresetSearch::Filters::fromVar (juce::JSON::parse (prefs.getString ("presetBrowser.filters", {})));
    similarTo = prefs.getString ("presetBrowser.viewSimilar", {});

    const juce::ScopedValueSetter<juce::String> keepSimilar (similarTo, similarTo);
    filtersChanged();
}

void PresetBrowserPanel::refreshMorph()
{
    auto& morph = processor.getPresetMorph();
    const bool on = morph.isEnabled();

    morphToggle.setToggleState (on, juce::dontSendNotification);
    slotAButton.setVisible (on);
    slotBButton.setVisible (on);
    morphSlider.setVisible (on);

    slotAButton.setButtonText ("A: " + (morph.getSlotName (PresetMorph::slotA).isNotEmpty()
                                          ? morph.getSlotName (PresetMorph::slotA) : juce::String ("empty")));
    slotBButton.setButtonText ("B: " + (morph.getSlotName (PresetMorph::slotB).isNotEmpty()
                                          ? morph.getSlotName (PresetMorph::slotB) : juce::String ("empty")));
    slotAButton.setToggleState (morph.getCurrentSlot() == PresetMorph::slotA, juce::dontSendNotification);
    slotBButton.setToggleState (morph.getCurrentSlot() == PresetMorph::slotB, juce::dontSendNotification);
}

//==============================================================================
void PresetBrowserPanel::timerCallback()
{
    auto& library = getLibrary();
    const auto& settings = library.getSettings();
    const double now = nowMs();

    // 4.4: the hover dwell.
    if (hoverEntry >= 0 && settings.hoverTrigger && ! isPreviewing (hoverEntry)
        && ! library.isWaitingFor (hoverEntry) && now - hoverSince >= kHoverDwellMs && hoverSince > 0.0)
    {
        const int entry = hoverEntry;
        hoverSince = 0.0;   // once per rest
        startPreview (entry, false);
        hoverStartedPreview = previewingKey.isNotEmpty();
    }

    // 4.4: the keyboard-selection dwell.
    if (selectDwellEntry >= 0 && now - selectDwellSince >= kSelectDwellMs)
    {
        const int entry = selectDwellEntry;
        selectDwellEntry = -1;

        if (settings.onSelect)
            startPreview (entry, false);
    }

    if (previewingKey.isNotEmpty() && ! processor.getPreviewPlayer().isActive()
        && library.getPlayingEntry() < 0)
    {
        previewingKey.clear();
        hoverStartedPreview = false;
    }

    // The glyphs and the played part of each waveform.
    for (int row = 0; row < (int) results.size(); ++row)
        if (auto* component = dynamic_cast<PresetRow*> (list.getComponentForRowNumber (row)))
            component->refreshPlayState();

    detail->refreshPlayState();

    if (hint.isNotEmpty() && library.whyBlocked (true).isEmpty() && previewingKey.isNotEmpty())
        hint.clear();

    updateFooter();
}

//==============================================================================
bool PresetBrowserPanel::keyPressed (const juce::KeyPress& key)
{
    return handleBrowserKey (key);
}

bool PresetBrowserPanel::keyPressed (const juce::KeyPress& key, juce::Component* originator)
{
    if (originator == &searchBox)
    {
        // Down moves into the list (7.4).
        if (key == juce::KeyPress::downKey && ! results.empty())
        {
            keyboardSelecting = true;
            list.selectRow (juce::jmax (0, list.getSelectedRow()));
            list.grabKeyboardFocus();
            return true;
        }

        if (key == juce::KeyPress::escapeKey || PresetBrowserKeys::matches ("clearAll", key)
            || key == juce::KeyPress::returnKey)
            return handleBrowserKey (key);

        return false;   // the search box types it
    }

    // From the list: navigation keys go to the ListBox, marked as keyboard.
    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey || key == juce::KeyPress::pageUpKey
        || key == juce::KeyPress::pageDownKey || key == juce::KeyPress::homeKey || key == juce::KeyPress::endKey)
    {
        keyboardSelecting = true;
        return false;
    }

    return handleBrowserKey (key);
}

bool PresetBrowserPanel::handleBrowserKey (const juce::KeyPress& key)
{
    const int selected = getSelectedEntry();

    if (key == juce::KeyPress::escapeKey)
    {
        // 7.4: stop the preview if one is playing, otherwise close.
        if (processor.getPreviewPlayer().isActive() || previewingKey.isNotEmpty()
            || (selected >= 0 && getLibrary().isWaitingFor (selected)))
        {
            stopPreview();
            return true;
        }

        if (onDismiss)
            onDismiss();

        return true;
    }

    if (PresetBrowserKeys::matches ("focusSearch", key))
    {
        searchBox.grabKeyboardFocus();
        searchBox.selectAll();
        return true;
    }

    if (PresetBrowserKeys::matches ("clearAll", key))
    {
        clearSearchAndFilters();
        return true;
    }

    if (PresetBrowserKeys::matches ("preview", key))
    {
        // 7.4: the focused browser consumes Space even with nothing selected,
        // so it never reaches the tune transport or the audition.
        if (selected >= 0)
            togglePreview (selected);

        return true;
    }

    if (PresetBrowserKeys::matches ("load", key))
    {
        loadEntry (selected);
        return true;
    }

    if (PresetBrowserKeys::matches ("favourite", key))
    {
        if (selected >= 0)
            setFavourite (selected, ! PresetLibraryPrefs::get().isFavourite (getLibrary().getIndex()[selected].key));

        return true;
    }

    if (PresetBrowserKeys::matches ("soundsLike", key))
    {
        if (selected >= 0)
            showSoundsLike (selected);

        return true;
    }

    // 0-5 set the rating; the focused browser consumes every digit (7.4).
    if (const auto c = key.getTextCharacter(); c >= '0' && c <= '9' && ! key.getModifiers().isCommandDown())
    {
        if (selected >= 0 && c <= '5')
            setRating (selected, c - '0');

        return true;
    }

    // Any other printable key goes to the search box (7.4).
    if (const auto c = key.getTextCharacter(); c >= ' ' && ! key.getModifiers().isCommandDown()
                                                && ! key.getModifiers().isAltDown())
    {
        searchBox.grabKeyboardFocus();
        searchBox.moveCaretToEnd();
        searchBox.insertTextAtCaret (juce::String::charToString (c));
        return true;
    }

    return false;
}

//==============================================================================
void PresetBrowserPanel::layoutContent (juce::Rectangle<int> content)
{
    const bool advanced = isAdvancedLayout();
    advancedAtLastLayout = advanced;

    auto bottom = content.removeFromBottom (Metrics::buttonHeight);
    content.removeFromBottom (Metrics::gridHalf);

    // Bottom row: [Save As] [Morph] A ===o=== B          [Delete] [Load]
    loadButton.setBounds (bottom.removeFromRight (90));
    bottom.removeFromRight (Metrics::gridHalf);
    deleteButton.setBounds (bottom.removeFromRight (90));
    bottom.removeFromRight (Metrics::grid);
    saveAsButton.setBounds (bottom.removeFromLeft (100));
    bottom.removeFromLeft (Metrics::gridHalf);
    morphToggle.setBounds (bottom.removeFromLeft (70));
    bottom.removeFromLeft (Metrics::gridHalf);

    {
        const int slotWidth = juce::jmin (150, bottom.getWidth() / 3);
        slotAButton.setBounds (bottom.removeFromLeft (slotWidth));
        slotBButton.setBounds (bottom.removeFromRight (slotWidth));
        bottom.reduce (Metrics::gridHalf, 0);
        morphSlider.setBounds (bottom);
    }

    // Footer: (spk)[vol] status ... [Clear filters]
    auto footer = content.removeFromBottom (22);
    content.removeFromBottom (Metrics::gridHalf);

    if (advanced)
    {
        speakerButton.setBounds (footer.removeFromLeft (110));
        footer.removeFromLeft (Metrics::gridHalf);
        volumeSlider.setBounds (footer.removeFromLeft (120));
        footer.removeFromLeft (Metrics::grid);
        clearFiltersButton.setBounds (footer.removeFromRight (110));
        footerLabel.setBounds (footer);
    }
    else
    {
        footerLabel.setBounds (footer);
    }

    sortLabel.setVisible (advanced);
    sortBox.setVisible (advanced);
    chipRow.setVisible (! advanced);
    sidebarViewport->setVisible (advanced);
    columnHeader.setVisible (advanced);

    if (advanced)
    {
        // SIDEBAR | search + sort / columns / list | detail + SOUNDS LIKE
        auto side = content.removeFromLeft (200);
        content.removeFromLeft (Metrics::grid);
        sidebarViewport->setBounds (side);
        sidebar->setSize (side.getWidth() - sidebarViewport->getScrollBarThickness(),
                          sidebar->getIdealHeight (side.getWidth() - sidebarViewport->getScrollBarThickness()));

        auto right = content.removeFromRight (juce::jmin (270, content.getWidth() / 3));
        content.removeFromRight (Metrics::grid);
        detail->setBounds (right);

        auto top = content.removeFromTop (26);
        sortBox.setBounds (top.removeFromRight (150));
        sortLabel.setBounds (top.removeFromRight (40));
        top.removeFromRight (Metrics::gridHalf);
        searchBox.setBounds (top);
        content.removeFromTop (Metrics::gridHalf);
    }
    else
    {
        auto top = content.removeFromTop (26);
        volumeSlider.setBounds (top.removeFromRight (110));
        top.removeFromRight (Metrics::gridHalf);
        speakerButton.setBounds (top.removeFromRight (110));
        top.removeFromRight (Metrics::grid);
        searchBox.setBounds (top);
        content.removeFromTop (Metrics::gridHalf);

        auto chipArea = content.removeFromTop (24);
        chipRow.setBounds (chipArea);
        int x = 0;

        for (auto* chip : chips)
        {
            const int w = chip->getIdealWidth();
            chip->setBounds (x, 1, w, 22);
            x += w + Metrics::gridHalf;
        }

        content.removeFromTop (Metrics::gridHalf);

        auto right = content.removeFromRight (juce::jmin (260, content.getWidth() * 2 / 5));
        content.removeFromRight (Metrics::grid);
        detail->setBounds (right);
    }

    if (similarTo.isNotEmpty() || breadcrumb.isVisible())
    {
        auto crumb = content.removeFromTop (20);
        breadcrumbClear.setBounds (crumb.removeFromRight (24));
        breadcrumb.setBounds (crumb);
    }

    if (advanced)
        columnHeader.setBounds (content.removeFromTop (16));

    list.setBounds (content);

    auto empty = content.withSizeKeepingCentre (content.getWidth() - 20, 60);
    emptyLabel.setBounds (empty.removeFromTop (34));
    emptyClearButton.setBounds (empty.withSizeKeepingCentre (120, 24));

    detail->refresh();
    refreshMorph();
    updateEmptyState();
}

} // namespace luthier
