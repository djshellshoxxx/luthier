/*  The HELP workspace tab and the Help overlay (gui-integration.md 4.4, 16, 17,
    20; include.md "Help File"; accessibility.md 2 and 7).

    Three kinds of check. The content ones read HelpContent and a HelpTab on
    its own: that include.md's list is covered, that every panel a user can ask
    for docs from has a topic, and that the cheat sheet is the registry rather
    than a copy of it. The render writes %TEMP%/luthier-guitar-renders/_help.png
    (column 4 at its narrowest) and _help_wide.png (the overlay's size). The
    last two open the editor and drive F1, the header's ? and the tab's own
    buttons, which is what connects the tab to the window.

    Processors are on the heap throughout: a processor is large, and more than
    one in a frame has overflowed the runner's stack before the first line of a
    test ran.
*/

#include "TestFramework.h"

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../Accessibility/Localisation.h"
#include "../Presets/PresetManager.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/HeaderBar.h"
#include "../UI/HelpContent.h"
#include "../UI/HelpTab.h"
#include "../UI/Overlays.h"
#include "../UI/UiPreferences.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    /** gui-integration 4.4's strip, which the workspace topic and the aliases
        have to keep up with. TECHNIQUES (C-32) joins before HELP when built;
        theWorkspaceTopicNamesEveryTabThatExists catches it from the real strip. */
    const char* const kCanonicalTabs[] =
    {
        "WORKSHOP", "MOD", "RHYTHM", "TUNE", "LIVE", "ROUTING", "TONE MATCH", "CHARACTER",
        "PRACTICE", "NOTATION", "MIDI OUT", "CONTROLLERS", "HELP"
    };

    /*  UiPreferences writes through to the user's real config file, and opening
        tabs writes the last-used one; put it back as it was. */
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

            UiPreferences::get().reset();
            UiPreferences::get().load();
        }

        juce::File file;
        bool existed;
        juce::String contents;
    };

    template <typename T>
    void collect (juce::Component& root, juce::Array<T*>& found)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* match = dynamic_cast<T*> (child))
                found.add (match);

            collect<T> (*child, found);
        }
    }

    template <typename T>
    T* findOne (juce::Component& root)
    {
        juce::Array<T*> found;
        collect<T> (root, found);
        return found.isEmpty() ? nullptr : found.getFirst();
    }

    juce::Button* findButton (juce::Component& root, const juce::String& text)
    {
        juce::Array<juce::Button*> buttons;
        collect<juce::Button> (root, buttons);

        for (auto* button : buttons)
            if (button->getButtonText() == text)
                return button;

        return nullptr;
    }

    /** What a click does, without triggerClick's message (never delivered in a
        console runner). Refuses a disabled button. */
    bool press (juce::Button& button)
    {
        if (! button.isEnabled() || button.onClick == nullptr)
            return false;

        button.onClick();
        return true;
    }

    juce::KeyPress shortcutFor (const char* actionId)
    {
        if (const auto* binding = AccessibilitySettings::get().findShortcut (actionId))
            return binding->key;

        return {};
    }

    const ShortcutRow* rowFor (const std::vector<ShortcutRow>& rows, const juce::String& actionId)
    {
        for (const auto& row : rows)
            if (row.actionId == actionId)
                return &row;

        return nullptr;
    }

    juce::String bodyOf (HelpTab& tab, const char* topic)
    {
        tab.showTopicFor (topic);
        return tab.getBodyText();
    }

    /** Renders into %TEMP%/luthier-guitar-renders like the other panel tests,
        and says whether anything but one flat colour was drawn. */
    bool renderTo (juce::Component& c, const juce::String& fileName)
    {
        juce::Image image (juce::Image::ARGB, c.getWidth(), c.getHeight(), true, juce::SoftwareImageType());

        {
            juce::Graphics g (image);
            g.fillAll (Palette::background);
            c.paintEntireComponent (g, true);
        }

        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-guitar-renders");
        dir.createDirectory();
        const auto png = dir.getChildFile (fileName);
        png.deleteFile();

        {
            juce::FileOutputStream out (png);
            juce::PNGImageFormat().writeImageToStream (image, out);
        }

        if (! png.existsAsFile() || png.getSize() <= 0)
            return false;

        const auto corner = image.getPixelAt (2, 2);

        for (int y = 0; y < image.getHeight(); y += 7)
            for (int x = 0; x < image.getWidth(); x += 7)
                if (image.getPixelAt (x, y) != corner)
                    return true;

        return false;
    }
}

//==============================================================================
/*  include.md's Help File list, item by item. Each check reads the body as the
    tab shows it, so a topic that exists but is never shown would still fail. */
LUTHIER_TEST (HelpTab, theContentCoversWhatIncludeMdAsksFor)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    HelpTab tab (*processor);

    // "explains each feature" and "a general description of the GUI".
    const auto interface = bodyOf (tab, "interface");
    CHECK_MSG (interface.contains ("HEADER") && interface.contains ("EASY MODE")
                 && interface.contains ("ADVANCED MODE"),
               "The Interface does not describe the header and both modes");

    CHECK_MSG (bodyOf (tab, "controls").contains ("Right-click"), "Controls does not describe the control menu");

    // "how to use the VST and the workflow involved".
    CHECK_MSG (bodyOf (tab, "getting-started").contains ("WORKFLOW"), "Getting Started has no workflow");

    // "the version number", on the header row and in About.
    CHECK (HelpTab::getVersionText().contains (JucePlugin_VersionString));

    const auto about = bodyOf (tab, "about");
    CHECK_MSG (about.contains (JucePlugin_VersionString), "About has no version");

    // "the license", and its live state.
    CHECK_MSG (about.contains ("LICENCE") && about.contains ("licensed, not sold"), "About has no licence terms");
    CHECK_MSG (about.contains (License::getStateName (processor->getLicense().getState())),
               "About does not show the licence state");

    // "link to github", "link to homepage", "link to support email": in the text
    // and on a button each.
    CHECK (about.contains (HelpContent::homepageUrl));
    CHECK (about.contains (HelpContent::sourceUrl));
    CHECK (about.contains (HelpContent::supportEmail));
    CHECK (tab.getSourceButton().onClick != nullptr);
    CHECK (tab.getHomepageButton().onClick != nullptr);
    CHECK (tab.getSupportButton().onClick != nullptr);
    CHECK (HelpContent::getSupportMailUrl().toString (true).startsWith (juce::String ("mailto:")
                                                                          + HelpContent::supportEmail));

    // "troubleshooting guide if the installation is broken (explain how to
    // manually install and uninstall)".
    const auto trouble = bodyOf (tab, "troubleshooting");
    CHECK_MSG (trouble.contains ("Luthier.vst3") && trouble.contains ("Common Files"),
               "Troubleshooting does not say where the plugin goes");
    CHECK_MSG (trouble.contains ("UNINSTALL BY HAND"), "Troubleshooting does not explain a manual uninstall");

    // "troubleshooting guide for where to put presets files": the folder on this
    // machine, not a description of the usual one.
    const auto presets = bodyOf (tab, "presets");
    CHECK (presets.contains (".luthierpreset"));
    CHECK_MSG (presets.contains (PresetManager::getUserPresetFolder().getFullPathName()),
               "Presets does not show this machine's preset folder");

    // "a debug button", and what the debug files are for.
    int debugRequests = 0;
    tab.onOpenDebug = [&debugRequests] { ++debugRequests; };
    CHECK (press (tab.getDebugButton()));
    CHECK_MSG (debugRequests == 1, "the debug button does not ask for the debug window");

    const auto debug = bodyOf (tab, "debug");
    CHECK (debug.contains ("Create log file on crash"));
    CHECK (debug.contains ("troubleshooting file"));
    CHECK (debug.contains ("Reset all settings and clear caches"));

    // Every topic has text, and every {key:...} named a real action.
    for (int i = 0; i < HelpContent::getNumTopics(); ++i)
    {
        tab.showTopic (i);
        const auto text = tab.getBodyText();
        const juce::String id (HelpContent::getTopic (i).id);

        CHECK_MSG (text.length() > 80, "the topic \"" + id + "\" is nearly empty");
        CHECK_MSG (! text.contains ("{key:"), "the topic \"" + id + "\" shows a raw placeholder");
        CHECK_MSG (! text.contains ("(not bound)"),
                   "the topic \"" + id + "\" quotes a shortcut the registry does not have");
    }
}

//==============================================================================
/*  gui-integration 16 and 20: Docs on a panel opens Help pinned to that panel.
    Every name a panel can pass has to land somewhere, and on one topic only. */
LUTHIER_TEST (HelpTab, everyPanelNamePinsOneTopic)
{
    for (const auto* name : kCanonicalTabs)
        CHECK_MSG (HelpContent::findTopic (name) >= 0, juce::String ("no help topic for the tab ") + name);

    // Column 1-3 section headings (AdvancedPanel::buildColumn1-3).
    for (const auto* section : { "Temperament", "Body", "Strings", "String Set", "Tuning Realism", "Selected String",
                                 "Neck", "Sympathetic", "Bridge", "Pickups", "Circuit",
                                 "Pedalboard (before the amp)", "Playing Hand", "String Noise", "Amplifier",
                                 "Effects Loop (after the amp)", "Cabinet and Mic", "Room", "Sustain",
                                 "Performance", "Humanise", "Master" })
        CHECK_MSG (HelpContent::findTopic (section) >= 0, juce::String ("no help topic for the section ") + section);

    // Section 5's Options pages, from the real overlay.
    auto processor = std::make_unique<LuthierAudioProcessor>();

    {
        OptionsPanel options (*processor);

        for (const auto& page : options.getPageNames())
            CHECK_MSG (HelpContent::findTopic (page) >= 0, "no help topic for the Options page " + page);
    }

    // No name belongs to two topics: each alias finds its own topic first.
    for (int i = 0; i < HelpContent::getNumTopics(); ++i)
    {
        const auto& topic = HelpContent::getTopic (i);

        CHECK_MSG (HelpContent::findTopic (topic.id) == i, juce::String ("the id ") + topic.id + " is shadowed");

        for (const auto& alias : HelpContent::getAliases (topic))
            CHECK_MSG (HelpContent::findTopic (alias) == i,
                       "\"" + alias + "\" pins " + HelpContent::getTopic (HelpContent::findTopic (alias)).id
                         + ", not " + topic.id);
    }

    // The forms a caller might use agree.
    const int toneMatch = HelpContent::findTopic ("TONE MATCH");
    CHECK (toneMatch >= 0);
    CHECK (HelpContent::findTopic ("tone-match") == toneMatch);
    CHECK (HelpContent::findTopic ("  Tone   Match tab ") == toneMatch);
    CHECK (HelpContent::findTopic ("no such panel") < 0);
    CHECK (HelpContent::findTopic ({}) < 0);
}

//==============================================================================
/*  Pinning from outside and choosing in the list end in the same place, and a
    name nobody answers to leaves Help where it was. */
LUTHIER_TEST (HelpTab, pinningShowsTheTopicAndSelectsIt)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    HelpTab tab (*processor);

    CHECK (tab.getShownTopic() == 0);

    CHECK (tab.showTopicFor ("TONE MATCH"));
    CHECK (tab.getShownTopicId() == "tone-match");
    CHECK (tab.getTopicList().getSelectedRow() == HelpContent::findTopic ("tone-match"));
    CHECK (tab.getBodyText().contains ("IR slots"));

    CHECK (tab.showTopicFor ("Cabinet and Mic"));
    CHECK (tab.getShownTopicId() == "amplification");

    CHECK_MSG (! tab.showTopicFor ("A PANEL WITH NO DOCS"), "an unknown name reported a pin");
    CHECK_MSG (tab.getShownTopicId() == "amplification", "an unknown name moved the topic");

    // A click in the list: the model's callback, as the ListBox makes it.
    const int notation = HelpContent::findTopic ("NOTATION");
    tab.getTopicList().selectRow (notation);
    CHECK (tab.getShownTopic() == notation);
    CHECK (tab.getBodyText().contains ("MusicXML"));
}

//==============================================================================
/*  4.4 and accessibility 7: the cheat sheet is a live view of the bindings.
    A rebind reaches the sheet and the prose through the registry's change
    message, and a reset takes it back out. */
LUTHIER_TEST (HelpTab, aRebindShowsUpInTheCheatSheetAndTheText)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    HelpTab tab (*processor);

    auto& settings = AccessibilitySettings::get();
    settings.resetShortcut ("panic");
    settings.dispatchPendingMessages();

    // Every binding once, with its catalog description rather than its key.
    for (const auto& binding : settings.getShortcuts())
    {
        int seen = 0;

        for (const auto& row : tab.getShownShortcuts())
            if (row.actionId == binding.id)
                ++seen;

        CHECK_MSG (seen == 1, "\"" + binding.id + "\" is on the sheet " + juce::String (seen) + " times");
    }

    for (const auto& row : tab.getShownShortcuts())
    {
        CHECK_MSG (row.description.isNotEmpty() && ! row.description.contains ("accessibility.shortcut."),
                   "a sheet row shows a raw catalog key: " + row.description);
        CHECK (row.keyText.isNotEmpty());
    }

    // The keys that are not in the registry are listed too, and say so.
    int fixed = 0;
    for (const auto& row : tab.getShownShortcuts())
        if (row.fixed)
            ++fixed;

    CHECK_MSG (fixed == 3, "expected Escape and the two digit rows as fixed keys, found " + juce::String (fixed));

    const auto* panic = rowFor (tab.getShownShortcuts(), "panic");
    CHECK (panic != nullptr && ! panic->rebound);

    const auto defaultKey = settings.findShortcut ("panic")->defaultKey.getTextDescription();

    // --- rebind ------------------------------------------------------------------------
    const juce::KeyPress newKey ('k', juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier, 0);
    CHECK_MSG (settings.rebind ("panic", newKey), "the test key is already taken");
    settings.dispatchPendingMessages();

    panic = rowFor (tab.getShownShortcuts(), "panic");
    CHECK_MSG (panic != nullptr && panic->keyText == newKey.getTextDescription(),
               "the sheet still shows " + (panic != nullptr ? panic->keyText : juce::String ("nothing"))
                 + " for Panic after the rebind");
    CHECK (panic != nullptr && panic->rebound);
    CHECK_MSG (tab.getShortcutText().contains (tr ("accessibility.shortcut.panic") + "  (rebound)"),
               "the rebound shortcut is not marked in words");

    // The prose quotes the binding too: Troubleshooting says which key panics.
    CHECK (tab.showTopicFor ("troubleshooting"));
    CHECK_MSG (tab.getBodyText().contains ("Press Panic (" + newKey.getTextDescription() + ")"),
               "the troubleshooting text still quotes the old Panic key");

    // --- reset -------------------------------------------------------------------------
    settings.resetShortcut ("panic");
    settings.dispatchPendingMessages();

    panic = rowFor (tab.getShownShortcuts(), "panic");
    CHECK (panic != nullptr && ! panic->rebound && panic->keyText == defaultKey);
    CHECK (tab.getBodyText().contains ("Press Panic (" + defaultKey + ")"));

    // --- the backstop: a change the tab missed while hidden --------------------------
    tab.setVisible (false);
    settings.rebind ("panic", newKey);
    tab.setVisible (true);

    panic = rowFor (tab.getShownShortcuts(), "panic");
    CHECK_MSG (panic != nullptr && panic->rebound, "showing the tab did not re-read the registry");

    settings.resetShortcut ("panic");
    settings.dispatchPendingMessages();
}

//==============================================================================
/*  accessibility 2: the shortcut list filters by search. The box's own change
    message is asynchronous, so the test calls the callback the box would. */
LUTHIER_TEST (HelpTab, theSearchFiltersTheSheet)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    HelpTab tab (*processor);

    const auto all = tab.getShownShortcuts().size();

    auto& search = tab.getShortcutSearch();
    search.setText ("undo", false);
    search.onTextChange();

    const auto& filtered = tab.getShownShortcuts();
    CHECK_MSG (! filtered.empty() && filtered.size() < all, "the search did not narrow the list");
    CHECK (rowFor (filtered, "undo") != nullptr);

    for (const auto& row : filtered)
        CHECK_MSG (row.description.containsIgnoreCase ("undo") || row.keyText.containsIgnoreCase ("undo")
                     || row.actionId.containsIgnoreCase ("undo"),
                   "\"" + row.description + "\" does not match the search");

    // By key as well as by name.
    search.setText (shortcutFor ("help").getTextDescription(), false);
    search.onTextChange();
    CHECK (rowFor (tab.getShownShortcuts(), "help") != nullptr);

    search.setText ({}, false);
    search.onTextChange();
    CHECK (tab.getShownShortcuts().size() == all);

    // Rebind... goes to the table that can change them.
    int tableRequests = 0;
    tab.onOpenShortcutTable = [&tableRequests] { ++tableRequests; };
    CHECK (press (tab.getRebindButton()));
    CHECK (tableRequests == 1);
}

//==============================================================================
/*  Column 4 at its 480-point minimum stacks the sheet under the topic; the
    overlay's width puts it beside. Nothing is clipped out of the tab in either,
    and both paint. */
LUTHIER_TEST (HelpTab, rendersNarrowAndWide)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    HelpTab tab (*processor);

    LuthierLookAndFeel lookAndFeel;
    tab.setLookAndFeel (&lookAndFeel);
    tab.showTopicFor ("getting-started");

    auto inside = [&tab] (juce::Component& c)
    {
        return ! c.getBounds().isEmpty() && tab.getLocalBounds().contains (c.getBounds());
    };

    // --- narrow ------------------------------------------------------------------------
    tab.setSize (480, tab.getPreferredHeight());

    CHECK_MSG (tab.getShortcutSearch().getY() > tab.getTopicList().getBottom(),
               "at 480 points the cheat sheet is not below the topic");

    for (auto* c : { static_cast<juce::Component*> (&tab.getTopicList()),
                     static_cast<juce::Component*> (&tab.getShortcutSearch()),
                     static_cast<juce::Component*> (&tab.getRebindButton()),
                     static_cast<juce::Component*> (&tab.getDebugButton()),
                     static_cast<juce::Component*> (&tab.getSourceButton()),
                     static_cast<juce::Component*> (&tab.getHomepageButton()),
                     static_cast<juce::Component*> (&tab.getSupportButton()) })
        CHECK_MSG (inside (*c), "a control is outside the tab at 480 points: " + c->getTitle());

    CHECK_MSG (renderTo (tab, "_help.png"), "the narrow tab painted nothing");

    // --- wide (the overlay's content area) -----------------------------------------------
    tab.setSize (828, 528);

    CHECK_MSG (tab.getShortcutSearch().getX() > tab.getTopicList().getRight(),
               "at overlay width the cheat sheet is not beside the topic");

    for (auto* c : { static_cast<juce::Component*> (&tab.getDebugButton()),
                     static_cast<juce::Component*> (&tab.getSupportButton()) })
        CHECK (inside (*c));

    CHECK_MSG (renderTo (tab, "_help_wide.png"), "the wide tab painted nothing");

    tab.setLookAndFeel (nullptr);
}

//==============================================================================
/*  Integration: 4.4's strip ends in HELP (C-32 puts TECHNIQUES before it), and
    the workspace topic names every tab that exists, so a tab added to the strip
    without its help fails here. */
LUTHIER_TEST (HelpTab, theWorkspaceTopicNamesEveryTabThatExists)
{
    PreservedPreferences preserved;
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    auto panel = std::make_unique<AdvancedPanel> (*processor);
    panel->setSize (1600, 900);

    const int count = panel->getNumWorkspaceTabs();
    CHECK (count > 0);
    CHECK_MSG (panel->getWorkspaceTabName (count - 1) == "HELP",
               "the last workspace tab is " + panel->getWorkspaceTabName (count - 1) + ", not HELP");

    const auto workspace = juce::String (HelpContent::getTopic (HelpContent::findTopic ("workspace")).body);

    for (int i = 0; i < count; ++i)
    {
        const auto name = panel->getWorkspaceTabName (i);
        CHECK_MSG (HelpContent::findTopic (name) >= 0, "the tab " + name + " has no help topic");
        CHECK_MSG (workspace.contains (name), "the workspace topic does not list the tab " + name);
    }

    CHECK (panel->setWorkspaceTabNamed ("HELP"));
    CHECK_MSG (dynamic_cast<HelpTab*> (panel->getWorkspacePanel (panel->getWorkspaceTab())) != nullptr,
               "the HELP tab does not show a HelpTab");
}

//==============================================================================
/*  Integration: F1 and the header's ? open Help on the panel the user is in
    (accessibility 2, gui-integration 20) - the HELP tab in Advanced, the
    overlay in Easy, and the overlay is the same surface. The tab's own buttons
    reach the overlays only the editor can show. */
LUTHIER_TEST (HelpTab, f1AndTheHeaderOpenHelpOnThePanelYouAreIn)
{
    PreservedPreferences preserved;
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());

    if (editor == nullptr)
    {
        CHECK_MSG (false, "no editor");
        return;
    }

    editor->setVisible (true);
    editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);

    auto* advanced = findOne<AdvancedPanel> (*editor);
    auto* host = findOne<OverlayHost> (*editor);
    auto* header = findOne<HeaderBar> (*editor);

    CHECK (advanced != nullptr && host != nullptr && header != nullptr);

    if (advanced == nullptr || host == nullptr || header == nullptr)
        return;

    auto shownTab = [advanced] { return advanced->getWorkspaceTabName (advanced->getWorkspaceTab()); };
    auto shownHelp = [advanced] { return dynamic_cast<HelpTab*> (advanced->getWorkspacePanel (advanced->getWorkspaceTab())); };
    auto escape = [&editor] { editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)); };

    // --- Advanced: F1 on TONE MATCH opens HELP on TONE MATCH's docs -------------------
    CHECK (editor->keyPressed (shortcutFor ("toggleAdvanced")));
    CHECK (advanced->isVisible());

    CHECK (advanced->setWorkspaceTabNamed ("TONE MATCH"));
    CHECK (editor->keyPressed (shortcutFor ("help")));

    CHECK_MSG (shownTab() == "HELP", "F1 in Advanced opened " + shownTab() + ", not the HELP tab");
    CHECK_MSG (! host->isShowingOverlay(), "F1 in Advanced put an overlay over the HELP tab");

    auto* help = shownHelp();
    CHECK_MSG (help != nullptr && help->getShownTopicId() == "tone-match",
               "F1 on TONE MATCH did not pin Help to its docs");

    // F1 again, on HELP itself, keeps the topic rather than resetting it.
    CHECK (editor->keyPressed (shortcutFor ("help")));
    CHECK (help != nullptr && help->getShownTopicId() == "tone-match");

    // The header's ? does the same from another tab.
    CHECK (advanced->setWorkspaceTabNamed ("MOD"));

    if (auto* question = findButton (*header, "?"))
        CHECK (press (*question));
    else
        CHECK_MSG (false, "the header has no ? button");

    CHECK (shownTab() == "HELP");
    CHECK_MSG (shownHelp() != nullptr && shownHelp()->getShownTopicId() == "mod",
               "the header's ? on MOD did not pin Help to its docs");

    // The tab's buttons reach the overlays.
    if (help != nullptr)
    {
        CHECK (press (help->getDebugButton()));
        CHECK_MSG (dynamic_cast<DebugPanel*> (host->getCurrentOverlay()) != nullptr,
                   "the HELP tab's debug button did not open the debug window");
        escape();

        CHECK (press (help->getRebindButton()));
        CHECK_MSG (dynamic_cast<OptionsPanel*> (host->getCurrentOverlay()) != nullptr,
                   "Rebind... did not open Options");
        escape();
    }

    // --- Easy: F1 opens the overlay, which is a HelpTab too ------------------------------
    CHECK (editor->keyPressed (shortcutFor ("toggleAdvanced")));
    CHECK (! advanced->isVisible());

    CHECK (editor->keyPressed (shortcutFor ("help")));
    CHECK (host->isShowingOverlay());

    auto* overlayHelp = host->getCurrentOverlay() != nullptr ? findOne<HelpTab> (*host->getCurrentOverlay())
                                                             : nullptr;
    CHECK_MSG (overlayHelp != nullptr, "the Help overlay is not the HelpTab surface");

    if (overlayHelp != nullptr)
    {
        CHECK (overlayHelp != help);
        CHECK (press (overlayHelp->getDebugButton()));
        CHECK_MSG (dynamic_cast<DebugPanel*> (host->getCurrentOverlay()) != nullptr,
                   "the overlay's debug button did not open the debug window");
    }

    escape();
    CHECK (! host->isShowingOverlay());
}
