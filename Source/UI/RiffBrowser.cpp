#include "RiffBrowser.h"

#include "Theme.h"
#include "UiPreferences.h"
#include "MidiExportDefaults.h"
#include "PracticePanel.h"
#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Riffs/RiffAnalysis.h"
#include "../Support/ErrorLog.h"
#include "../Accessibility/Accessibility.h"

#include <cmath>

namespace luthier
{

namespace
{
    constexpr const char* kAuditionOnSelectKey = "riffs.auditionOnSelect";

    juce::ThreadPool& riffPool()
    {
        // One worker: indexing is the only job, and it runs once per session.
        static juce::ThreadPool pool (1);
        return pool;
    }

    template <typename T>
    T* findInTree (juce::Component* root)
    {
        if (root == nullptr)
            return nullptr;

        if (auto* match = dynamic_cast<T*> (root))
            return match;

        for (auto* child : root->getChildren())
            if (auto* found = findInTree<T> (child))
                return found;

        return nullptr;
    }

    juce::String techGlyph (const juce::String& token)
    {
        if (token == "bend" || token == "bendrelease" || token == "prebend") return "b";
        if (token.startsWith ("slide"))                                       return "/";
        if (token == "hammer")                                                return "h";
        if (token == "pull")                                                  return "p";
        if (token == "vibrato")                                               return "~";
        if (token == "pm")                                                    return "PM";
        if (token == "dead")                                                  return "x";
        if (token == "natural" || token == "artificial" || token == "tapharm") return "<>";
        if (token == "pinch")                                                 return "*";
        if (token == "tap")                                                   return "T";
        if (token == "trill")                                                 return "tr";
        if (token == "strum")                                                 return "D";
        if (token == "slap" || token == "thump")                              return "S";
        if (token == "pop")                                                   return "P";
        return {};
    }

    void styleLabel (juce::Label& label, float height = 12.0f, bool muted = true)
    {
        label.setFont (Fonts::ui (height));
        label.setColour (juce::Label::textColourId, muted ? Palette::textMuted : Palette::textPrimary);
    }
}

//==============================================================================
juce::var RiffUiState::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("selectedId", selectedId);
    o->setProperty ("query", query.toVar());
    o->setProperty ("keyChoice", keyChoice);
    o->setProperty ("mapScale", mapScale);
    o->setProperty ("targetScale", targetScale);
    o->setProperty ("clockMode", clockMode);
    o->setProperty ("tempoMode", tempoMode);
    o->setProperty ("tempoFactor", tempoFactor);
    o->setProperty ("absoluteBpm", absoluteBpm);
    o->setProperty ("loop", loop);
    o->setProperty ("startQuantise", startQuantise);
    o->setProperty ("levelDb", levelDb);
    o->setProperty ("dragAs", dragGeneric ? "generic" : "luthier");
    o->setProperty ("drawerOpen", drawerOpen);
    o->setProperty ("previewSplit", previewSplit);
    return juce::var (o);
}

RiffUiState RiffUiState::fromVar (const juce::var& v, bool defaultDragGeneric)
{
    RiffUiState s;
    s.dragGeneric = defaultDragGeneric;

    if (v.getDynamicObject() == nullptr)
        return s;

    s.selectedId = v["selectedId"].toString();

    if (v["query"].getDynamicObject() != nullptr)
        s.query = RiffQuery::fromVar (v["query"]);

    s.keyChoice = juce::jlimit (0, 13, (int) v.getProperty ("keyChoice", 0));
    s.mapScale = (bool) v.getProperty ("mapScale", false);
    s.targetScale = v["targetScale"].toString();
    s.clockMode = juce::jlimit (0, 1, (int) v.getProperty ("clockMode", 0));
    s.tempoMode = juce::jlimit (0, 1, (int) v.getProperty ("tempoMode", 0));
    s.tempoFactor = juce::jlimit (0.25, 2.0, (double) v.getProperty ("tempoFactor", 1.0));
    s.absoluteBpm = juce::jlimit (30.0, 300.0, (double) v.getProperty ("absoluteBpm", 120.0));
    s.loop = (bool) v.getProperty ("loop", true);
    s.startQuantise = juce::jlimit (0, 2, (int) v.getProperty ("startQuantise", 0));
    s.levelDb = juce::jlimit (-24.0, 0.0, (double) v.getProperty ("levelDb", -6.0));

    if (v.hasProperty ("dragAs"))
        s.dragGeneric = v["dragAs"].toString() == "generic";

    s.drawerOpen = (bool) v.getProperty ("drawerOpen", false);
    s.previewSplit = juce::jlimit (0.2, 0.8, (double) v.getProperty ("previewSplit", 0.5));
    return s;
}

//==============================================================================
/** The "Drag .mid" tile: drag it out for the file; click it (or press it from
    the keyboard) for Save .mid... (riff-library 6.1, 10). */
class RiffBrowser::DragTile : public juce::TextButton
{
public:
    explicit DragTile (RiffBrowser& b) : juce::TextButton ("Drag .mid"), browser (b)
    {
        setTooltip ("Drag onto a MIDI track (Alt drags Generic). Click to save the .mid file.");
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging && e.getDistanceFromDragStart() > 4 && browser.getSelectedId().isNotEmpty())
        {
            dragging = true;
            browser.startDragging ("riff:" + browser.getSelectedId(), this);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragging)
        {
            dragging = false;
            return;
        }

        juce::TextButton::mouseUp (e);
    }

private:
    RiffBrowser& browser;
    bool dragging = false;
};

//==============================================================================
RiffBrowser::RiffBrowser (LuthierAudioProcessor& p, bool isCompact)
    : processor (p),
      library (p.getRiffLibrary()),
      player (p.getEngine().getRiffPlayer()),
      compact (isCompact),
      dragTileOwner (std::make_unique<DragTile> (*this)),
      dragTile (*dragTileOwner)
{
    setWantsKeyboardFocus (true);
    setTitle (compact ? "Riff drawer" : "Riffs");
    setDescription ("Browse, filter and audition the riff library.");

    loadState();
    buildControls();
    lastGuitar = RiffDestinations::guitarSummary (processor);

    motion.startTimerHz (*this, 30);   // cpu-quality-modes 6
}

RiffBrowser::~RiffBrowser()
{
    stopTimer();
}

void RiffBrowser::ensureLibraryLoaded (bool now)
{
    if (library.isIndexLoaded())
    {
        if (! indexSeen)
            pollLibrary();

        return;
    }

    // Options -> FILE LOCATIONS "Riffs folder" (user riffs).
    const auto userFolder = UiPreferences::get().getString ("riffs.userFolder", {});

    if (userFolder.isNotEmpty() && juce::File::isAbsolutePath (userFolder))
        library.setFolders (library.getFactoryFolder(), juce::File (userFolder), RiffLibrary::getDefaultGlobalFile());

    // 6.1: drag files older than 30 days go, once a session, off the message thread.
    static std::atomic<bool> pruned { false };

    if (! pruned.exchange (true))
        riffPool().addJob ([] { RiffDestinations::pruneDragFolder (RiffLibrary::getDragFolder(), 30); });

    if (now)
        library.loadIndexNow();
    else
        library.loadIndexAsync (riffPool());

    pollLibrary();
}

void RiffBrowser::visibilityChanged()
{
    if (isShowing())
        ensureLibraryLoaded();
}

void RiffBrowser::parentHierarchyChanged()
{
    if (isShowing())
        ensureLibraryLoaded();
}

//==============================================================================
void RiffBrowser::loadState()
{
    const auto defaults = MidiExportDefaults::load();
    state = RiffUiState::fromVar (processor.getUiState().riffs, defaults.profile == MidiProfile::generic);
}

void RiffBrowser::saveState()
{
    processor.getUiState().riffs = state.toVar();
}

void RiffBrowser::stateChanged (bool recompile)
{
    saveState();
    applyPlayerSettings();

    if (recompile)
        recompilePreview (true, false);

    updateStatus();
}

//==============================================================================
void RiffBrowser::buildControls()
{
    auto add = [this] (juce::Component& c) { addAndMakeVisible (c); };

    // ---- header ----------------------------------------------------------------------
    titleLabel.setFont (Fonts::display (18.0f));
    titleLabel.setColour (juce::Label::textColourId, Palette::accent);

    if (! compact)
        add (titleLabel);

    searchBox.setTextToShowWhenEmpty ("Search riffs...", Palette::textDisabled);
    searchBox.setText (state.query.text, false);
    searchBox.onTextChange = [this]
    {
        state.query.text = searchBox.getText();
        saveState();
        refreshList();
    };
    AccessibleSetup::configureDescriptive (searchBox, "Search riffs", "Words to find in names, tags and genres.");
    searchBox.setTitle ("Search riffs");
    add (searchBox);

    fitsToggle.setToggleState (state.query.fitsInstrument, juce::dontSendNotification);
    fitsToggle.onClick = [this]
    {
        state.query.fitsInstrument = fitsToggle.getToggleState();
        saveState();
        refreshList();
    };
    fitsToggle.setTooltip ("Hide riffs that would need another instrument family or more strings");
    AccessibleSetup::configureButton (fitsToggle, "Fits this instrument");

    if (! compact)
        add (fitsToggle);

    saveButton.onClick = [this] { showPlusMenu(); };
    saveButton.setTooltip ("Save the captured phrase as a riff, or import a .mid file");
    AccessibleSetup::configureButton (saveButton, "Save riff", "Saves a phrase from the capture as a user riff.");

    if (! compact)
        add (saveButton);

    // ---- genre -----------------------------------------------------------------------
    const auto& genres = RiffVocabulary::genres();

    for (size_t g = 0; g < genres.size(); ++g)
    {
        auto* chip = genreChips.add (new juce::TextButton (juce::String (genres[g].displayName).upToFirstOccurrenceOf (" /", false, false)));
        chip->setClickingTogglesState (true);
        chip->setToggleState (state.query.genres.test (g), juce::dontSendNotification);
        chip->onClick = [this, g, chip]
        {
            state.query.genres.set (g, chip->getToggleState());
            saveState();
            refreshList();
        };
        AccessibleSetup::configureButton (*chip, juce::String (genres[g].displayName) + " genre filter");

        if (! compact)
            add (*chip);
    }

    userChip.setClickingTogglesState (true);
    userChip.setToggleState (state.query.userOnly, juce::dontSendNotification);
    userChip.onClick = [this]
    {
        state.query.userOnly = userChip.getToggleState();
        saveState();
        refreshList();
    };
    AccessibleSetup::configureButton (userChip, "User riffs only");

    if (! compact)
        add (userChip);

    genreBox.addItem ("All genres", 1);

    for (size_t g = 0; g < genres.size(); ++g)
        genreBox.addItem (genres[g].displayName, (int) g + 2);

    {
        int selected = 1;

        for (size_t g = 0; g < genres.size(); ++g)
            if (state.query.genres.test (g) && state.query.genres.count() == 1)
                selected = (int) g + 2;

        genreBox.setSelectedId (selected, juce::dontSendNotification);
    }

    genreBox.onChange = [this]
    {
        state.query.genres.reset();

        if (genreBox.getSelectedId() >= 2)
            state.query.genres.set ((size_t) genreBox.getSelectedId() - 2);

        saveState();
        refreshList();
    };
    AccessibleSetup::configureComboBox (genreBox, "Genre");

    if (compact)
        add (genreBox);

    // ---- filters ---------------------------------------------------------------------
    typeBox.addItem ("All types", 1);

    for (int t = 0; t < RiffVocabulary::types().size(); ++t)
        typeBox.addItem (RiffVocabulary::types()[t].substring (0, 1).toUpperCase() + RiffVocabulary::types()[t].substring (1), t + 2);

    {
        int selected = 1;

        for (int t = 0; t < 4; ++t)
            if (state.query.types.test ((size_t) t) && state.query.types.count() == 1)
                selected = t + 2;

        typeBox.setSelectedId (selected, juce::dontSendNotification);
    }

    typeBox.onChange = [this]
    {
        state.query.types.reset();

        if (typeBox.getSelectedId() >= 2)
            state.query.types.set ((size_t) typeBox.getSelectedId() - 2);

        saveState();
        refreshList();
    };
    AccessibleSetup::configureComboBox (typeBox, "Type");
    add (typeBox);

    difficultySlider.setRange (1.0, 5.0, 1.0);
    difficultySlider.setMinAndMaxValues (state.query.minDifficulty, state.query.maxDifficulty, juce::dontSendNotification);
    difficultySlider.onValueChange = [this]
    {
        state.query.minDifficulty = (int) difficultySlider.getMinValue();
        state.query.maxDifficulty = (int) difficultySlider.getMaxValue();
        difficultyLabel.setText ("Diff " + juce::String (state.query.minDifficulty) + "-"
                                   + juce::String (state.query.maxDifficulty), juce::dontSendNotification);
        saveState();
        refreshList();
    };
    AccessibleSetup::configureSlider (difficultySlider, "Difficulty range");
    difficultyLabel.setText ("Diff " + juce::String (state.query.minDifficulty) + "-" + juce::String (state.query.maxDifficulty),
                             juce::dontSendNotification);
    styleLabel (difficultyLabel);

    tempoFilterSlider.setRange (40.0, 240.0, 1.0);
    tempoFilterSlider.setMinAndMaxValues (juce::jlimit (40.0, 240.0, state.query.minTempo),
                                          juce::jlimit (40.0, 240.0, state.query.maxTempo), juce::dontSendNotification);
    tempoFilterSlider.onValueChange = [this]
    {
        // The ends mean "any": a 300 bpm riff is not hidden by a 240 slider.
        state.query.minTempo = tempoFilterSlider.getMinValue() <= 40.0 ? 0.0 : tempoFilterSlider.getMinValue();
        state.query.maxTempo = tempoFilterSlider.getMaxValue() >= 240.0 ? 1000.0 : tempoFilterSlider.getMaxValue();
        tempoFilterLabel.setText (juce::String ((int) tempoFilterSlider.getMinValue()) + "-"
                                    + juce::String ((int) tempoFilterSlider.getMaxValue()), juce::dontSendNotification);
        saveState();
        refreshList();
    };
    AccessibleSetup::configureSlider (tempoFilterSlider, "Tempo range", " bpm");
    tempoFilterLabel.setText (juce::String ((int) tempoFilterSlider.getMinValue()) + "-"
                                + juce::String ((int) tempoFilterSlider.getMaxValue()), juce::dontSendNotification);
    styleLabel (tempoFilterLabel);

    if (! compact)
    {
        add (difficultyLabel);
        add (difficultySlider);
        add (tempoFilterLabel);
        add (tempoFilterSlider);
    }

    for (int d = 1; d <= 5; ++d)
    {
        auto* chip = difficultyChips.add (new juce::TextButton (juce::String (d)));
        chip->setClickingTogglesState (true);
        chip->setToggleState (d >= state.query.minDifficulty && d <= state.query.maxDifficulty
                                && ! (state.query.minDifficulty == 1 && state.query.maxDifficulty == 5),
                              juce::dontSendNotification);
        chip->onClick = [this]
        {
            int lo = 6, hi = 0;

            for (int k = 0; k < difficultyChips.size(); ++k)
                if (difficultyChips[k]->getToggleState())
                {
                    lo = juce::jmin (lo, k + 1);
                    hi = juce::jmax (hi, k + 1);
                }

            state.query.minDifficulty = hi == 0 ? 1 : lo;
            state.query.maxDifficulty = hi == 0 ? 5 : hi;
            saveState();
            refreshList();
        };
        AccessibleSetup::configureButton (*chip, "Difficulty " + juce::String (d));

        if (compact)
            add (*chip);
    }

    keyFilterBox.addItem ("Any key", 1);

    for (int k = 0; k < 12; ++k)
        keyFilterBox.addItem (RiffVocabulary::rootName (k), k + 2);

    keyFilterBox.setSelectedId (state.query.keyRoot >= 0 ? state.query.keyRoot + 2 : 1, juce::dontSendNotification);
    keyFilterBox.onChange = [this]
    {
        state.query.keyRoot = keyFilterBox.getSelectedId() - 2;
        saveState();
        refreshList();
    };
    AccessibleSetup::configureComboBox (keyFilterBox, "Key filter");

    techButton.onClick = [this]
    {
        juce::PopupMenu menu;
        const auto& all = RiffVocabulary::allTechniques();

        for (int t = 0; t < all.size(); ++t)
            menu.addItem (t + 1, RiffVocabulary::techniqueDisplayName (all[t]), true, state.query.techniques.test ((size_t) t));

        menu.addSeparator();
        menu.addItem (1000, "Any technique");

        juce::Component::SafePointer<RiffBrowser> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&techButton), [safe] (int r)
        {
            if (safe == nullptr || r <= 0)
                return;

            if (r == 1000)
                safe->state.query.techniques.reset();
            else
                safe->state.query.techniques.flip ((size_t) r - 1);

            safe->saveState();
            safe->refreshList();
        });
    };
    AccessibleSetup::configureButton (techButton, "Technique filter", "Shows only riffs using every chosen technique.");

    favFilter.setClickingTogglesState (true);
    favFilter.setToggleState (state.query.favouritesOnly, juce::dontSendNotification);
    favFilter.onClick = [this]
    {
        state.query.favouritesOnly = favFilter.getToggleState();
        saveState();
        refreshList();
    };
    AccessibleSetup::configureButton (favFilter, "Favourites only");

    sortBox.addItem ("Genre", 1);
    sortBox.addItem ("Name", 2);
    sortBox.addItem ("Tempo", 3);
    sortBox.addItem ("Difficulty", 4);
    sortBox.addItem ("Key", 5);
    sortBox.addItem ("Recent", 6);
    sortBox.setSelectedId ((int) state.query.sort + 1, juce::dontSendNotification);
    sortBox.onChange = [this]
    {
        state.query.sort = (RiffQuery::Sort) (sortBox.getSelectedId() - 1);
        saveState();
        refreshList();
    };
    AccessibleSetup::configureComboBox (sortBox, "Sort by");

    if (! compact)
    {
        add (keyFilterBox);
        add (techButton);
        add (favFilter);
        add (sortBox);
    }

    // ---- list ------------------------------------------------------------------------
    list.setRowHeight (compact ? 30 : 34);
    list.setMultipleSelectionEnabled (false);
    list.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);
    list.setTitle ("Riff list");
    list.setWantsKeyboardFocus (true);
    add (list);

    // ---- preview ---------------------------------------------------------------------
    styleLabel (nameLabel, 15.0f, false);
    nameLabel.setFont (Fonts::ui (15.0f, true));
    styleLabel (infoLabel);
    add (nameLabel);

    if (! compact)
        add (infoLabel);

    favButton.setClickingTogglesState (true);
    favButton.onClick = [this]
    {
        if (state.selectedId.isNotEmpty())
        {
            library.setFavourite (state.selectedId, favButton.getToggleState());
            list.repaint();
        }
    };
    favButton.setTooltip ("Favourite");
    AccessibleSetup::configureButton (favButton, "Favourite");
    add (favButton);

    if (! compact)
        add (tabView);

    playButton.onClick = [this] { toggleAudition(); };
    playButton.setTooltip ("Play or stop the riff (Space)");
    AccessibleSetup::configureButton (playButton, "Play riff", "Auditions the selected riff through the current sound.");
    add (playButton);

    loopToggle.setToggleState (state.loop, juce::dontSendNotification);
    loopToggle.onClick = [this]
    {
        state.loop = loopToggle.getToggleState();
        stateChanged (false);
    };
    AccessibleSetup::configureButton (loopToggle, "Loop");
    add (loopToggle);

    startBox.addItem ("Start: next bar", 1);
    startBox.addItem ("Start: next beat", 2);
    startBox.addItem ("Start: at once", 3);
    startBox.setSelectedId (state.startQuantise + 1, juce::dontSendNotification);
    startBox.onChange = [this]
    {
        state.startQuantise = startBox.getSelectedId() - 1;
        stateChanged (false);
    };
    AccessibleSetup::configureComboBox (startBox, "Start");

    if (! compact)
        add (startBox);

    keyBox.addItem ("Key: riff's own", 1);

    for (int k = 0; k < 12; ++k)
        keyBox.addItem ("Key: " + RiffVocabulary::rootName (k), k + 2);

    keyBox.addItem ("Key: Tune", 14);
    keyBox.setSelectedId (state.keyChoice + 1, juce::dontSendNotification);
    keyBox.onChange = [this]
    {
        state.keyChoice = keyBox.getSelectedId() - 1;
        stateChanged();
    };
    AccessibleSetup::configureComboBox (keyBox, "Key");
    add (keyBox);

    mapScaleToggle.setToggleState (state.mapScale, juce::dontSendNotification);
    mapScaleToggle.onClick = [this]
    {
        state.mapScale = mapScaleToggle.getToggleState();
        stateChanged();
    };
    AccessibleSetup::configureButton (mapScaleToggle, "Map scale", "Keeps each note's scale degree in the new scale.");

    scaleBox.addItem ("Scale: keep", 1);

    for (int sc = 0; sc < RiffVocabulary::scales().size() - 1; ++sc)
        scaleBox.addItem ("Scale: " + RiffVocabulary::scaleDisplayName (RiffVocabulary::scales()[sc]), sc + 2);

    scaleBox.setSelectedId (state.targetScale.isEmpty() ? 1 : RiffVocabulary::scales().indexOf (state.targetScale) + 2,
                            juce::dontSendNotification);
    scaleBox.onChange = [this]
    {
        state.targetScale = scaleBox.getSelectedId() >= 2 ? RiffVocabulary::scales()[scaleBox.getSelectedId() - 2] : juce::String();
        stateChanged();
    };
    AccessibleSetup::configureComboBox (scaleBox, "Target scale");

    if (! compact)
    {
        add (mapScaleToggle);
        add (scaleBox);
    }

    clockBox.addItem ("Clock: Auto", 1);
    clockBox.addItem ("Clock: Own", 2);
    clockBox.setSelectedId (state.clockMode + 1, juce::dontSendNotification);
    clockBox.onChange = [this]
    {
        state.clockMode = clockBox.getSelectedId() - 1;
        stateChanged (false);
    };
    clockBox.setTooltip ("Auto follows the host while it plays; Own always uses the tempo here");
    AccessibleSetup::configureComboBox (clockBox, "Clock");
    add (clockBox);

    tempoModeBox.addItem ("Tempo x", 1);
    tempoModeBox.addItem ("Tempo bpm", 2);
    tempoModeBox.setSelectedId (state.tempoMode + 1, juce::dontSendNotification);

    auto setTempoRange = [this]
    {
        suppressCallbacks = true;

        if (state.tempoMode == 0)
        {
            tempoSlider.setRange (0.25, 2.0, 0.05);
            tempoSlider.setTextValueSuffix (" x");
            tempoSlider.setValue (state.tempoFactor, juce::dontSendNotification);
        }
        else
        {
            tempoSlider.setRange (30.0, 300.0, 1.0);
            tempoSlider.setTextValueSuffix (" bpm");
            tempoSlider.setValue (state.absoluteBpm, juce::dontSendNotification);
        }

        suppressCallbacks = false;
    };

    tempoModeBox.onChange = [this, setTempoRange]
    {
        state.tempoMode = tempoModeBox.getSelectedId() - 1;
        setTempoRange();
        stateChanged();
    };
    AccessibleSetup::configureComboBox (tempoModeBox, "Tempo mode");
    add (tempoModeBox);

    setTempoRange();
    tempoSlider.onValueChange = [this]
    {
        if (suppressCallbacks)
            return;

        if (state.tempoMode == 0)
            state.tempoFactor = tempoSlider.getValue();
        else
            state.absoluteBpm = tempoSlider.getValue();

        stateChanged();
    };
    AccessibleSetup::configureSlider (tempoSlider, "Audition tempo");
    add (tempoSlider);

    levelSlider.setRange (-24.0, 0.0, 0.5);
    levelSlider.setTextValueSuffix (" dB");
    levelSlider.setValue (state.levelDb, juce::dontSendNotification);
    levelSlider.onValueChange = [this]
    {
        state.levelDb = levelSlider.getValue();
        stateChanged();
    };
    levelSlider.setTooltip ("Audition level: scales velocity, so the rig's tone does not change");
    AccessibleSetup::configureSlider (levelSlider, "Audition level", " dB");
    styleLabel (levelLabel);

    if (! compact)
    {
        add (levelLabel);
        add (levelSlider);
    }

    dragTile.onClick = [this]
    {
        // The keyboard equivalent of the drag (10): Save .mid...
        if (state.selectedId.isEmpty())
            return;

        const auto folder = UiPreferences::get().getString ("riffs.lastSaveFolder",
                                                            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getFullPathName());
        auto riff = getSelectedRiff();
        const auto name = riff != nullptr && previewCompiled != nullptr
                            ? RiffDestinations::dragFileName (*riff, *previewCompiled, getShownTempo()) : juce::String ("riff.mid");

        chooser = std::make_unique<juce::FileChooser> ("Save .mid", juce::File (folder).getChildFile (name), "*.mid");
        juce::Component::SafePointer<RiffBrowser> safe (this);

        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();

            if (safe != nullptr && file != juce::File())
            {
                UiPreferences::get().setString ("riffs.lastSaveFolder", file.getParentDirectory().getFullPathName());
                safe->saveMidTo (file.withFileExtension ("mid"), safe->state.dragGeneric);
            }
        });
    };
    AccessibleSetup::configureButton (dragTile, "Save .mid", "Drag to a MIDI track, or press to save the riff as a .mid file.");
    add (dragTile);

    addToTuneButton.onClick = [this] { addToTune(); };
    addToTuneButton.setTooltip ("Put the riff into the selected section of the tune");
    AccessibleSetup::configureButton (addToTuneButton, "Add to Tune");

    if (! compact)
        add (addToTuneButton);

    looperButton.onClick = [this] { sendToLooper(); };
    looperButton.setTooltip ("Render the riff as a looper layer, whole bars at this tempo");
    AccessibleSetup::configureButton (looperButton, "Send to Looper");
    add (looperButton);

    learnButton.onClick = [this] { learnIt(); };
    learnButton.setTooltip ("Open it in the practice tab reader, looping at 70% with the speed trainer");
    AccessibleSetup::configureButton (learnButton, "Learn It");
    add (learnButton);

    for (auto* b : { &dragAsLuthier, &dragAsGeneric })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (0x7166);
    }

    dragAsLuthier.setToggleState (! state.dragGeneric, juce::dontSendNotification);
    dragAsGeneric.setToggleState (state.dragGeneric, juce::dontSendNotification);
    dragAsLuthier.onClick = [this] { state.dragGeneric = false; saveState(); };
    dragAsGeneric.onClick = [this] { state.dragGeneric = true;  saveState(); };
    AccessibleSetup::configureButton (dragAsLuthier, "Drag as Luthier profile");
    AccessibleSetup::configureButton (dragAsGeneric, "Drag as Generic profile");
    styleLabel (dragAsLabel);

    if (! compact)
    {
        add (dragAsLabel);
        add (dragAsLuthier);
        add (dragAsGeneric);
    }

    editInfoButton.onClick = [this] { showSaveDialog (getSelectedRiff()); };
    duplicateButton.onClick = [this]
    {
        if (auto riff = getSelectedRiff())
        {
            Riff copy = *riff;
            copy.meta.id = {};
            copy.meta.created = {};
            copy.meta.name = riff->meta.name + " copy";

            if (library.saveUserRiff (copy).wasOk())
            {
                refreshList();
                selectRiff (copy.meta.id);
            }
        }
    };
    revealButton.onClick = [this]
    {
        if (const auto* e = library.findEntry (state.selectedId))
            e->file.revealToUser();
    };
    deleteButton.onClick = [this]
    {
        const auto* e = library.findEntry (state.selectedId);

        if (e == nullptr || e->factory)
            return;

        auto* w = new juce::AlertWindow ("Delete riff", "Move \"" + e->name + "\" to the trash?",
                                         juce::MessageBoxIconType::WarningIcon, this);
        w->addButton ("Delete", 1);
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        juce::Component::SafePointer<RiffBrowser> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int result)
        {
            if (safe != nullptr && result == 1)
                safe->deleteSelectedUserRiff();
        }), true);
    };

    for (auto* b : { &editInfoButton, &duplicateButton, &revealButton, &deleteButton })
    {
        AccessibleSetup::configureButton (*b, b->getButtonText() + " user riff");

        if (! compact)
            addChildComponent (*b);
    }

    // ---- status ----------------------------------------------------------------------
    styleLabel (countLabel);
    styleLabel (statusLabel);
    styleLabel (hintLabel, 13.0f, false);
    hintLabel.setJustificationType (juce::Justification::centred);
    styleLabel (bannerLabel, 12.0f, false);
    bannerLabel.setColour (juce::Label::textColourId, Palette::warning);

    add (countLabel);

    if (! compact)
        add (statusLabel);

    addChildComponent (hintLabel);
    addChildComponent (bannerLabel);

    clearFiltersButton.onClick = [this] { clearFilters(); };
    AccessibleSetup::configureButton (clearFiltersButton, "Clear filters");
    addChildComponent (clearFiltersButton);

    bannerRevealButton.onClick = [this]
    {
        if (! library.getUnreadableFiles().isEmpty())
            juce::File (library.getUnreadableFiles()[0]).revealToUser();
    };
    AccessibleSetup::configureButton (bannerRevealButton, "Reveal unreadable riffs");
    addChildComponent (bannerRevealButton);

    // accessibility 10's order: search, chips, filters, list, preview.
    int order = 1;

    for (auto* c : getFocusOrder())
        c->setExplicitFocusOrder (order++);
}

juce::Array<juce::Component*> RiffBrowser::getFocusOrder()
{
    juce::Array<juce::Component*> order;
    order.add (&searchBox);

    if (compact)
    {
        order.add (&genreBox);
        order.add (&typeBox);

        for (auto* chip : difficultyChips)
            order.add (chip);
    }
    else
    {
        order.add (&fitsToggle);
        order.add (&saveButton);

        for (auto* chip : genreChips)
            order.add (chip);

        order.add (&userChip);
        order.add (&typeBox);
        order.add (&difficultySlider);
        order.add (&tempoFilterSlider);
        order.add (&keyFilterBox);
        order.add (&techButton);
        order.add (&favFilter);
        order.add (&sortBox);
    }

    order.add (&list);
    order.add (&favButton);

    if (! compact)
        order.add (&tabView);

    order.add (&playButton);
    order.add (&loopToggle);

    if (! compact)
        order.add (&startBox);

    order.add (&keyBox);

    if (! compact)
    {
        order.add (&mapScaleToggle);
        order.add (&scaleBox);
    }

    order.add (&clockBox);
    order.add (&tempoModeBox);
    order.add (&tempoSlider);

    if (! compact)
        order.add (&levelSlider);

    order.add (&dragTile);

    if (! compact)
        order.add (&addToTuneButton);

    order.add (&looperButton);
    order.add (&learnButton);

    if (! compact)
    {
        order.add (&dragAsLuthier);
        order.add (&dragAsGeneric);
    }

    return order;
}

//==============================================================================
void RiffBrowser::refreshList()
{
    rows = library.query (state.query);

    // The instrument the "Fits" filter checks against is the loaded one.
    if (state.query.fitsInstrument)
    {
        RiffQuery q = state.query;
        q.instrument = RiffDestinations::guitarSummary (processor);
        rows = library.query (q);
    }

    list.updateContent();

    const int selectedRow = getSelectedRow();

    if (selectedRow >= 0)
        list.selectRow (selectedRow, false, true);
    else
        list.deselectAllRows();

    list.repaint();
    updateStatus();
}

const RiffIndexEntry* RiffBrowser::getRowEntry (int row) const
{
    return juce::isPositiveAndBelow (row, (int) rows.size()) ? library.getEntry (rows[(size_t) row]) : nullptr;
}

int RiffBrowser::getSelectedRow() const
{
    const int index = library.indexOf (state.selectedId);

    for (size_t r = 0; r < rows.size(); ++r)
        if (rows[r] == index)
            return (int) r;

    return -1;
}

void RiffBrowser::selectRow (int row)
{
    const auto* entry = getRowEntry (row);

    if (entry == nullptr)
        return;

    list.selectRow (row, false, true);

    if (entry->id == state.selectedId && previewCompiled != nullptr)
        return;

    state.selectedId = entry->id;
    saveState();
    recompilePreview (false, true);
    updateUserRiffButtons();

    // "Audition on select" (Options -> General, default off); while playing,
    // moving to another riff auditions it too.
    if (UiPreferences::get().getBool (kAuditionOnSelectKey, false) || (auditionLatched && isAuditioning()))
        audition();

    updateStatus();
}

void RiffBrowser::selectRiff (const juce::String& id)
{
    for (size_t r = 0; r < rows.size(); ++r)
        if (library.getEntry (rows[r]) != nullptr && library.getEntry (rows[r])->id == id)
        {
            selectRow ((int) r);
            return;
        }

    // Not in the filtered list: still the selection (a restore, a new user riff).
    if (library.findEntry (id) != nullptr)
    {
        state.selectedId = id;
        saveState();
        recompilePreview (false, true);
        updateUserRiffButtons();
        updateStatus();
    }
}

std::shared_ptr<const Riff> RiffBrowser::getSelectedRiff()
{
    return state.selectedId.isEmpty() ? nullptr : library.getRiff (state.selectedId);
}

void RiffBrowser::clearFilters()
{
    const bool fits = state.query.fitsInstrument;
    const auto sort = state.query.sort;
    state.query = RiffQuery {};
    state.query.fitsInstrument = fits;
    state.query.sort = sort;

    suppressCallbacks = true;
    searchBox.setText ({}, false);

    for (auto* chip : genreChips)
        chip->setToggleState (false, juce::dontSendNotification);

    for (auto* chip : difficultyChips)
        chip->setToggleState (false, juce::dontSendNotification);

    userChip.setToggleState (false, juce::dontSendNotification);
    favFilter.setToggleState (false, juce::dontSendNotification);
    genreBox.setSelectedId (1, juce::dontSendNotification);
    typeBox.setSelectedId (1, juce::dontSendNotification);
    keyFilterBox.setSelectedId (1, juce::dontSendNotification);
    difficultySlider.setMinAndMaxValues (1.0, 5.0, juce::dontSendNotification);
    tempoFilterSlider.setMinAndMaxValues (40.0, 240.0, juce::dontSendNotification);
    suppressCallbacks = false;

    saveState();
    refreshList();
}

//==============================================================================
RiffPlaySettings RiffBrowser::playSettings()
{
    RiffPlaySettings s;

    if (state.keyChoice >= 1 && state.keyChoice <= 12)
        s.targetRoot = state.keyChoice - 1;
    else if (state.keyChoice == 13 && processor.getTuneSession().getTune().getNumSections() > 0)
        s.targetRoot = processor.getTuneSession().getTune().meta.keyTonic;

    s.targetScale = state.targetScale;
    s.mapScale = state.mapScale;
    s.nominalBpm = getShownTempo();
    s.levelDb = state.levelDb;
    return s;
}

double RiffBrowser::getShownTempo()
{
    if (state.clockMode == 0 && player.isFollowingHost() && player.getPlayingBpm() > 0.0)
        return player.getPlayingBpm();

    if (state.tempoMode == 1)
        return state.absoluteBpm;

    const auto* entry = library.findEntry (state.selectedId);
    return juce::jlimit (Riff::kMinTempo, Riff::kMaxTempo, (entry != nullptr ? entry->tempoBpm : 120.0) * state.tempoFactor);
}

void RiffBrowser::applyPlayerSettings()
{
    player.setLooping (state.loop);
    player.setClockMode ((RiffPlayer::ClockMode) state.clockMode);
    player.setStartQuantise ((RiffPlayer::StartQuantise) state.startQuantise);
    player.setTempoFactor (state.tempoFactor);
    player.setAbsoluteBpm (state.tempoMode == 1 ? state.absoluteBpm : 0.0);
}

void RiffBrowser::recompilePreview (bool toPlayer, bool immediate)
{
    auto riff = getSelectedRiff();

    if (riff == nullptr)
    {
        previewCompiled = nullptr;
        tabView.setCompiled (nullptr);
        nameLabel.setText ({}, juce::dontSendNotification);
        infoLabel.setText ({}, juce::dontSendNotification);
        return;
    }

    lastGuitar = RiffDestinations::guitarSummary (processor);
    previewCompiled = RiffCompiler::compile (*riff, playSettings(), lastGuitar);
    tabView.setCompiled (previewCompiled);

    nameLabel.setText (riff->meta.name, juce::dontSendNotification);
    const int genre = RiffVocabulary::indexOfGenre (riff->genre);
    infoLabel.setText ((genre >= 0 ? juce::String (RiffVocabulary::genres()[(size_t) genre].displayName) : juce::String ("User"))
                         + " - " + riff->type + " - " + RiffVocabulary::rootName (previewCompiled->placement.targetRoot)
                         + " " + RiffVocabulary::scaleDisplayName (previewCompiled->placement.targetScale),
                       juce::dontSendNotification);
    favButton.setToggleState (library.isFavourite (riff->meta.id), juce::dontSendNotification);

    if (toPlayer && playingId == riff->meta.id && (player.isPlaying() || player.isWaiting()))
        player.setCompiled (previewCompiled, immediate);
}

void RiffBrowser::audition()
{
    auto riff = getSelectedRiff();

    if (riff == nullptr)
        return;

    applyPlayerSettings();
    recompilePreview (false, true);
    player.setCompiled (previewCompiled, true);
    player.play();

    playingId = riff->meta.id;
    auditionLatched = true;
    library.notePlayed (riff->meta.id);

    announce (RiffDestinations::auditionAnnouncement (riff->meta.name, previewCompiled->placement.targetRoot,
                                                       getShownTempo(), state.loop));
    list.repaint();
    updateStatus();
}

void RiffBrowser::stopAudition()
{
    player.stop();
    learning = false;

    if (auditionLatched)
        announce ("Stopped");

    auditionLatched = false;
    list.repaint();
}

bool RiffBrowser::isAuditioning() const
{
    return player.isPlaying() || player.isWaiting();
}

void RiffBrowser::toggleAudition()
{
    if (auditionLatched && isAuditioning())
        stopAudition();
    else
        audition();
}

void RiffBrowser::announce (const juce::String& text)
{
    lastAnnouncement = text;
    juce::AccessibilityHandler::postAnnouncement (text, juce::AccessibilityHandler::AnnouncementPriority::medium);
}

void RiffBrowser::postBanner (const juce::String& id, const juce::String& message, bool warning)
{
    if (auto* editor = findParentComponentOfClass<LuthierAudioProcessorEditor>())
        editor->getNotifications().post ({ id, message, warning ? Notification::Level::warning : Notification::Level::info });
    else
        statusLabel.setText (message, juce::dontSendNotification);
}

//==============================================================================
juce::File RiffBrowser::makeDragFile (bool forceGeneric)
{
    auto riff = getSelectedRiff();

    if (riff == nullptr)
        return {};

    // 6.1: compiled at the key and tempo shown.
    const auto compiled = RiffCompiler::compile (*riff, playSettings(), RiffDestinations::guitarSummary (processor));
    const auto defaults = MidiExportDefaults::load();
    const auto profile = (forceGeneric || state.dragGeneric) ? MidiProfile::generic : MidiProfile::luthier;

    juce::String error;
    const auto file = RiffDestinations::writeDragFile (*riff, *compiled, profile, RiffLibrary::getDragFolder(),
                                                      defaults.getPpq(), getShownTempo(), &error);

    if (file == juce::File())
        postBanner ("riff-drag", "Could not write " + RiffLibrary::getDragFolder().getFullPathName() + ": " + error, true);

    return file;
}

bool RiffBrowser::saveMidTo (const juce::File& file, bool generic)
{
    const auto made = makeDragFile (generic);

    if (made == juce::File())
        return false;

    const bool ok = made == file || made.copyFileTo (file);
    statusLabel.setText (ok ? "Saved " + file.getFileName() : "Could not save " + file.getFullPathName(),
                         juce::dontSendNotification);
    return ok;
}

bool RiffBrowser::shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails& details,
                                                        juce::StringArray& files, bool& canMoveFiles)
{
    if (! details.description.toString().startsWith ("riff:"))
        return false;

    const auto file = makeDragFile (juce::ModifierKeys::currentModifiers.isAltDown());   // midi-export 4.2: Alt forces Generic

    if (file == juce::File())
        return false;

    files.add (file.getFullPathName());
    canMoveFiles = false;
    statusLabel.setText ("If the host did not take it, the file is in " + file.getParentDirectory().getFullPathName(),
                         juce::dontSendNotification);
    return true;
}

RiffDestinations::TuneInsert RiffBrowser::addToTune()
{
    RiffDestinations::TuneInsert result;
    auto riff = getSelectedRiff();

    if (riff == nullptr)
        return result;

    auto& session = processor.getTuneSession();
    result = RiffDestinations::addToTune (session, *riff, session.getSelectedSection());
    statusLabel.setText (result.message, juce::dontSendNotification);
    return result;
}

RiffDestinations::LooperSend RiffBrowser::sendToLooper()
{
    RiffDestinations::LooperSend result;
    auto riff = getSelectedRiff();

    if (riff == nullptr)
        return result;

    result = RiffDestinations::sendToLooper (processor, *riff, playSettings(), getShownTempo());
    statusLabel.setText (result.message, juce::dontSendNotification);

    if (! result.ok)
        postBanner ("riff-looper", result.message, false);

    return result;
}

bool RiffBrowser::learnIt()
{
    auto riff = getSelectedRiff();

    if (riff == nullptr || previewCompiled == nullptr)
        return false;

    auto* practice = findInTree<PracticePanel> (getTopLevelComponent());

    if (practice == nullptr)
        return false;

    // 6.4: the TAB tab, the riff loaded, looping at 70% with the speed trainer.
    practice->setOpen (true);
    practice->showTool (PracticeTool::tabReader);

    if (auto* reader = findInTree<TabReaderTab> (practice))
        reader->openScore (previewCompiled->toScore (*riff), riff->meta.name);

    state.loop = true;
    state.clockMode = (int) RiffPlayer::ClockMode::own;
    state.tempoMode = 0;
    state.tempoFactor = 0.7;

    suppressCallbacks = true;
    loopToggle.setToggleState (true, juce::dontSendNotification);
    clockBox.setSelectedId (2, juce::dontSendNotification);
    tempoModeBox.setSelectedId (1, juce::dontSendNotification);
    tempoSlider.setRange (0.25, 2.0, 0.05);
    tempoSlider.setValue (0.7, juce::dontSendNotification);
    suppressCallbacks = false;
    saveState();

    audition();

    SpeedTrainer::Settings trainer;
    trainer.phrase = riff->meta.name;
    trainer.startBpm = riff->tempoBpm * 0.7;
    trainer.stepPercent = 5.0;
    trainer.maxBpm = riff->tempoBpm;
    speedTrainer.start (trainer);
    learning = true;
    learnLoopsSeen = 0;

    if (auto* editor = practice->getParentComponent())
        editor->resized();

    return true;
}

void RiffBrowser::tickSpeedTrainer()
{
    if (! learning)
        return;

    if (! isAuditioning())
    {
        learning = false;
        return;
    }

    const int loops = player.getLoopCount();

    while (learnLoopsSeen < loops && speedTrainer.isRunning() && ! speedTrainer.isFinished())
    {
        ++learnLoopsSeen;

        // A pass through the loop counts as clean: audition has no note
        // follower to score it against (decision recorded in FEAT-RIFFS.md).
        const double next = speedTrainer.passCompleted (0);
        const auto* entry = library.findEntry (state.selectedId);

        if (entry != nullptr && entry->tempoBpm > 0.0)
        {
            state.tempoFactor = juce::jlimit (0.25, 2.0, next / entry->tempoBpm);
            player.setTempoFactor (state.tempoFactor);
            tempoSlider.setValue (state.tempoFactor, juce::dontSendNotification);
        }
    }

    learnLoopsSeen = loops;
}

//==============================================================================
juce::Result RiffBrowser::saveRiffFromCapture (const juce::String& name, const juce::String& type,
                                               const juce::String& genre, const juce::StringArray& tags)
{
    Riff riff;
    juce::String error;

    if (! RiffDestinations::riffFromCapture (processor, 2, processor.getHostTempo(), riff, &error))
        return juce::Result::fail (error);

    riff.meta.name = name.trim();
    riff.type = RiffVocabulary::types().contains (type) ? type : riff.type;
    riff.genre = genre;
    riff.meta.tags = tags;

    const auto result = library.saveUserRiff (riff);

    if (result.wasOk())
    {
        refreshList();
        selectRiff (riff.meta.id);
    }

    return result;
}

juce::Result RiffBrowser::importMidiAsRiff (const juce::File& file)
{
    MidiPerformance performance (48000.0);
    const auto read = MidiProfiles::importFromFile (file, performance, 48000.0);

    if (! read.ok)
        return juce::Result::fail (read.error);

    const auto guitar = RiffDestinations::guitarSummary (processor);

    PerformanceScore score;
    auto& track = score.getTrack (0);
    track.numStrings = guitar.numStrings;
    track.tuning = guitar.tuning;
    performance.toScore (score);

    const auto& meta = score.getMeta();
    const double barBeats = meta.timeSignatureNumerator * 4.0 / juce::jmax (1, meta.timeSignatureDenominator);

    Riff riff;
    riff.tuning.assign (guitar.tuning.begin(), guitar.tuning.begin() + guitar.numStrings);
    riff.instrument = guitar.isBass ? (guitar.numStrings >= 6 ? "bass6" : guitar.numStrings == 5 ? "bass5" : "bass4")
                                    : (guitar.numStrings >= 7 ? "guitar7" : "guitar6");
    riff.type = guitar.isBass ? "bass" : "lick";
    riff.tempoBpm = std::round (juce::jlimit (Riff::kMinTempo, Riff::kMaxTempo, meta.tempoBpm));
    riff.meterNumerator = meta.timeSignatureNumerator;
    riff.meterDenominator = meta.timeSignatureDenominator;

    double last = 0.0;
    const double maxBeats = juce::jmin (Riff::kMaxBeats, 8.0 * barBeats);

    for (size_t m = 0; m < score.getTrack (0).measures.size(); ++m)
        for (const auto* n : score.getTrack (0).measures[m].collectNotes())
        {
            auto copy = *n;
            copy.startBeat = std::round ((copy.startBeat + (double) m * barBeats) * 1.0e6) / 1.0e6;

            if (copy.startBeat >= maxBeats || ! juce::isPositiveAndBelow (copy.stringIndex, riff.getNumStrings()))
                continue;

            copy.durationBeats = std::round (juce::jlimit (0.0625, maxBeats - copy.startBeat, copy.durationBeats) * 1.0e6) / 1.0e6;
            copy.fret = juce::jlimit (0, Riff::kMaxFret, copy.fret);
            last = juce::jmax (last, copy.startBeat + copy.durationBeats);
            riff.notes.push_back (copy);
        }

    if (riff.notes.empty())
        return juce::Result::fail ("The file has no notes.");

    riff.lengthBeats = juce::jlimit (barBeats, maxBeats, std::ceil (last / barBeats - 1.0e-9) * barBeats);
    RiffAnalysis::analyse (riff);
    riff.meta.name = file.getFileNameWithoutExtension().substring (0, 60);
    riff.meta.origin = "import";
    riff.genre = "rock";

    const auto result = library.saveUserRiff (riff);

    if (result.wasOk())
    {
        refreshList();
        selectRiff (riff.meta.id);
    }

    return result;
}

juce::Result RiffBrowser::deleteSelectedUserRiff()
{
    if (playingId == state.selectedId)
        stopAudition();

    const auto result = library.deleteUserRiff (state.selectedId);

    if (result.wasOk())
    {
        state.selectedId = {};
        saveState();
        recompilePreview (false, true);
        refreshList();
        updateUserRiffButtons();
    }

    return result;
}

void RiffBrowser::showPlusMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Save riff from the capture...");
    menu.addItem (2, "Import .mid as riff...");

    juce::Component::SafePointer<RiffBrowser> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&saveButton), [safe] (int r)
    {
        if (safe == nullptr)
            return;

        if (r == 1)
            safe->showSaveDialog();

        if (r == 2)
        {
            safe->chooser = std::make_unique<juce::FileChooser> ("Import .mid as riff",
                                                                 juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                                 "*.mid;*.midi");
            safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                        [safe] (const juce::FileChooser& fc)
            {
                if (safe != nullptr && fc.getResult() != juce::File())
                {
                    const auto result = safe->importMidiAsRiff (fc.getResult());

                    if (result.failed())
                        safe->postBanner ("riff-import", result.getErrorMessage(), true);
                }
            });
        }
    });
}

void RiffBrowser::showSaveDialog (std::shared_ptr<const Riff> editing)
{
    auto* w = new juce::AlertWindow (editing != nullptr ? "Edit riff info" : "Save as riff",
                                     editing != nullptr ? juce::String()
                                                        : juce::String ("The capture's marked region, or its last 2 bars, quantised to 1/16. "
                                                                        "Key, tempo, techniques and difficulty are filled in."),
                                     juce::MessageBoxIconType::NoIcon, this);

    w->addTextEditor ("name", editing != nullptr ? editing->meta.name : juce::String(), "Name");

    juce::StringArray types, genres;
    for (const auto& t : RiffVocabulary::types()) types.add (t);
    for (const auto& g : RiffVocabulary::genres()) genres.add (g.displayName);

    w->addComboBox ("type", types, "Type");
    w->addComboBox ("genre", genres, "Genre");
    w->addTextEditor ("tags", editing != nullptr ? editing->meta.tags.joinIntoString (" ") : juce::String(), "Tags");

    if (editing != nullptr)
    {
        w->getComboBoxComponent ("type")->setSelectedItemIndex (juce::jmax (0, RiffVocabulary::types().indexOf (editing->type)));
        w->getComboBoxComponent ("genre")->setSelectedItemIndex (juce::jmax (0, RiffVocabulary::indexOfGenre (editing->genre)));
    }
    else
    {
        w->getComboBoxComponent ("type")->setSelectedItemIndex (processor.getEngine().getGuitarSpec().category == GuitarCategory::Bass ? 3 : 1);
    }

    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<RiffBrowser> safe (this);

    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, editing] (int result)
    {
        if (safe == nullptr || result != 1)
            return;

        const auto name = w->getTextEditorContents ("name").trim();
        const auto type = RiffVocabulary::types()[w->getComboBoxComponent ("type")->getSelectedItemIndex()];
        const auto genre = juce::String (RiffVocabulary::genres()[(size_t) juce::jmax (0, w->getComboBoxComponent ("genre")->getSelectedItemIndex())].id);
        auto tags = juce::StringArray::fromTokens (w->getTextEditorContents ("tags").toLowerCase(), " ,", "");
        tags.removeEmptyStrings();

        if (name.isEmpty())
        {
            safe->postBanner ("riff-save", "A riff needs a name.", true);
            return;
        }

        juce::Result saved = juce::Result::ok();

        if (editing != nullptr)
        {
            Riff riff = *editing;
            riff.meta.name = name;
            riff.type = type;
            riff.genre = genre;
            riff.meta.tags = tags;
            saved = safe->library.saveUserRiff (riff);
            safe->refreshList();
            safe->recompilePreview (false, true);
        }
        else
        {
            saved = safe->saveRiffFromCapture (name, type, genre, tags);
        }

        if (saved.failed())
            safe->postBanner ("riff-save", saved.getErrorMessage(), true);
    }), true);
}

void RiffBrowser::updateUserRiffButtons()
{
    const auto* e = library.findEntry (state.selectedId);
    const bool user = e != nullptr && ! e->factory;

    for (auto* b : { &editInfoButton, &duplicateButton, &revealButton, &deleteButton })
        b->setVisible (user && ! compact);

    resized();
}

//==============================================================================
void RiffBrowser::updateStatus()
{
    const bool loaded = library.isIndexLoaded();

    if (! loaded)
    {
        countLabel.setText (library.isLoading() ? "Loading riffs..." : juce::String(), juce::dontSendNotification);
        hintLabel.setVisible (false);
        clearFiltersButton.setVisible (false);
        return;
    }

    countLabel.setText (juce::String (library.getNumEntries()) + " riffs - " + juce::String ((int) rows.size()) + " shown",
                        juce::dontSendNotification);

    // 7.5: empty states.
    bool hint = false, clear = false;

    if (rows.empty())
    {
        hint = true;
        bool anyUser = false;

        for (int i = 0; i < library.getNumEntries(); ++i)
            anyUser = anyUser || ! library.getEntry (i)->factory;

        if (state.query.userOnly && ! anyUser)
        {
            hintLabel.setText ("Play something, mark it in the capture, then + Save riff.", juce::dontSendNotification);
        }
        else
        {
            hintLabel.setText ("No riffs match.", juce::dontSendNotification);
            clear = true;
        }
    }

    hintLabel.setVisible (hint);
    clearFiltersButton.setVisible (clear);

    // Banners: the missing folder, unreadable user riffs.
    juce::String banner;

    if (library.isFactoryMissing())
        banner = "Factory riffs not found. Reinstall or check Options -> File Locations.";
    else if (library.getUnreadableFiles().size() > 0)
        banner = juce::String (library.getUnreadableFiles().size()) + " riffs could not be read";

    bannerLabel.setText (banner, juce::dontSendNotification);
    bannerLabel.setVisible (banner.isNotEmpty());
    bannerRevealButton.setVisible (! library.isFactoryMissing() && library.getUnreadableFiles().size() > 0);

    // Placement notices go in the status line, not banners.
    if (previewCompiled != nullptr)
        statusLabel.setText (previewCompiled->notices.joinIntoString ("; "), juce::dontSendNotification);

    resized();
}

bool RiffBrowser::pollLibrary()
{
    if (library.isIndexLoaded() && ! indexSeen)
    {
        indexSeen = true;

        if (library.isFactoryMissing())
            ErrorLog::write (ErrorLog::Severity::warn, "Content", "RIFFS_MISSING",
                             "Factory riffs not found", juce::var (library.getFactoryFolder().getFullPathName()));

        if (library.getUnreadableFiles().size() > 0)
            ErrorLog::write (ErrorLog::Severity::warn, "Content", "RIFFS_UNREADABLE",
                             juce::String (library.getUnreadableFiles().size()) + " riffs could not be read",
                             juce::var (library.getUnreadableFiles().joinIntoString ("\n")));

        // An unknown id on restore selects nothing, silently (8).
        if (library.findEntry (state.selectedId) == nullptr)
            state.selectedId = {};

        refreshList();

        if (state.selectedId.isNotEmpty())
        {
            recompilePreview (false, true);
            updateUserRiffButtons();
        }

        updateStatus();
    }

    if (! library.isIndexLoaded())
    {
        if (library.isLoading() != loadingShown)
        {
            loadingShown = library.isLoading();
            updateStatus();
        }

        return false;
    }

    return true;
}

void RiffBrowser::timerCallback()
{
    // A browser nobody can see does no work (the drawer while closed, the tab
    // while another is showing).
    if (! isShowing())
        return;

    if (! pollLibrary())
        return;

    // A guitar swap recompiles the audition for the new strings at once.
    const auto guitar = RiffDestinations::guitarSummary (processor);

    if (! (guitar == lastGuitar) && previewCompiled != nullptr)
    {
        lastGuitar = guitar;
        recompilePreview (true, true);

        if (state.query.fitsInstrument)
            refreshList();
    }

    const bool mine = auditionLatched && playingId == state.selectedId;
    tabView.setPlayhead (mine ? player.getBeatPosition() : -1.0, player.getPositionStamp());

    // Play / Stop / waiting.
    juce::String text = "Play";

    if (auditionLatched && player.isWaiting())
        text = AccessibilitySettings::get().isReducedMotion() ? "waiting" : "Stop";
    else if (auditionLatched && player.isPlaying())
        text = "Stop";

    if (playButton.getButtonText() != text)
        playButton.setButtonText (text);

    if (auditionLatched && player.isWaiting() && ! AccessibilitySettings::get().isReducedMotion()
        && AnimationPolicy::get().mayAnimate (AnimationPolicy::Decorative))
        playButton.setAlpha (0.6f + 0.4f * (float) std::abs (std::sin (juce::Time::getMillisecondCounter() * 0.006)));
    else
        playButton.setAlpha (1.0f);

    if (auditionLatched && ! isAuditioning())
    {
        auditionLatched = false;
        list.repaint();
    }

    tickSpeedTrainer();
}

//==============================================================================
juce::String RiffBrowser::getRowName (int row) const
{
    const auto* e = getRowEntry (row);

    if (e == nullptr)
        return {};

    return e->describe() + (library.isFavourite (e->id) ? ", favourite" : "");
}

juce::String RiffBrowser::getTooltipForRow (int row)
{
    return getRowName (row);
}

void RiffBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    const auto* e = getRowEntry (row);

    if (e == nullptr)
        return;

    if (selected)
        g.fillAll (Palette::accentDim.withAlpha (0.35f));

    auto area = juce::Rectangle<int> (0, 0, width, height).reduced (6, 2);

    // Star, and a speaker while it plays.
    g.setFont (Fonts::ui (13.0f, true));
    g.setColour (library.isFavourite (e->id) ? Palette::accent : Palette::textDisabled);
    g.drawText ("*", area.removeFromLeft (12), juce::Justification::centredLeft);

    if (auditionLatched && playingId == e->id)
    {
        g.setColour (Palette::secondary);
        g.drawText (juce::String::fromUTF8 ("\xe2\x96\xb6"), area.removeFromLeft (14), juce::Justification::centredLeft);
    }

    auto top = area.removeFromTop (area.getHeight() / 2);
    g.setColour (Palette::textPrimary);
    g.drawText (e->name, top.removeFromLeft (juce::jmax (80, top.getWidth() - (compact ? 60 : 110))), juce::Justification::centredLeft, true);
    g.setColour (Palette::textMuted);
    g.setFont (Fonts::ui (11.0f));
    g.drawText (e->type + "  " + e->keyRoot, top, juce::Justification::centredRight, true);

    // Tempo, difficulty pips with the number, technique glyphs.
    g.drawText (juce::String (juce::roundToInt (e->tempoBpm)), area.removeFromLeft (28), juce::Justification::centredLeft);

    auto pips = area.removeFromLeft (52);

    for (int d = 0; d < 5; ++d)
    {
        g.setColour (d < e->difficulty ? Palette::accent : Palette::edge);
        g.fillRect (pips.getX() + d * 7, pips.getCentreY() - 3, 5, 6);
    }

    g.setColour (Palette::textMuted);
    g.drawText (juce::String (e->difficulty), pips.withTrimmedLeft (36), juce::Justification::centredLeft);

    juce::StringArray glyphs;

    for (const auto& t : e->techniques)
        glyphs.addIfNotAlreadyThere (techGlyph (t));

    glyphs.removeEmptyStrings();
    g.drawText (glyphs.joinIntoString (" "), area, juce::Justification::centredLeft, true);
}

void RiffBrowser::selectedRowsChanged (int lastRowSelected)
{
    if (lastRowSelected >= 0)
        selectRow (lastRowSelected);
}

void RiffBrowser::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    selectRow (row);
    audition();
}

void RiffBrowser::returnKeyPressed (int lastRowSelected)
{
    selectRow (lastRowSelected);
    toggleAudition();
}

juce::var RiffBrowser::getDragSourceDescription (const juce::SparseSet<int>& selectedRows)
{
    if (selectedRows.isEmpty())
        return {};

    const auto* e = getRowEntry (selectedRows[0]);
    return e != nullptr ? juce::var ("riff:" + e->id) : juce::var();
}

//==============================================================================
bool RiffBrowser::keyPressed (const juce::KeyPress& key)
{
    const bool typing = searchBox.hasKeyboardFocus (true);

    if (key == juce::KeyPress::escapeKey && onCloseRequested)
    {
        onCloseRequested();
        return true;
    }

    // Ctrl+E: Save .mid... while the browser has focus (6.1).
    if (key == juce::KeyPress ('e', juce::ModifierKeys::commandModifier, 0))
    {
        dragTile.triggerClick();
        return true;
    }

    if (typing)
        return false;

    if (key == juce::KeyPress::spaceKey || key == juce::KeyPress::returnKey)
    {
        toggleAudition();
        return true;
    }

    // Up and Down while playing: the next riff, auditioned.
    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
    {
        const int row = juce::jlimit (0, (int) rows.size() - 1,
                                      getSelectedRow() + (key == juce::KeyPress::downKey ? 1 : -1));
        selectRow (row);
        return true;
    }

    // A letter jumps to the next name starting with it.
    const auto c = key.getTextCharacter();

    if (juce::CharacterFunctions::isLetterOrDigit (c) && ! key.getModifiers().isCommandDown())
    {
        const int start = getSelectedRow();

        for (int k = 1; k <= (int) rows.size(); ++k)
        {
            const int row = (start + k) % (int) rows.size();
            const auto* e = getRowEntry (row);

            if (e != nullptr && e->name.startsWithIgnoreCase (juce::String::charToString (c)))
            {
                selectRow (row);
                list.scrollToEnsureRowIsOnscreen (row);
                return true;
            }
        }
    }

    return false;
}

//==============================================================================
int RiffBrowser::getPreferredHeight() const
{
    return compact ? 560 : 600;
}

void RiffBrowser::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6
    g.fillAll (Palette::background);

    if (compact)
    {
        g.setColour (Palette::edge);
        g.drawRect (getLocalBounds());
    }
}

void RiffBrowser::resized()
{
    auto b = getLocalBounds().reduced (Metrics::gridHalf);
    const int row = compact ? 26 : Metrics::buttonHeight - 2;
    const int gap = Metrics::gridHalf;

    auto takeRow = [&b, row, gap]
    {
        auto r = b.removeFromTop (row);
        b.removeFromTop (gap);
        return r;
    };

    // ---- status (bottom) -------------------------------------------------------------
    {
        auto status = b.removeFromBottom (18);
        countLabel.setBounds (status.removeFromLeft (compact ? status.getWidth() : 170));
        statusLabel.setBounds (status);
        b.removeFromBottom (gap);
    }

    if (bannerLabel.isVisible())
    {
        auto banner = b.removeFromBottom (22);
        bannerRevealButton.setBounds (banner.removeFromRight (bannerRevealButton.isVisible() ? 70 : 0));
        bannerLabel.setBounds (banner);
        b.removeFromBottom (gap);
    }

    // ---- header ----------------------------------------------------------------------
    if (compact)
    {
        searchBox.setBounds (takeRow());
        {
            auto r = takeRow();
            genreBox.setBounds (r.removeFromLeft (r.getWidth() / 2 - 2));
            r.removeFromLeft (4);
            typeBox.setBounds (r);
        }
        {
            auto r = takeRow();
            const int w = r.getWidth() / juce::jmax (1, difficultyChips.size());

            for (auto* chip : difficultyChips)
                chip->setBounds (r.removeFromLeft (w).reduced (1, 0));
        }
    }
    else
    {
        {
            auto r = takeRow();
            titleLabel.setBounds (r.removeFromLeft (64));
            saveButton.setBounds (r.removeFromRight (96));
            r.removeFromRight (gap);
            fitsToggle.setBounds (r.removeFromRight (juce::jmin (160, r.getWidth() / 3)));
            r.removeFromRight (gap);
            searchBox.setBounds (r);
        }
        {
            auto r = takeRow();
            const int count = genreChips.size() + 1;
            const int w = r.getWidth() / count;

            for (auto* chip : genreChips)
                chip->setBounds (r.removeFromLeft (w).reduced (1, 0));

            userChip.setBounds (r.reduced (1, 0));
        }
        {
            auto r = takeRow();
            const int w = r.getWidth();
            typeBox.setBounds (r.removeFromLeft (w * 12 / 100));
            difficultyLabel.setBounds (r.removeFromLeft (w * 8 / 100));
            difficultySlider.setBounds (r.removeFromLeft (w * 13 / 100));
            tempoFilterLabel.setBounds (r.removeFromLeft (w * 8 / 100));
            tempoFilterSlider.setBounds (r.removeFromLeft (w * 15 / 100));
            keyFilterBox.setBounds (r.removeFromLeft (w * 12 / 100).reduced (1, 0));
            techButton.setBounds (r.removeFromLeft (w * 10 / 100).reduced (1, 0));
            favFilter.setBounds (r.removeFromLeft (w * 9 / 100).reduced (1, 0));
            sortBox.setBounds (r.reduced (1, 0));
        }
    }

    // ---- list and preview ------------------------------------------------------------
    juce::Rectangle<int> listArea, preview;

    if (compact)
    {
        listArea = b.removeFromTop (list.getRowHeight() * 8);
        b.removeFromTop (gap);
        preview = b;
    }
    else if (b.getWidth() >= 640)
    {
        listArea = b.removeFromLeft (juce::roundToInt (b.getWidth() * state.previewSplit * 0.9));
        b.removeFromLeft (Metrics::grid);
        preview = b;
    }
    else
    {
        // 7.2: below 640 points the preview stacks under the list.
        listArea = b.removeFromTop (b.getHeight() * 2 / 5);
        b.removeFromTop (gap);
        preview = b;
    }

    list.setBounds (listArea);

    if (hintLabel.isVisible())
    {
        auto hint = listArea.withSizeKeepingCentre (listArea.getWidth(), 48);
        hintLabel.setBounds (hint.removeFromTop (24));
        clearFiltersButton.setBounds (hint.withSizeKeepingCentre (110, 22));
        hintLabel.toFront (false);
        clearFiltersButton.toFront (false);
    }

    auto pRow = [&preview, row, gap]
    {
        auto r = preview.removeFromTop (row);
        preview.removeFromTop (gap);
        return r;
    };

    {
        auto r = pRow();
        favButton.setBounds (r.removeFromRight (row));
        nameLabel.setBounds (r);
    }

    if (! compact)
    {
        infoLabel.setBounds (pRow().withHeight (18));
        const int tabHeight = juce::jlimit (70, 150, preview.getHeight() - 7 * (row + gap));
        tabView.setBounds (preview.removeFromTop (tabHeight));
        preview.removeFromTop (gap);
    }

    {
        auto r = pRow();
        playButton.setBounds (r.removeFromLeft (72));
        r.removeFromLeft (gap);
        loopToggle.setBounds (r.removeFromLeft (70));

        if (compact)
            keyBox.setBounds (r);
        else
            startBox.setBounds (r.removeFromLeft (juce::jmin (140, r.getWidth())));
    }

    if (! compact)
    {
        auto r = pRow();
        keyBox.setBounds (r.removeFromLeft (r.getWidth() / 3));
        mapScaleToggle.setBounds (r.removeFromLeft (r.getWidth() / 2));
        scaleBox.setBounds (r);
    }

    {
        auto r = pRow();
        clockBox.setBounds (r.removeFromLeft (r.getWidth() / 3).reduced (1, 0));
        tempoModeBox.setBounds (r.removeFromLeft (r.getWidth() / 3).reduced (1, 0));
        tempoSlider.setBounds (r);
    }

    if (! compact)
    {
        auto r = pRow();
        levelLabel.setBounds (r.removeFromLeft (44));
        levelSlider.setBounds (r);
    }

    {
        auto r = pRow();
        const int n = compact ? 3 : 4;
        const int w = r.getWidth() / n;
        dragTile.setBounds (r.removeFromLeft (w).reduced (1, 0));

        if (! compact)
            addToTuneButton.setBounds (r.removeFromLeft (w).reduced (1, 0));

        looperButton.setBounds (r.removeFromLeft (w).reduced (1, 0));
        learnButton.setBounds (r.reduced (1, 0));
    }

    if (! compact)
    {
        auto r = pRow();
        dragAsLabel.setBounds (r.removeFromLeft (60));
        dragAsLuthier.setBounds (r.removeFromLeft (76).reduced (1, 0));
        dragAsGeneric.setBounds (r.removeFromLeft (76).reduced (1, 0));

        auto u = pRow();
        const int w = u.getWidth() / 4;

        for (auto* btn : { &editInfoButton, &duplicateButton, &revealButton, &deleteButton })
            btn->setBounds (u.removeFromLeft (w).reduced (1, 0));
    }
}

} // namespace luthier
