/*  riff-library.md 16: the riff library's GUI and destination tests, under
    xvfb, in the style of EditorTests.cpp - and the column-4 tab strip's
    overflow (FEAT-RIFFS). */

#include "TestFramework.h"

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/EasyPanel.h"
#include "../UI/PracticePanel.h"
#include "../UI/RiffBrowser.h"
#include "../UI/UiPreferences.h"
#include "../UI/WorkspaceTabStrip.h"
#include "../Riffs/RiffDestinations.h"
#include "../Tune/TuneTemplates.h"
#include "../Accessibility/Accessibility.h"

#include <set>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    struct PreservedPreferences
    {
        PreservedPreferences()
            : file (UiPreferences::getConfigFile()),
              existed (file.existsAsFile()),
              contents (existed ? file.loadFileAsString() : juce::String())
        {
        }

        ~PreservedPreferences()
        {
            if (existed)
                file.replaceWithText (contents);
            else
                file.deleteFile();
        }

        juce::File file;
        bool existed;
        juce::String contents;
    };

    juce::File scratch (const juce::String& name)
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                     .getChildFile ("LuthierRiffPanelTests").getChildFile (name);
        dir.deleteRecursively();
        dir.createDirectory();
        return dir;
    }

    juce::File factoryFolder()
    {
        auto folder = RiffLibrary::getDefaultFactoryFolder();

        if (! folder.getChildFile ("catalog.json").existsAsFile())
            folder = juce::File (__FILE__).getParentDirectory().getParentDirectory().getParentDirectory()
                       .getChildFile ("Resources").getChildFile ("Riffs");

        return folder;
    }

    /** A processor whose riff library reads the factory riffs and a scratch
        user folder, never the user's own Documents. */
    std::unique_ptr<LuthierAudioProcessor> makeProcessor (const juce::String& name, bool advanced = true)
    {
        auto processor = std::make_unique<LuthierAudioProcessor>();
        processor->prepareToPlay (kSr, kBlock);
        const auto user = scratch (name);
        processor->getRiffLibrary().setFolders (factoryFolder(), user.getChildFile ("User"), user.getChildFile ("library.json"));
        processor->getUiState().advancedMode = advanced;
        return processor;
    }

    template <typename T>
    T* findOne (juce::Component& root)
    {
        if (auto* match = dynamic_cast<T*> (&root))
            return match;

        for (auto* child : root.getChildren())
            if (auto* found = findOne<T> (*child))
                return found;

        return nullptr;
    }

    void render (LuthierAudioProcessor& processor, int blocks)
    {
        juce::AudioBuffer<float> buffer (processor.getTotalNumOutputChannels(), kBlock);

        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            buffer.clear();
            processor.processBlock (buffer, midi);
        }
    }

    int rowOf (RiffBrowser& browser, const juce::String& id)
    {
        for (int r = 0; r < browser.getNumListRows(); ++r)
            if (browser.getRowEntry (r)->id == id)
                return r;

        return -1;
    }

    /** Every visible child inside the component's bounds. */
    void checkNothingClips (TestContext& ctx, juce::Component& c, const juce::String& where)
    {
        for (auto* child : c.getChildren())
        {
            if (! child->isVisible() || child->getWidth() <= 0)
                continue;

            CHECK_MSG (c.getLocalBounds().contains (child->getBounds()),
                       where + ": " + child->getTitle() + " " + child->getBounds().toString()
                         + " is outside " + c.getLocalBounds().toString());
        }
    }
}

//==============================================================================
// FEAT-RIFFS (c): the column-4 strip at 1200 and 1920 wide - every tab
// reachable, the selected one whole on screen, nothing clipped or overlapping.
LUTHIER_TEST (RiffPanel, workspaceTabStripOverflowsCleanlyAt1200And1920)
{
    PreservedPreferences preserved;
    auto processor = makeProcessor ("strip");

    for (const int width : { 1200, 1920 })
    {
        auto panel = std::make_unique<AdvancedPanel> (*processor);
        panel->setSize (width, 800);

        auto& strip = panel->getWorkspaceTabStrip();
        const int count = panel->getNumWorkspaceTabs();
        const auto label = juce::String (width) + " px: ";

        CHECK (strip.getNumTabs() == count);
        CHECK_MSG (strip.getMenuItems().size() == count, label + "the overflow menu does not list every tab");

        for (int i = 0; i < count; ++i)
            CHECK (strip.getMenuItems()[i] == panel->getWorkspaceTabName (i));

        for (int tab = 0; tab < count; ++tab)
        {
            panel->setWorkspaceTab (tab);
            CHECK_MSG (strip.isTabVisible (tab), label + panel->getWorkspaceTabName (tab) + " is not on screen when selected");

            // Visible tabs: whole, wide enough for their label, not overlapping.
            juce::Array<juce::Rectangle<int>> shown;

            for (int i = 0; i < count; ++i)
            {
                if (! strip.isTabVisible (i))
                    continue;

                auto* button = dynamic_cast<juce::Button*> (strip.getChildComponent (i));
                CHECK (button != nullptr);

                if (button == nullptr)
                    continue;

                CHECK_MSG (button->getWidth() + 1 >= juce::jmin (strip.getNaturalWidth (i), strip.getWidth()),
                           label + button->getButtonText() + " is narrower than its label");

                for (const auto& other : shown)
                    CHECK_MSG (! other.intersects (button->getBounds()), label + button->getButtonText() + " overlaps a tab");

                shown.add (button->getBounds());
            }

            if (strip.isOverflowing())
            {
                CHECK (strip.getMenuButton().isVisible());
                CHECK (strip.getLeftArrow().isVisible() && strip.getRightArrow().isVisible());

                for (const auto& r : shown)
                    CHECK (! r.intersects (strip.getMenuButton().getBounds())
                           && ! r.intersects (strip.getLeftArrow().getBounds())
                           && ! r.intersects (strip.getRightArrow().getBounds()));
            }
        }

        // The arrows reach both ends; the menu reaches any tab.
        if (strip.isOverflowing())
        {
            panel->setWorkspaceTab (0);

            for (int k = 0; k < count; ++k)
                strip.getRightArrow().onClick();

            CHECK_MSG (strip.isTabVisible (count - 1), label + "the right arrow never reaches the last tab");

            strip.chooseFromMenu (count / 2);
            CHECK_MSG (panel->getWorkspaceTab() == count / 2, label + "the menu did not select its tab");
            CHECK (strip.isTabVisible (count / 2));

            // Keyboard reachable: the controls take focus (an arrow at its end is
            // disabled, and JUCE gives a disabled control no focus) and have names;
            // the menu, which reaches every tab, is always enabled.
            CHECK (strip.getMenuButton().isEnabled() && strip.getMenuButton().getWantsKeyboardFocus());

            for (auto* b : { &strip.getLeftArrow(), &strip.getRightArrow(), &strip.getMenuButton() })
            {
                CHECK (b->getWantsKeyboardFocus() || ! b->isEnabled());
                CHECK (b->getTitle().isNotEmpty());
            }
        }
    }

    // With more tabs than fit at 1200, the strip must scroll rather than squash.
    {
        auto panel = std::make_unique<AdvancedPanel> (*processor);
        panel->setSize (1200, 800);
        auto& strip = panel->getWorkspaceTabStrip();
        int natural = 0;

        for (int i = 0; i < strip.getNumTabs(); ++i)
            natural += strip.getNaturalWidth (i) + Metrics::gridHalf;

        CHECK (strip.isOverflowing() == (natural - Metrics::gridHalf > strip.getWidth()));
    }

    // The remembered tab is remembered by name.
    {
        auto panel = std::make_unique<AdvancedPanel> (*processor);
        panel->setSize (1200, 800);
        CHECK (panel->setWorkspaceTabNamed ("RIFFS"));

        auto again = std::make_unique<AdvancedPanel> (*processor);
        again->setSize (1200, 800);
        CHECK (again->getWorkspaceTabName (again->getWorkspaceTab()) == "RIFFS");
    }
}

//==============================================================================
// RL-25: the RIFFS tab, between TUNE and LIVE; R; the Easy drawer.
LUTHIER_TEST (RiffPanel, riffsTabAndDrawerAreReachable)
{
    PreservedPreferences preserved;

    {
        auto processor = makeProcessor ("reach-advanced", true);
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
        CHECK (editor != nullptr);

        if (editor == nullptr)
            return;

        editor->setVisible (true);
        editor->setSize (1280, 800);

        auto* panel = findOne<AdvancedPanel> (*editor);
        CHECK (panel != nullptr);

        if (panel == nullptr)
            return;

        int tune = -1, riffs = -1, live = -1;

        for (int i = 0; i < panel->getNumWorkspaceTabs(); ++i)
        {
            if (panel->getWorkspaceTabName (i) == "TUNE")  tune = i;
            if (panel->getWorkspaceTabName (i) == "RIFFS") riffs = i;
            if (panel->getWorkspaceTabName (i) == "LIVE")  live = i;
        }

        CHECK (riffs == tune + 1 && live == riffs + 1);

        panel->setWorkspaceTabNamed ("LIVE");
        const auto* binding = AccessibilitySettings::get().findShortcut ("riffs");
        CHECK (binding != nullptr && binding->key == juce::KeyPress ('r'));
        CHECK (editor->keyPressed (juce::KeyPress ('r')));
        CHECK (panel->getWorkspaceTabName (panel->getWorkspaceTab()) == "RIFFS");

        auto* browser = panel->getRiffsPanel();
        CHECK (browser != nullptr && browser->isVisible() && panel->getWorkspacePanel (panel->getWorkspaceTab()) == browser);

        browser->ensureLibraryLoaded (true);
        CHECK (browser->getNumListRows() > 0);

        // Nothing clips at 1280 x 800, nor at the minimum size.
        checkNothingClips (ctx, *browser, "RIFFS at 1280x800");

        editor->setSize (LuthierAudioProcessorEditor::minimumWidth, LuthierAudioProcessorEditor::minimumHeight);
        checkNothingClips (ctx, *browser, "RIFFS at the minimum size");
    }

    {
        auto processor = makeProcessor ("reach-easy", false);
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());

        if (editor == nullptr)
            return;

        editor->setVisible (true);
        editor->setSize (1280, 800);

        auto* easy = findOne<EasyPanel> (*editor);
        CHECK (easy != nullptr);

        if (easy == nullptr)
            return;

        CHECK (! easy->isRiffDrawerOpen());
        CHECK (editor->keyPressed (juce::KeyPress ('r')));
        CHECK (easy->isRiffDrawerOpen());
        CHECK (editor->keyPressed (juce::KeyPress ('r')));
        CHECK (! easy->isRiffDrawerOpen());

        easy->getRiffsButton().onClick();
        CHECK (easy->isRiffDrawerOpen());

        auto* drawer = easy->getRiffDrawer();
        CHECK (drawer != nullptr && drawer->isCompact());

        if (drawer == nullptr)
            return;

        // 320 wide, over the rig strip, full height of the main area.
        CHECK (easy->getRiffDrawerBounds().getWidth() == EasyPanel::kRiffDrawerWidth);
        CHECK (easy->getRiffDrawerBounds().getRight() == easy->getRigArea().getRight());

        // No Add to Tune in Easy (gui-integration 0.7).
        CHECK (! drawer->getAddToTuneButton().isVisible());

        drawer->ensureLibraryLoaded (true);
        drawer->setBounds (easy->getRiffDrawerBounds());
        checkNothingClips (ctx, *drawer, "Riff drawer");

        CHECK (drawer->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
        CHECK (! easy->isRiffDrawerOpen());
        CHECK_MSG (easy->getRiffsButton().hasKeyboardFocus (false) || ! editor->isShowing(),
                   "focus did not return to the Riffs button");
    }
}

//==============================================================================
// RL-26: keyboard order, Space, Ctrl+E, and names for everything.
LUTHIER_TEST (RiffPanel, keyboardOrderAndAccessibleNames)
{
    PreservedPreferences preserved;
    auto processor = makeProcessor ("keys");
    RiffBrowser browser (*processor, false);
    browser.setSize (900, 640);
    browser.ensureLibraryLoaded (true);

    const auto order = browser.getFocusOrder();
    CHECK (order.getFirst() == &browser.getSearchBox());

    int listAt = -1, playAt = -1, lastOrder = 0;

    for (int i = 0; i < order.size(); ++i)
    {
        auto* c = order[i];
        CHECK_MSG (c->getExplicitFocusOrder() > lastOrder, "focus order skips at " + juce::String (i));
        lastOrder = c->getExplicitFocusOrder();

        if (c == &browser.getList())       listAt = i;
        if (c == &browser.getPlayButton()) playAt = i;

        CHECK_MSG (c->getTitle().isNotEmpty(), "a control in the tab order has no accessible name (index "
                                                 + juce::String (i) + ")");
    }

    // search, chips, filters, the list, then the preview.
    CHECK (listAt > 0 && playAt > listAt);

    // Rows carry type, key, tempo and difficulty.
    CHECK (browser.getNumListRows() > 0);

    for (int r = 0; r < juce::jmin (40, browser.getNumListRows()); ++r)
    {
        const auto* e = browser.getRowEntry (r);
        const auto name = browser.getRowName (r);
        CHECK (name.contains (e->name) && name.contains (e->type) && name.contains (e->keyRoot)
               && name.contains (juce::String (juce::roundToInt (e->tempoBpm)) + " bpm")
               && name.contains ("difficulty " + juce::String (e->difficulty) + " of 5"));
    }

    // Space toggles audition; the announcement names the riff, key and tempo.
    browser.selectRow (0);
    CHECK (browser.keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    render (*processor, 2);
    CHECK (browser.isAuditioning());
    CHECK (browser.getLastAnnouncement().startsWith ("Playing " + browser.getRowEntry (0)->name + " in "));
    CHECK (browser.getLastAnnouncement().contains (" bpm"));
    CHECK (browser.keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    render (*processor, 2);
    CHECK (! browser.isAuditioning());

    // Ctrl+E is Save .mid... while the browser has focus.
    CHECK (browser.keyPressed (juce::KeyPress ('e', juce::ModifierKeys::commandModifier, 0)));

    // The tab view reads the notes, two bars at a time.
    CHECK (browser.getTabView().getDescription().startsWith ("Bar 1: string "));
}

//==============================================================================
// RL-27: empty and error states, in 7.5's words.
LUTHIER_TEST (RiffPanel, emptyAndErrorStatesShowTheirHints)
{
    PreservedPreferences preserved;

    {
        auto processor = makeProcessor ("empty");
        RiffBrowser browser (*processor, false);
        browser.setSize (900, 640);
        browser.ensureLibraryLoaded (true);

        browser.getSearchBox().setText ("zzzqqq nothing", false);
        browser.getSearchBox().onTextChange();   // the editor posts it; this runner has no message loop
        CHECK (browser.getNumListRows() == 0);
        CHECK (browser.getHintText() == "No riffs match.");
        CHECK (browser.getClearFiltersButton().isVisible());
        CHECK (browser.getClearFiltersButton().getButtonText() == "Clear filters");

        browser.getClearFiltersButton().onClick();
        CHECK (browser.getNumListRows() > 0);

        browser.getState().query.userOnly = true;
        browser.refreshList();
        CHECK (browser.getHintText() == "Play something, mark it in the capture, then + Save riff.");
    }

    {
        auto processor = makeProcessor ("missing");
        const auto nowhere = scratch ("missing-factory").getChildFile ("NotHere");
        processor->getRiffLibrary().setFolders (nowhere, scratch ("missing-user"), scratch ("missing-user").getChildFile ("library.json"));

        RiffBrowser browser (*processor, false);
        browser.setSize (900, 640);
        browser.ensureLibraryLoaded (true);
        CHECK (browser.getBannerText() == "Factory riffs not found. Reinstall or check Options -> File Locations.");
    }

    {
        auto processor = makeProcessor ("corrupt");
        const auto user = scratch ("corrupt-user");
        const auto bad = user.getChildFile ("user.lick.broken.luthierriff");
        const juce::String junk ("{ \"magic\": \"luthier.riff\", \"schema\": 1, \"meta\": [ half");
        bad.replaceWithText (junk);

        processor->getRiffLibrary().setFolders (factoryFolder(), user, user.getChildFile ("library.json"));
        RiffBrowser browser (*processor, false);
        browser.setSize (900, 640);
        browser.ensureLibraryLoaded (true);

        CHECK (browser.getBannerText() == "1 riffs could not be read");
        CHECK (bad.loadFileAsString() == junk);   // untouched
    }
}

//==============================================================================
// RL-28: view state survives a close and a host save, never enters a preset,
// and a restore leaves audition stopped.
LUTHIER_TEST (RiffPanel, stateSurvivesReopenAndHostRestoreButNotPresets)
{
    PreservedPreferences preserved;
    auto processor = makeProcessor ("state");
    juce::String chosen;

    {
        RiffBrowser browser (*processor, false);
        browser.setSize (900, 640);
        browser.ensureLibraryLoaded (true);

        browser.getGenreChip (1)->setToggleState (true, juce::dontSendNotification);   // Blues
        browser.getGenreChip (1)->onClick();
        browser.selectRow (2);
        chosen = browser.getSelectedId();
        browser.getKeyBox().setSelectedId (6, juce::sendNotificationSync);          // E
        browser.getTempoModeBox().setSelectedId (2, juce::sendNotificationSync);    // bpm
        browser.getState().dragGeneric = true;
        browser.stateChanged (false);
        browser.audition();
        render (*processor, 4);
        CHECK (browser.isAuditioning());
    }

    CHECK (chosen.startsWith ("blues."));

    // Reopen: same view.
    {
        RiffBrowser browser (*processor, false);
        browser.setSize (900, 640);
        browser.ensureLibraryLoaded (true);
        CHECK (browser.getSelectedId() == chosen);
        CHECK (browser.getState().query.genres.test (1));
        CHECK (browser.getState().keyChoice == 5);
        CHECK (browser.getState().tempoMode == 1);
        CHECK (browser.getState().dragGeneric);
    }

    // Host save and restore into a fresh instance.
    juce::MemoryBlock block;
    processor->getStateInformation (block);

    auto restored = makeProcessor ("state-restored");
    restored->setStateInformation (block.getData(), (int) block.getSize());
    render (*restored, 2);
    CHECK (! restored->getEngine().getRiffPlayer().isPlaying());

    {
        RiffBrowser browser (*restored, false);
        browser.setSize (900, 640);
        browser.ensureLibraryLoaded (true);
        CHECK (browser.getSelectedId() == chosen);
        CHECK (browser.getState().keyChoice == 5);
        CHECK (browser.getState().dragGeneric);
    }

    // Never in a preset.
    auto& presets = processor->getPresetManager();
    const auto preset = juce::JSON::toString (presets.toVar ("probe"));
    CHECK (! preset.contains (chosen));
    CHECK (! preset.contains ("dragAs"));
}

//==============================================================================
// RL-19: Add to Tune.
LUTHIER_TEST (RiffPanel, addToTuneKeepsLocksAndIsOneUndo)
{
    PreservedPreferences preserved;
    auto processor = makeProcessor ("tune");
    auto& session = processor->getTuneSession();

    auto tune = TuneTemplateLibrary::createBlank();
    tune.meta.keyTonic = 4;   // E
    tune.arrangement.sections.clear();

    TuneSection verse;
    verse.name = "Verse";
    verse.lengthBars = 2;
    verse.melody = MelodyTrack {};

    auto lockA = MelodyNote::make (0.0, 0.5, 60);
    lockA.locked = true;
    auto lockB = MelodyNote::make (3.0, 1.0, 62);
    lockB.locked = true;
    auto loose = MelodyNote::make (5.0, 1.0, 65);
    verse.melody->notes = { lockA, lockB, loose };
    tune.arrangement.sections.push_back (verse);
    tune.arrangement.setlist.push_back ({ verse.name, 1, {} });
    session.newTune (tune);
    session.setSelectedSection (0);

    const auto before = session.getTune().arrangement.sections[0];
    const int undoBefore = session.getNumUndoSteps();

    auto& library = processor->getRiffLibrary();
    library.loadIndexNow();

    // A two-bar guitar lick in A.
    juce::String lickId, bassId;

    for (int i = 0; i < library.getNumEntries(); ++i)
    {
        const auto* e = library.getEntry (i);

        if (lickId.isEmpty() && e->type == "lick" && e->keyRoot == "A" && e->lengthBeats == 8.0)
            lickId = e->id;

        if (bassId.isEmpty() && e->type == "bass" && e->lengthBeats <= 8.0)
            bassId = e->id;
    }

    CHECK (lickId.isNotEmpty() && bassId.isNotEmpty());
    const auto lick = library.getRiff (lickId);
    const auto result = RiffDestinations::addToTune (session, *lick, 0);

    CHECK_MSG (result.ok, result.message);
    CHECK (session.getNumUndoSteps() == undoBefore + 1);
    CHECK (session.getUndoDescription().contains ("Insert riff " + lick->meta.name));

    const auto& after = session.getTune().arrangement.sections[0];
    int locked = 0;

    for (const auto& n : after.melody->notes)
    {
        if (n.locked)
        {
            ++locked;
            CHECK (n == lockA || n == lockB);   // byte-identical
        }
        else
        {
            CHECK (n.extra.contains ("riff_str") && n.extra.contains ("riff_fret") && n.extra.contains ("riff_tech"));

            // Nothing overlaps a locked note.
            for (const auto& l : { lockA, lockB })
                CHECK (! (n.startBeat < l.getEndBeat() - 1.0e-9 && l.startBeat < n.getEndBeat() - 1.0e-9));
        }
    }

    CHECK (locked == 2);
    CHECK (result.inserted > 0);

    // Transposed to the tune's key: A -> E is down five.
    const auto compiled = RiffCompiler::compile (*lick, {}, GuitarSpecSummary::forRiff (*lick));
    int lowest = 128;

    for (const auto& n : after.melody->notes)
        if (! n.locked)
            lowest = juce::jmin (lowest, n.pitch.value);

    int riffLowest = 128;
    for (const auto& n : compiled->placement.notes)
        riffLowest = juce::jmin (riffLowest, n.midiNote);

    CHECK (lowest == riffLowest - 5 || lowest == riffLowest + 7);

    // One undo restores the section exactly.
    CHECK (session.undo());
    CHECK (session.getTune().arrangement.sections[0] == before);

    // A bass line goes to the BassTrack, which becomes manual.
    const auto bass = library.getRiff (bassId);
    CHECK (RiffDestinations::addToTune (session, *bass, 0).ok);
    CHECK (session.getTune().arrangement.sections[0].bass.mode == BassMode::manual);
    CHECK (! session.getTune().arrangement.sections[0].bass.notes.empty());
}

//==============================================================================
// RL-20: Send to Looper - whole bars at the current tempo, not silent.
LUTHIER_TEST (RiffPanel, sendToLooperMakesAWholeBarLayer)
{
    PreservedPreferences preserved;
    auto processor = makeProcessor ("looper");
    auto& library = processor->getRiffLibrary();
    library.loadIndexNow();

    juce::String id;

    for (int i = 0; i < library.getNumEntries() && id.isEmpty(); ++i)
        if (library.getEntry (i)->type == "riff" && library.getEntry (i)->lengthBeats == 8.0
              && library.getEntry (i)->meterNumerator == 4)
            id = library.getEntry (i)->id;

    const auto riff = library.getRiff (id);
    CHECK (riff != nullptr);

    if (riff == nullptr)
        return;

    const double bpm = 120.0;
    const auto sent = RiffDestinations::sendToLooper (*processor, *riff, {}, bpm);
    CHECK_MSG (sent.ok, sent.message);

    const double expected = riff->getNumBars() * riff->getBeatsPerBar() * 60.0 / bpm * kSr;
    CHECK_NEAR ((double) sent.lengthSamples, expected, 1.0);
    CHECK_NEAR ((double) processor->getLooper().getLoopLengthSamples(), expected, 1.0);

    const auto audio = RiffDestinations::render (*riff, {}, processor->getEngine().getGuitarType(), bpm, kSr, (int) expected);
    CHECK (audio.getMagnitude (0, 0, audio.getNumSamples()) > 1.0e-4f);
}

//==============================================================================
// RL-21: Learn It opens the tab reader, looping at 70%.
LUTHIER_TEST (RiffPanel, learnItOpensTheTabReaderAtSeventyPercent)
{
    PreservedPreferences preserved;
    auto processor = makeProcessor ("learn", true);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());

    if (editor == nullptr)
        return;

    editor->setVisible (true);
    editor->setSize (1280, 800);

    auto* panel = findOne<AdvancedPanel> (*editor);
    CHECK (panel != nullptr && panel->setWorkspaceTabNamed ("RIFFS"));

    auto* browser = panel->getRiffsPanel();
    browser->ensureLibraryLoaded (true);
    browser->selectRow (0);

    CHECK (browser->learnIt());

    auto* practice = findOne<PracticePanel> (*editor);
    CHECK (practice != nullptr && practice->isOpen());
    CHECK (practice->getCurrentTab() == (int) PracticeTool::tabReader);

    auto* reader = findOne<TabReaderTab> (*editor);
    CHECK (reader != nullptr);

    if (reader == nullptr)
        return;

    const auto riff = browser->getSelectedRiff();
    const auto expected = browser->getPreviewCompiled()->toScore (*riff);
    CHECK (reader->getScore().getTotalNoteCount() == expected.getTotalNoteCount());
    CHECK (reader->getScoreTitle() == riff->meta.name);

    // Same notes, in order.
    const auto& got = reader->getScore().getTrack (0);
    const auto& want = expected.getTrack (0);
    CHECK (got.measures.size() == want.measures.size());

    for (size_t m = 0; m < juce::jmin (got.measures.size(), want.measures.size()); ++m)
    {
        const auto a = got.measures[m].collectNotes();
        const auto b = want.measures[m].collectNotes();
        CHECK (a.size() == b.size());

        for (size_t k = 0; k < juce::jmin (a.size(), b.size()); ++k)
            CHECK (a[k]->stringIndex == b[k]->stringIndex && a[k]->fret == b[k]->fret);
    }

    CHECK (browser->getLoopToggle().getToggleState());
    CHECK_NEAR (browser->getState().tempoFactor, 0.7, 1.0e-9);
    CHECK_NEAR (processor->getEngine().getRiffPlayer().getTempoFactor(), 0.7, 1.0e-9);
    CHECK (processor->getEngine().getRiffPlayer().isLooping());
}

//==============================================================================
// RL-31: tune playing, the rhythm engine on, snapshot recalls and Slide Mode
// toggling around a looping riff: no NaN, nothing stuck, no lost note-ons.
LUTHIER_TEST (RiffPanel, riffAuditionSurvivesTheCombination)
{
    PreservedPreferences preserved;
    auto processor = makeProcessor ("combo");
    auto& library = processor->getRiffLibrary();
    library.loadIndexNow();

    // A tune on its own clock.
    auto tune = TuneTemplateLibrary::createBlank();
    tune.meta.tempoBpm = 100.0;
    processor->getTuneSession().newTune (tune);
    processor->getTunePlayer().play();

    processor->getEngine().getRhythmEngine().setEnabled (true);
    processor->captureSnapshot (0, "A");
    processor->captureSnapshot (1, "B");

    juce::String id;
    for (int i = 0; i < library.getNumEntries() && id.isEmpty(); ++i)
        if (library.getEntry (i)->genre == "rock" && library.getEntry (i)->type == "lick")
            id = library.getEntry (i)->id;

    const auto riff = library.getRiff (id);
    auto& player = processor->getEngine().getRiffPlayer();
    player.setCompiled (RiffCompiler::compile (*riff, {}, RiffDestinations::guitarSummary (*processor)), true);
    player.setLooping (true);
    player.play();

    auto* slide = processor->getState().getParameter (ParamIDs::slideMode);

    juce::AudioBuffer<float> buffer (processor->getTotalNumOutputChannels(), kBlock);
    const int blocks = (int) (60.0 * kSr / kBlock);
    const int twoSeconds = (int) (2.0 * kSr / kBlock);
    bool finite = true;
    int loops = 0;

    for (int b = 0; b < blocks; ++b)
    {
        if (b % twoSeconds == 0)
            processor->recallSnapshot ((b / twoSeconds) % 2);

        if (slide != nullptr && b % (3 * twoSeconds) == 0)
            slide->setValueNotifyingHost (slide->getValue() > 0.5f ? 0.0f : 1.0f);

        juce::MidiBuffer midi;
        buffer.clear();
        processor->processBlock (buffer, midi);

        for (int c = 0; c < buffer.getNumChannels() && finite; ++c)
            for (int i = 0; i < kBlock; ++i)
                finite = finite && std::isfinite (buffer.getSample (c, i));

        loops = player.getLoopCount();
    }

    CHECK (finite);
    CHECK (player.isPlaying());
    CHECK (loops > 3);
    CHECK_MSG (player.getOverflowCount() == 0, juce::String (player.getOverflowCount()) + " events dropped");

    player.stop();
    render (*processor, 1);
    CHECK_MSG (player.getSoundingMask() == 0, "a riff note was left sounding");

    for (int s = 0; s < kMaxStrings; ++s)
        CHECK (std::abs (processor->getEngine().getRiffBendCents (s)) < 1.0e-9);

    processor->getTunePlayer().stop();
}
