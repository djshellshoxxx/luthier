#include "HelpTab.h"
#include "../PluginProcessor.h"
#include "../Presets/PresetManager.h"
#include "../Support/Diagnostics.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

namespace
{
    constexpr int kHeader = 26;
    constexpr int kRowGap = Metrics::gridHalf;

    /** At and above this the cheat sheet goes beside the topic, not under it. */
    constexpr int kWideWidth = 760;
}

//==============================================================================
HelpTab::HelpTab (LuthierAudioProcessor& p)
    : processor (p)
{
    // global-search.md 6.1 (FEAT-SEARCH).
    searchField.onOpen = [this] (const juce::String& typed) { if (onOpenSearch) onOpenSearch (typed); };
    addAndMakeVisible (searchField);

    // --- topics -----------------------------------------------------------------------
    topicList.setModel (&topicModel);
    topicList.setRowHeight (24);
    topicList.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);
    topicList.setTitle ("Help topics");
    topicList.setDescription ("The help topics. Choose one to read it.");
    addAndMakeVisible (topicList);

    body.setMultiLine (true, true);
    body.setReadOnly (true);
    body.setScrollbarsShown (true);
    body.setCaretVisible (false);
    body.setFont (Fonts::ui (12.5f));
    body.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);
    body.setColour (juce::TextEditor::textColourId, Palette::textPrimary);
    body.setColour (juce::TextEditor::outlineColourId, Palette::edge);
    AccessibleSetup::configureDescriptive (body, "Help text", "The text of the chosen help topic");
    addAndMakeVisible (body);

    // --- shortcuts --------------------------------------------------------------------
    searchBox.setTextToShowWhenEmpty ("Search shortcuts", Palette::textDisabled);
    searchBox.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);
    searchBox.setColour (juce::TextEditor::textColourId, Palette::textPrimary);
    searchBox.setTooltip ("Filter the shortcuts by what they do or by key.");
    searchBox.onTextChange = [this] { refreshShortcuts(); };

    // accessibility 1: Escape closes the overlay even with the cursor in here.
    searchBox.setEscapeAndReturnKeysConsumed (false);
    AccessibleSetup::configureDescriptive (searchBox, "Search shortcuts",
                                           "Filters the keyboard shortcut list");
    addAndMakeVisible (searchBox);

    rebindButton.setTooltip ("Change a shortcut: opens the shortcut table in Options, Accessibility.");
    rebindButton.onClick = [this] { if (onOpenShortcutTable) onOpenShortcutTable(); };
    AccessibleSetup::configureButton (rebindButton, "Rebind shortcuts",
                                      "Opens the shortcut table in Options, where any shortcut can be changed");
    addAndMakeVisible (rebindButton);

    shortcutList.setModel (&shortcutModel);
    shortcutList.setRowHeight (20);
    shortcutList.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);
    shortcutList.setTitle ("Keyboard shortcuts");
    shortcutList.setDescription ("Every keyboard shortcut as it is bound now.");
    addAndMakeVisible (shortcutList);

    // --- footer: include.md's debug button and links ------------------------------------
    debugButton.setTooltip ("Live internals, crash logging, the troubleshooting file and the hard reset.");
    debugButton.onClick = [this] { if (onOpenDebug) onOpenDebug(); };
    AccessibleSetup::configureButton (debugButton, "Open Debug Tools",
                                      "Live internals, crash logging, the troubleshooting file and the hard reset");
    addAndMakeVisible (debugButton);

    sourceButton.setTooltip (HelpContent::sourceUrl);
    sourceButton.onClick = [] { juce::URL (HelpContent::sourceUrl).launchInDefaultBrowser(); };
    AccessibleSetup::configureButton (sourceButton, "GitHub", "Opens the source repository in your browser");
    addAndMakeVisible (sourceButton);

    homepageButton.setTooltip (HelpContent::homepageUrl);
    homepageButton.onClick = [] { juce::URL (HelpContent::homepageUrl).launchInDefaultBrowser(); };
    AccessibleSetup::configureButton (homepageButton, "Homepage", "Opens the Luthier homepage in your browser");
    addAndMakeVisible (homepageButton);

    supportButton.setTooltip (HelpContent::supportEmail);
    supportButton.onClick = [] { HelpContent::getSupportMailUrl().launchInDefaultBrowser(); };
    AccessibleSetup::configureButton (supportButton, "Email Support", "Starts an email to support");
    addAndMakeVisible (supportButton);

    AccessibilitySettings::get().addChangeListener (this);

    refreshShortcuts();
    showTopic (0);

    setSize (480, getPreferredHeight());
}

HelpTab::~HelpTab()
{
    AccessibilitySettings::get().removeChangeListener (this);
}

//==============================================================================
juce::String HelpTab::getVersionText()
{
    return "Luthier " JucePlugin_VersionString;
}

juce::String HelpTab::getShownTopicId() const
{
    return HelpContent::getTopic (shownTopic).id;
}

bool HelpTab::showTopicFor (const juce::String& panelOrTopicName)
{
    const int index = HelpContent::findTopic (panelOrTopicName);

    if (index < 0)
        return false;

    showTopic (index);
    return true;
}

void HelpTab::showTopic (int index)
{
    shownTopic = juce::jlimit (0, HelpContent::getNumTopics() - 1, index);

    // The list's callback sees shownTopic already moved and does nothing, so a
    // pin from outside and a click in the list end in the same place.
    topicList.selectRow (shownTopic);

    body.setText (composeBody (shownTopic), false);
    body.moveCaretToTop (false);
}

juce::String HelpTab::composeBody (int index) const
{
    const auto& topic = HelpContent::getTopic (index);
    const juce::String id (topic.id);

    auto text = HelpContent::resolveKeys (topic.body);

    /*  include.md: where preset files go, and where the debug files are. The
        folders are read from the same functions that decide them, so this is
        the path on this machine rather than a description of the usual one -
        which is the one a user with a missing preset needs. */
    if (id == "presets")
    {
        text << "\n\nON THIS MACHINE\n"
             << "- Your presets: " << PresetManager::getUserPresetFolder().getFullPathName() << "\n"
             << "- Factory presets: " << PresetManager::getFactoryPresetFolder().getFullPathName() << "\n"
             << "- Renders: " << PresetManager::getRenderFolder().getFullPathName();
    }
    else if (id == "debug" || id == "troubleshooting")
    {
        text << "\n\nON THIS MACHINE\n"
             << "- Crash logs and troubleshooting files: "
             << Diagnostics::getDiagnosticsFolder().getFullPathName() << "\n"
             << "- Your presets: " << PresetManager::getUserPresetFolder().getFullPathName();
    }
    else if (id == "about")
    {
        // include.md: the version and the licence. The licence state is
        // License's own, so an expired or grace-period copy says so here too.
        auto& license = processor.getLicense();

        text << "\n\nVERSION\n" << getVersionText()
             << "\nRunning as " << juce::AudioProcessor::getWrapperTypeDescription (processor.wrapperType)
             << " in " << juce::PluginHostType().getHostDescription()
             << "\n\nLICENCE STATE\n" << License::getStateName (license.getState());

        if (license.getState() == License::State::grace)
            text << ", " << license.getDaysUntilRevalidation() << " days until it must be revalidated";

        text << "\n\nLINKS\n"
             << "- Homepage: " << HelpContent::homepageUrl << "\n"
             << "- Source: " << HelpContent::sourceUrl << "\n"
             << "- Support: " << HelpContent::supportEmail;
    }

    return text;
}

//==============================================================================
void HelpTab::refreshShortcuts()
{
    shownShortcuts = HelpContent::getShortcutRows (searchBox.getText());

    sheetLines.clear();
    juce::String group;

    for (int i = 0; i < (int) shownShortcuts.size(); ++i)
    {
        if (shownShortcuts[(size_t) i].group != group)
        {
            group = shownShortcuts[(size_t) i].group;
            sheetLines.push_back ({ -1, group });
        }

        sheetLines.push_back ({ i, {} });
    }

    shortcutList.updateContent();
    shortcutList.repaint();

    /*  The body quotes keys too ({key:...}), so it follows the same change. Only
        when it differs: this runs on every keystroke in the search box, and
        resetting the body each time would throw away where the reader was. */
    const auto updated = composeBody (shownTopic);

    if (updated != body.getText())
        body.setText (updated, false);
}

juce::String HelpTab::getShortcutText() const
{
    return HelpContent::formatShortcutRows (shownShortcuts);
}

void HelpTab::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshShortcuts();
    repaint();
}

void HelpTab::visibilityChanged()
{
    // A rebind made while the tab was hidden sent its message to a tab that
    // could not be seen; this is the backstop, and it costs one list rebuild.
    if (isVisible())
        refreshShortcuts();
}

//==============================================================================
int HelpTab::TopicModel::getNumRows()
{
    return HelpContent::getNumTopics();
}

void HelpTab::TopicModel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, HelpContent::getNumTopics()))
        return;

    if (selected)
    {
        g.setColour (Palette::accent.withAlpha (0.16f));
        g.fillRect (0, 0, width, height);

        // The bar is the selection's shape, so it does not rest on colour (0.2).
        g.setColour (Palette::accent);
        g.fillRect (0, 0, 2, height);
    }

    g.setColour (selected ? Palette::accent : Palette::textMuted);
    g.setFont (Fonts::ui (12.0f, selected));
    g.drawText (HelpContent::getTopic (row).title, 10, 0, width - 14, height,
                juce::Justification::centredLeft, true);
}

void HelpTab::TopicModel::selectedRowsChanged (int lastRow)
{
    if (lastRow >= 0 && lastRow != owner.shownTopic)
        owner.showTopic (lastRow);
}

juce::String HelpTab::TopicModel::getNameForRow (int row)
{
    return juce::isPositiveAndBelow (row, HelpContent::getNumTopics())
             ? juce::String (HelpContent::getTopic (row).title) : juce::String();
}

//==============================================================================
int HelpTab::ShortcutModel::getNumRows()
{
    return (int) owner.sheetLines.size();
}

void HelpTab::ShortcutModel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) owner.sheetLines.size()))
        return;

    const auto& line = owner.sheetLines[(size_t) row];

    if (line.row < 0)
    {
        g.setColour (Palette::accent);
        g.setFont (Fonts::ui (10.5f, true));
        Fonts::drawTrackedText (g, line.heading.toUpperCase(), { 6, 0, width - 12, height },
                                juce::Justification::bottomLeft);
        return;
    }

    const auto& shortcut = owner.shownShortcuts[(size_t) line.row];

    if (selected)
    {
        g.setColour (Palette::accentDim.withAlpha (0.5f));
        g.fillRect (0, 0, width, height);
    }

    const int keyWidth = juce::jlimit (90, 150, width * 2 / 5);

    // Rebound is said in words and in the accent; fixed keys are dimmed and say
    // so in their description.
    g.setFont (Fonts::ui (11.0f));
    g.setColour (shortcut.fixed ? Palette::textDisabled : Palette::textPrimary);
    g.drawText (shortcut.description + (shortcut.rebound ? "  (rebound)" : ""),
                10, 0, width - keyWidth - 16, height, juce::Justification::centredLeft, true);

    g.setFont (Fonts::mono (11.0f));
    g.setColour (shortcut.rebound ? Palette::accent
                                  : (shortcut.fixed ? Palette::textDisabled : Palette::textMuted));
    g.drawText (shortcut.keyText, width - keyWidth - 6, 0, keyWidth, height,
                juce::Justification::centredRight, true);
}

void HelpTab::ShortcutModel::listBoxItemDoubleClicked (int, const juce::MouseEvent&)
{
    if (owner.onOpenShortcutTable)
        owner.onOpenShortcutTable();
}

juce::String HelpTab::ShortcutModel::getNameForRow (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) owner.sheetLines.size()))
        return {};

    const auto& line = owner.sheetLines[(size_t) row];

    if (line.row < 0)
        return line.heading;

    const auto& shortcut = owner.shownShortcuts[(size_t) line.row];

    return shortcut.description + ": " + shortcut.keyText + (shortcut.rebound ? ", rebound" : "");
}

//==============================================================================
void HelpTab::paint (juce::Graphics& g)
{
    LuthierLookAndFeel::drawSectionHeader (g, headerBounds, "HELP");
    LuthierLookAndFeel::drawSectionHeader (g, shortcutHeaderBounds, "SHORTCUTS");

    // include.md: the version number, on screen whatever topic is showing.
    g.setFont (Fonts::mono (11.0f));
    g.setColour (Palette::textMuted);
    g.drawText (getVersionText(), versionBounds, juce::Justification::centredRight, true);
}

void HelpTab::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::grid, 0);
    const int button = Metrics::buttonHeight;

    headerBounds = bounds.removeFromTop (kHeader);
    versionBounds = headerBounds.removeFromRight (juce::jmin (160, headerBounds.getWidth() / 2));
    searchField.setBounds (headerBounds.removeFromRight (juce::jmin (220, headerBounds.getWidth() / 2)).reduced (4, 2));   // FEAT-SEARCH
    bounds.removeFromTop (kRowGap);

    layoutFooter (bounds.removeFromBottom (button));
    bounds.removeFromBottom (Metrics::grid);

    juce::Rectangle<int> sheet;

    if (bounds.getWidth() >= kWideWidth)
    {
        sheet = bounds.removeFromRight (juce::jmin (340, bounds.getWidth() * 2 / 5));
        bounds.removeFromRight (Metrics::grid);
    }
    else
    {
        sheet = bounds.removeFromBottom (juce::jmax (160, bounds.getHeight() * 2 / 5));
        bounds.removeFromBottom (Metrics::grid);
    }

    topicList.setBounds (bounds.removeFromLeft (juce::jlimit (120, 180, bounds.getWidth() / 3)));
    bounds.removeFromLeft (Metrics::grid);
    body.setBounds (bounds);

    shortcutHeaderBounds = sheet.removeFromTop (kHeader);
    sheet.removeFromTop (kRowGap);

    auto searchRow = sheet.removeFromTop (button);
    rebindButton.setBounds (searchRow.removeFromRight (90));
    searchRow.removeFromRight (Metrics::gridHalf);
    searchBox.setBounds (searchRow);

    sheet.removeFromTop (kRowGap);
    shortcutList.setBounds (sheet);
}

void HelpTab::layoutFooter (juce::Rectangle<int> footer)
{
    const int gap = Metrics::gridHalf;
    const int wanted = 150 + Metrics::grid + 90 + 100 + 120 + 2 * gap;

    if (footer.getWidth() >= wanted)
    {
        debugButton.setBounds (footer.removeFromLeft (150));

        supportButton.setBounds (footer.removeFromRight (120));
        footer.removeFromRight (gap);
        homepageButton.setBounds (footer.removeFromRight (100));
        footer.removeFromRight (gap);
        sourceButton.setBounds (footer.removeFromRight (90));
        return;
    }

    // Column 4 at its narrowest: four equal buttons rather than one clipped.
    const int width = (footer.getWidth() - 3 * gap) / 4;

    for (auto* b : { static_cast<juce::Component*> (&debugButton), static_cast<juce::Component*> (&sourceButton),
                     static_cast<juce::Component*> (&homepageButton), static_cast<juce::Component*> (&supportButton) })
    {
        b->setBounds (footer.removeFromLeft (width));
        footer.removeFromLeft (gap);
    }
}

} // namespace luthier
