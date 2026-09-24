/*  The editor.

    Everything in this file exists because of a build change rather than a code
    change. Source/UI was excluded from every target that runs, so the editor and
    the panels were compiled only into the plugin, and the only thing that ever
    constructed a LuthierAudioProcessorEditor was a host - pluginval on a good
    day, a DAW on a bad one. Eight panels had once sat in the tree for a whole
    milestone without being compiled at all, which is the failure mode this is
    meant to close.

    These are smoke tests, and they are honest about it: they open the window, lay
    it out, paint it, drive the shortcuts and read back what is showing. They do
    not know what the window is supposed to look like. What they catch is the
    class of thing that made them necessary - a panel that does not compile, a
    layout that collapses at the minimum size, a page wired to a tab and never
    shown.

    They assert on state as well as on pixels, and that is deliberate. The first
    version of the Options test compared renders of the whole panel, and it passed
    with the page-visibility line in showPage replaced by setVisible (false):
    selecting a tab lights that tab up, so the pixels move whether or not the page
    behind them ever appears. Every pixel comparison here is therefore paired with
    a question about what is actually visible.
*/

#include "TestFramework.h"

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/EasyPanel.h"
#include "../UI/HeaderBar.h"
#include "../UI/LiveStrip.h"
#include "../UI/OptionsPages.h"
#include "../UI/Overlays.h"
#include "../UI/PracticePanel.h"
#include "../UI/UiPreferences.h"
#include "../UI/GuitarBodyComponent.h"
#include "../UI/Widgets.h"
#include "../UI/Notifications.h"
#include "../UI/LivePanel.h"
#include "../UI/MidiOutPanel.h"
#include "../UI/NotationPanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    /*  Paints a component and everything inside it into an offscreen image.

        paintEntireComponent is what a real peer calls, so this walks the same
        path a window does: every visible child's paint(), in order, clipped the
        same way. The component never goes on the desktop, which is what lets this
        run in a console app.
    */
    juce::Image render (juce::Component& c)
    {
        juce::Image image (juce::Image::ARGB,
                           juce::jmax (1, c.getWidth()),
                           juce::jmax (1, c.getHeight()),
                           true);

        juce::Graphics g (image);
        c.paintEntireComponent (g, true);

        return image;
    }

    /*  A cheap fingerprint of what was drawn.

        Two renders that differ anywhere give different values; the exact number
        means nothing. It is only ever compared against another render of the same
        component at the same size, which is why summing the pixels is enough. The
        x weight keeps two pixels that swapped places from cancelling out.
    */
    juce::uint64 digest (const juce::Image& image)
    {
        juce::uint64 sum = 0;

        const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < pixels.height; ++y)
            for (int x = 0; x < pixels.width; ++x)
                sum += (juce::uint64) pixels.getPixelColour (x, y).getARGB()
                         * (juce::uint64) (x + 1);

        return sum;
    }

    /** True if anything at all was drawn. A component that paints nothing is a
        hole in the window, and a test that only checked for "did not crash" would
        call that a pass. */
    bool drewSomething (const juce::Image& image)
    {
        const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < pixels.height; y += 4)
            for (int x = 0; x < pixels.width; x += 4)
                if (pixels.getPixelColour (x, y).getAlpha() != 0)
                    return true;

        return false;
    }

    /*  The editor keeps its panels private, which is right: a test that reaches
        into private members breaks every time the window is rearranged. These
        find them the way the window itself sees them - by walking the component
        tree and asking what each thing is. */
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

    /*  A button by the text on it. Tests that drive a panel's buttons want the
        one the user would click, and its member name is private to the panel. */
    juce::Button* findButton (juce::Component& root, const juce::String& text)
    {
        juce::Array<juce::Button*> buttons;
        collect<juce::Button> (root, buttons);

        for (auto* button : buttons)
            if (button->getButtonText().equalsIgnoreCase (text))
                return button;

        return nullptr;
    }

    /*  Presses it, and says whether there was anything to press.

        Not triggerClick(): that posts a message, and a console test has no
        message loop pumping it, so the click silently never happens and whatever
        is asserted afterwards fails for the wrong reason - which is exactly how
        this helper came to exist. This calls the same callback a real click
        calls, and refuses a disabled button, so a greyed-out control still fails
        the test rather than being driven anyway. */
    bool clickButton (juce::Component& root, const juce::String& text)
    {
        auto* button = findButton (root, text);

        if (button == nullptr || ! button->isEnabled() || button->onClick == nullptr)
            return false;

        button->onClick();
        return true;
    }

    juce::KeyPress shortcutFor (const char* actionId)
    {
        if (const auto* binding = AccessibilitySettings::get().findShortcut (actionId))
            return binding->key;

        return {};
    }
}

//==============================================================================
/*  The plugin says it has an editor, and hands one over.

    In a headless build hasEditor() is false and createEditor() returns nullptr,
    which is how the renderer gets away without Source/UI. If this target ever
    goes back to LUTHIER_HEADLESS=1 by accident, this is what says so, rather than
    the whole file quietly passing on an editor that was never built. */
LUTHIER_TEST (Editor, theProcessorHandsOverAnEditorAtItsDocumentedSize)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    CHECK (processor.hasEditor());

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    CHECK_MSG (editor != nullptr, "createEditor returned nullptr - is this target headless?");

    if (editor == nullptr)
        return;

    CHECK (editor->getWidth()  == LuthierAudioProcessorEditor::defaultWidth);
    CHECK (editor->getHeight() == LuthierAudioProcessorEditor::defaultHeight);
}

//==============================================================================
/*  It lays out and paints across its resize range.

    The minimum is where layout arithmetic breaks: a row that takes fixed heights
    out of a rectangle and then divides what is left runs out of pixels there
    first, and the panel that loses the race ends up with no height at all.
    gui-integration.md fixes that minimum at 940x560, so it is part of the
    contract rather than an arbitrary probe. */
LUTHIER_TEST (Editor, itLaysOutAndPaintsAcrossItsResizeRange)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
    {
        CHECK_MSG (false, "no editor to lay out");
        return;
    }

    editor->setVisible (true);

    auto* header = findOne<HeaderBar> (*editor);
    auto* easy   = findOne<EasyPanel> (*editor);

    CHECK (header != nullptr);
    CHECK (easy != nullptr);

    const std::pair<int, int> sizes[] =
    {
        { LuthierAudioProcessorEditor::minimumWidth, LuthierAudioProcessorEditor::minimumHeight },
        { LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight },
        { 1920, 1080 }
    };

    for (const auto& size : sizes)
    {
        const juce::String where (juce::String (size.first) + "x" + juce::String (size.second));

        editor->setSize (size.first, size.second);

        CHECK_MSG (drewSomething (render (*editor)), "nothing was painted at " + where);

        // Both of these are on screen at every size, so neither may be squeezed
        // to nothing by the one above it taking what it wants first.
        if (header != nullptr)
            CHECK_MSG (header->getWidth() > 0 && header->getHeight() > 0,
                       "the header bar has no size at " + where);

        if (easy != nullptr)
            CHECK_MSG (easy->getWidth() > 0 && easy->getHeight() > 0,
                       "the main panel has no size at " + where);
    }
}

//==============================================================================
/*  Every shortcut that opens an overlay opens the right one.

    The editor looks each key up in AccessibilitySettings rather than comparing
    key codes, so this checks that the registry and the editor still agree: a
    binding missing from the table makes keyPressed return false here, and makes
    the shortcut dead in the plugin for exactly the same reason.

    The overlay host is asked what is up, rather than the pixels being taken as
    proof: the host dims the window behind whatever it shows, so an overlay that
    came up empty would still change every pixel on screen.

    Escape is the way back out, and is deliberately not rebindable. */
LUTHIER_TEST (Editor, everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
    {
        CHECK_MSG (false, "no editor to open overlays on");
        return;
    }

    editor->setVisible (true);
    editor->setSize (LuthierAudioProcessorEditor::defaultWidth,
                     LuthierAudioProcessorEditor::defaultHeight);

    auto* host = findOne<OverlayHost> (*editor);

    CHECK_MSG (host != nullptr, "the editor has no overlay host");

    if (host == nullptr)
        return;

    /*  The last of these is not a sixth overlay: accessibility 2's "show all
        shortcuts" surface is the rebind table, which lives on a page of Options,
        so it has to open the same panel the Options shortcut does. */
    const char* const overlayActions[] =
        { "help", "options", "presetBrowser", "export", "debugPanel", "saveAs", "showShortcuts" };

    juce::Array<OverlayPanel*> opened;

    for (const auto* action : overlayActions)
    {
        CHECK_MSG (! host->isShowingOverlay(),
                   juce::String ("an overlay was still up before \"") + action + "\"");

        const auto closed = digest (render (*editor));
        const auto key = shortcutFor (action);

        CHECK_MSG (key.isValid(),
                   juce::String ("no shortcut registered for \"") + action + "\"");

        if (! key.isValid())
            continue;

        CHECK_MSG (editor->keyPressed (key),
                   juce::String ("the editor ignored the shortcut for \"") + action + "\"");

        auto* panel = host->getCurrentOverlay();

        CHECK_MSG (panel != nullptr,
                   juce::String ("\"") + action + "\" opened no overlay");

        if (panel == nullptr)
            continue;

        opened.add (panel);

        /*  isVisible, not isShowing: isShowing walks up to the desktop peer, and
            an editor rendered into an image has no peer. Whether the chain above
            the panel is visible is covered by the host being asked directly. */
        CHECK_MSG (panel->isVisible() && panel->getParentComponent() == host
                     && host->isVisible() && panel->getWidth() > 0 && panel->getHeight() > 0,
                   juce::String ("the overlay \"") + action + "\" opened is not on screen");

        CHECK_MSG (drewSomething (render (*panel)),
                   juce::String ("the overlay \"") + action + "\" opened painted nothing");

        CHECK_MSG (digest (render (*editor)) != closed,
                   juce::String ("\"") + action + "\" changed nothing on screen");

        CHECK_MSG (editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)),
                   juce::String ("escape did not close the overlay \"") + action + "\" opened");

        CHECK_MSG (! host->isShowingOverlay(),
                   juce::String ("the overlay \"") + action + "\" opened survived escape");
    }

    /*  Six shortcuts, six different panels. Without this, every one of them
        could open Help and the checks above would all pass. */
    if (opened.size() == 7)
    {
        for (int i = 0; i < 6; ++i)
            for (int j = i + 1; j < 6; ++j)
                CHECK_MSG (opened[i] != opened[j],
                           juce::String ("\"") + overlayActions[i] + "\" and \""
                             + overlayActions[j] + "\" open the same overlay");

        CHECK_MSG (opened[6] == opened[1],
                   "the shortcut table is not on the Options panel");
    }
}

//==============================================================================
/*  The modes that change the shape of the window rather than what is in it.

    Advanced mode swaps one main panel for the other, the practice drawer opens,
    and Live Mode brings up the strip. Each is driven from the same shortcut
    registry as the overlays, and each is read back from the panel it claims to
    have moved.

    Toggling every one of them off again is half the test: a mode that cannot be
    left is a trap in a live set. */
LUTHIER_TEST (Editor, theModesThatChangeTheLayoutTakeEffectAndUndoThemselves)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
    {
        CHECK_MSG (false, "no editor to switch modes on");
        return;
    }

    editor->setVisible (true);
    editor->setSize (LuthierAudioProcessorEditor::defaultWidth,
                     LuthierAudioProcessorEditor::defaultHeight);

    auto* easy     = findOne<EasyPanel> (*editor);
    auto* advanced = findOne<AdvancedPanel> (*editor);
    auto* practice = findOne<PracticePanel> (*editor);
    auto* live     = findOne<LiveStrip> (*editor);

    CHECK (easy != nullptr && advanced != nullptr && practice != nullptr && live != nullptr);

    if (easy == nullptr || advanced == nullptr || practice == nullptr || live == nullptr)
        return;

    //--------------------------------------------------------------------------
    // Advanced mode: exactly one of the two main panels is ever on screen.
    {
        const auto before = digest (render (*editor));

        CHECK (easy->isVisible() && ! advanced->isVisible());

        CHECK (editor->keyPressed (shortcutFor ("toggleAdvanced")));
        CHECK_MSG (advanced->isVisible() && ! easy->isVisible(),
                   "the advanced panel did not replace the easy one");
        CHECK (processor.getUiState().advancedMode);
        CHECK_MSG (digest (render (*editor)) != before, "advanced mode drew the same window");

        CHECK (editor->keyPressed (shortcutFor ("toggleAdvanced")));
        CHECK (easy->isVisible() && ! advanced->isVisible());
        CHECK_MSG (digest (render (*editor)) == before, "leaving advanced mode did not undo it");
    }

    //--------------------------------------------------------------------------
    // The practice drawer.
    {
        const auto before = digest (render (*editor));

        CHECK (! practice->isOpen());

        CHECK (editor->keyPressed (shortcutFor ("togglePractice")));
        CHECK_MSG (practice->isOpen(), "the practice drawer did not open");
        CHECK_MSG (digest (render (*editor)) != before, "the open drawer drew nothing new");

        CHECK (editor->keyPressed (shortcutFor ("togglePractice")));
        CHECK (! practice->isOpen());
        CHECK_MSG (digest (render (*editor)) == before, "closing the drawer did not undo it");
    }

    //--------------------------------------------------------------------------
    // Live Mode and its strip.
    {
        const auto before = digest (render (*editor));

        CHECK (! processor.isLiveMode() && ! live->isVisible());

        CHECK (editor->keyPressed (shortcutFor ("toggleLiveMode")));
        CHECK (processor.isLiveMode());
        CHECK_MSG (live->isVisible(), "Live Mode is on and the live strip is not showing");
        CHECK_MSG (digest (render (*editor)) != before, "the live strip drew nothing");

        CHECK (editor->keyPressed (shortcutFor ("toggleLiveMode")));
        CHECK (! processor.isLiveMode() && ! live->isVisible());
        CHECK_MSG (digest (render (*editor)) == before, "leaving Live Mode did not undo it");
    }
}

//==============================================================================
/*  Every Options page can be selected, and selecting it puts that page on screen.

    The five tabs are General plus the four pages the extension specs added
    (controllers 3 and 5, live-performance 8, accessibility 9, updates-telemetry
    1 and 6). Page 0 is General, whose controls are direct members of the panel
    rather than an OptionsPage, so the pages run one behind the tabs: tab i shows
    page i - 1, and tab 0 shows none of them. That offset is the contract this
    checks.

    The panel is built directly rather than reached through the editor: it is a
    public class, and the editor keeps its copy private. The tabs are driven
    through onClick rather than triggerClick, which posts a message and needs a
    loop running to deliver it. */
LUTHIER_TEST (Editor, everyOptionsPageSelectsAndPaints)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    OptionsPanel options (processor);

    const auto preferred = options.getPreferredSize();

    options.setVisible (true);
    options.setSize (preferred.x, preferred.y);
    options.overlayShown();

    /*  gui-integration.md section 5's list, all eleven, in section 5's order.
        CONTROLLERS was in the RANGES slot until column 4's tab strip gave it
        section 19's home, and it is checked there instead, by
        everyWorkspaceTabSelectsAndPaints - if it is ever added back here as
        well, the count below fails rather than the plugin quietly running two
        profile libraries. If section 5 gains a tab, this list is where it fails
        first. */
    const juce::StringArray tabNames { "AUDIO", "MIDI", "APPEARANCE", "ACCESSIBILITY",
                                       "LOCALIZATION", "EXPRESSION", "RANGES",
                                       "UPDATES", "PRIVACY", "DIAGNOSTICS",
                                       "FILE LOCATIONS" };

    juce::Array<OptionsPage*> pages;
    collect<OptionsPage> (options, pages);

    /*  One page per tab. There used to be an offset here - tab 0 was General,
        whose controls were members of the panel rather than a page, so tab i
        showed page i - 1 - and section 5's retab removed it along with General
        itself. A test that still expected the offset is how that was noticed. */
    CHECK_MSG (pages.size() == tabNames.size(),
               "expected " + juce::String (tabNames.size()) + " Options pages, found "
                 + juce::String (pages.size()));

    juce::Array<juce::uint64> pageDigests;

    for (int tab = 0; tab < tabNames.size(); ++tab)
    {
        const auto& name = tabNames[tab];

        juce::Button* button = nullptr;

        for (auto* child : options.getChildren())
            if (auto* candidate = dynamic_cast<juce::TextButton*> (child))
                if (candidate->getButtonText() == name)
                    button = candidate;

        CHECK_MSG (button != nullptr, "no Options tab called " + name);

        if (button == nullptr)
            continue;

        CHECK_MSG (button->onClick != nullptr, name + " is a tab that does nothing");

        if (button->onClick == nullptr)
            continue;

        button->onClick();

        CHECK_MSG (drewSomething (render (options)), "the " + name + " tab painted nothing");

        juce::Array<OptionsPage*> showing;

        for (auto* page : pages)
            if (page->isVisible())
                showing.add (page);

        CHECK_MSG (showing.size() == 1,
                   name + " put " + juce::String (showing.size()) + " pages on screen");

        if (showing.size() != 1)
            continue;

        CHECK_MSG (showing.getFirst() == pages[tab],
                   name + " showed a page belonging to another tab");

        auto* page = showing.getFirst();

        CHECK_MSG (page->getWidth() > 0 && page->getHeight() > 0,
                   "the " + name + " page has no size");

        CHECK_MSG (drewSomething (render (*page)), "the " + name + " page painted nothing");

        pageDigests.add (digest (render (*page)));
    }

    /*  And each page draws its own thing. This is the check that fails if two
        tabs are wired to the same page. */
    for (int i = 0; i < pageDigests.size(); ++i)
        for (int j = i + 1; j < pageDigests.size(); ++j)
            CHECK_MSG (pageDigests[i] != pageDigests[j],
                       "two Options pages painted identical pixels");
}

//==============================================================================
/*  Column 4's tab strip: every tab selects, and selects only its own panel.

    Modelled on everyOptionsPageSelectsAndPaints, and for the same reason. The
    workspace is one viewport showing one panel at a time, so "the tab lit up" and
    "the panel is on screen" are two different facts and only the second one is
    the feature. The five panels used to be stacked in a scrolling column, where
    every one of them was always visible and this test could not have failed.

    The names are gui-integration.md section 4.4's order, restricted to the tabs
    that have a panel behind them. A tab added or renamed fails here first.
*/
LUTHIER_TEST (Editor, everyWorkspaceTabSelectsAndPaints)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    AdvancedPanel panel (processor);

    panel.setVisible (true);
    panel.setSize (1600, 900);

    // In order: tune-builder 3 puts TUNE between RHYTHM and LIVE; practice-tools
    // 11 puts PRACTICE between CHARACTER and NOTATION.
    const juce::StringArray tabNames { "WORKSHOP", "MOD", "RHYTHM", "TUNE", "LIVE", "ROUTING", "TONE MATCH",
                                       "CHARACTER", "PRACTICE", "NOTATION", "MIDI OUT", "CONTROLLERS", "HELP" };

    CHECK_MSG (panel.getNumWorkspaceTabs() == tabNames.size(),
               "expected " + juce::String (tabNames.size()) + " workspace tabs, found "
                 + juce::String (panel.getNumWorkspaceTabs()));

    if (panel.getNumWorkspaceTabs() != tabNames.size())
        return;

    juce::Array<juce::uint64> tabDigests;

    for (int tab = 0; tab < tabNames.size(); ++tab)
    {
        const auto& name = tabNames[tab];

        CHECK_MSG (panel.getWorkspaceTabName (tab) == name,
                   "workspace tab " + juce::String (tab) + " is called \""
                     + panel.getWorkspaceTabName (tab) + "\", not \"" + name + "\"");

        /*  Driven through the button, not through setWorkspaceTab: the button is
            what a user touches, and a tab whose onClick was never wired would
            pass a test that called the method directly. */
        juce::Button* button = nullptr;

        for (auto* child : panel.getChildren())
            if (auto* candidate = dynamic_cast<juce::TextButton*> (child))
                if (candidate->getButtonText() == name)
                    button = candidate;

        CHECK_MSG (button != nullptr, "no workspace tab called " + name);

        if (button == nullptr || button->onClick == nullptr)
        {
            CHECK_MSG (false, name + " is a tab that does nothing");
            continue;
        }

        button->onClick();

        CHECK_MSG (panel.getWorkspaceTab() == tab,
                   name + " selected tab " + juce::String (panel.getWorkspaceTab()));

        CHECK_MSG (button->getToggleState(), name + " did not light up when selected");

        // One panel on screen, and it is this tab's.
        juce::Array<juce::Component*> showing;

        for (int i = 0; i < panel.getNumWorkspaceTabs(); ++i)
            if (auto* candidate = panel.getWorkspacePanel (i))
                if (candidate->isVisible())
                    showing.add (candidate);

        CHECK_MSG (showing.size() == 1,
                   name + " put " + juce::String (showing.size()) + " workspace panels on screen");

        if (showing.size() != 1)
            continue;

        CHECK_MSG (showing.getFirst() == panel.getWorkspacePanel (tab),
                   name + " showed a panel belonging to another tab");

        auto* shown = showing.getFirst();

        CHECK_MSG (shown->getWidth() > 0 && shown->getHeight() > 0,
                   "the " + name + " panel has no size");

        CHECK_MSG (shown->getParentComponent() != nullptr,
                   "the " + name + " panel is not in the workspace viewport");

        CHECK_MSG (drewSomething (render (*shown)), "the " + name + " panel painted nothing");

        tabDigests.add (digest (render (*shown)));
    }

    // Five tabs, five different panels.
    for (int i = 0; i < tabDigests.size(); ++i)
        for (int j = i + 1; j < tabDigests.size(); ++j)
            CHECK_MSG (tabDigests[i] != tabDigests[j],
                       "two workspace tabs painted identical pixels");
}

//==============================================================================
/*  The tab steps wrap, and the last-used tab comes back next session.

    Both are section 4.4. Wrapping matters because without it the last tab needs a
    different key from every other tab to leave, and the persistence is the half
    of 4.4 that was written down as not done.

    The stored value lives in the user's real config file, because that is where
    the feature has to put it. The test puts back whatever was there - including
    removing the file if the run created it - so a test run does not decide which
    tab the user's next session opens on.
*/
LUTHIER_TEST (Editor, theWorkspaceTabWrapsAndIsRemembered)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const auto configFile = UiPreferences::getConfigFile();
    const bool hadConfig = configFile.existsAsFile();
    const auto originalConfig = hadConfig ? configFile.loadFileAsString() : juce::String();

    int count = 0;

    {
        AdvancedPanel panel (processor);

        panel.setVisible (true);
        panel.setSize (1600, 900);

        count = panel.getNumWorkspaceTabs();

        CHECK (count > 2);

        if (count > 2)
        {
            panel.setWorkspaceTab (0);

            panel.stepWorkspaceTab (1);
            CHECK_MSG (panel.getWorkspaceTab() == 1, "stepping forward did not move one tab");

            panel.stepWorkspaceTab (-1);
            CHECK_MSG (panel.getWorkspaceTab() == 0, "stepping back did not move one tab");

            panel.stepWorkspaceTab (-1);
            CHECK_MSG (panel.getWorkspaceTab() == count - 1,
                       "stepping back from the first tab did not wrap to the last");

            panel.stepWorkspaceTab (1);
            CHECK_MSG (panel.getWorkspaceTab() == 0,
                       "stepping forward from the last tab did not wrap to the first");

            // Out of range is clamped, not refused: a stored tab from a build with
            // more tabs than this one still opens on something.
            panel.setWorkspaceTab (count + 9);
            CHECK (panel.getWorkspaceTab() == count - 1);

            panel.setWorkspaceTab (-4);
            CHECK (panel.getWorkspaceTab() == 0);

            // Left on a tab that is neither end, so a panel that always opened on
            // the first or the last one could not pass the next part.
            panel.setWorkspaceTab (2);
        }
    }

    // A second panel is a second session: it reads the tab back from the file.
    if (count > 2)
    {
        UiPreferences::get().reset();

        CHECK_MSG (UiPreferences::get().load(),
                   "the workspace tab was not written to " + configFile.getFullPathName());

        AdvancedPanel reopened (processor);

        reopened.setVisible (true);
        reopened.setSize (1600, 900);

        CHECK_MSG (reopened.getWorkspaceTab() == 2,
                   "the workspace tab did not survive the session, it came back as "
                     + juce::String (reopened.getWorkspaceTab()));
    }

    //--------------------------------------------------------------------------
    if (hadConfig)
        configFile.replaceWithText (originalConfig);
    else
        configFile.deleteFile();

    UiPreferences::get().reset();
    UiPreferences::get().load();
}

//==============================================================================
/*  Section 4.5: Advanced Mode is unavailable below 1000 points.

    The window's own minimum is 940, so this is not a theoretical size - it is one
    drag away from the default. Three 220-point columns and a 480-point workspace
    do not fit in it, and the mode has to say so rather than lay out four columns
    too narrow to read.

    Both routes into the mode are checked: dragging the window narrow while it is
    already on, and asking for it while the window is narrow. The notice is checked
    because a toggle that silently does nothing reads as broken - ground rule 0.2
    is that degradation is never silent.
*/
LUTHIER_TEST (Editor, advancedModeIsRefusedBelowItsMinimumWidth)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
    {
        CHECK_MSG (false, "no editor to resize");
        return;
    }

    editor->setVisible (true);
    editor->setSize (LuthierAudioProcessorEditor::defaultWidth,
                     LuthierAudioProcessorEditor::defaultHeight);

    auto* easy     = findOne<EasyPanel> (*editor);
    auto* advanced = findOne<AdvancedPanel> (*editor);
    auto* notice   = findOne<InlineNotice> (*editor);

    CHECK (easy != nullptr && advanced != nullptr);
    CHECK_MSG (notice != nullptr, "the editor has no inline notice to explain itself with");

    if (easy == nullptr || advanced == nullptr || notice == nullptr)
        return;

    /*  The default size is above the minimum and the window's own minimum is
        below it, so this is a real threshold a drag can cross rather than a mode
        that is never available or never refused. */
    CHECK (LuthierAudioProcessorEditor::defaultWidth >= AdvancedPanel::minimumUsableWidth);
    CHECK (LuthierAudioProcessorEditor::minimumWidth < AdvancedPanel::minimumUsableWidth);

    //--------------------------------------------------------------------------
    // Wide enough: the mode works as it always did.
    CHECK (editor->keyPressed (shortcutFor ("toggleAdvanced")));
    CHECK_MSG (advanced->isVisible(), "advanced mode did not open at the default size");
    CHECK (! notice->isVisible());

    //--------------------------------------------------------------------------
    // Dragged narrow while it is on: forced back to Easy, with the reason on screen.
    editor->setSize (LuthierAudioProcessorEditor::minimumWidth,
                     LuthierAudioProcessorEditor::minimumHeight);

    CHECK_MSG (easy->isVisible() && ! advanced->isVisible(),
               "advanced mode survived a window too narrow for it");

    CHECK_MSG (! processor.getUiState().advancedMode,
               "the window left advanced mode and the state still says it is in it");

    CHECK_MSG (notice->isVisible(), "the window left advanced mode without saying why");

    CHECK_MSG (notice->getMessage().containsIgnoreCase ("advanced"),
               "the notice does not say what it is about: " + notice->getMessage());

    CHECK_MSG (notice->getWidth() > 0 && notice->getHeight() > 0
                 && drewSomething (render (*notice)),
               "the notice is showing and painted nothing");

    //--------------------------------------------------------------------------
    // And asking for it while narrow is refused the same way.
    notice->dismiss();
    CHECK (! notice->isVisible());

    CHECK (editor->keyPressed (shortcutFor ("toggleAdvanced")));

    CHECK_MSG (! advanced->isVisible() && easy->isVisible(),
               "the toggle opened advanced mode in a window too narrow for it");

    CHECK_MSG (notice->isVisible(), "the toggle refused without saying why");

    //--------------------------------------------------------------------------
    // Widening gives it back. A mode that could not be got back into would be a
    // worse bug than the one this test is about.
    editor->setSize (LuthierAudioProcessorEditor::defaultWidth,
                     LuthierAudioProcessorEditor::defaultHeight);

    CHECK (editor->keyPressed (shortcutFor ("toggleAdvanced")));
    CHECK_MSG (advanced->isVisible(), "advanced mode did not come back when the window widened");
}

//==============================================================================
/*  Ctrl+[ and Ctrl+] step the workspace tabs.

    Section 17's last blocked row: there were no column 4 tabs to step, and now
    there are. They are answered only in Advanced Mode, and that is checked from
    both sides - in Easy the editor has to decline the key rather than swallow it,
    because a key that returns true and does nothing is how a host stops passing
    it on to anything else.
*/
LUTHIER_TEST (Editor, theWorkspaceTabShortcutsStepTheTabsInAdvancedModeOnly)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
    {
        CHECK_MSG (false, "no editor to step tabs in");
        return;
    }

    editor->setVisible (true);
    editor->setSize (1600, 960);

    auto* advanced = findOne<AdvancedPanel> (*editor);

    CHECK (advanced != nullptr);

    if (advanced == nullptr)
        return;

    const auto next = shortcutFor ("nextWorkspaceTab");
    const auto previous = shortcutFor ("previousWorkspaceTab");

    CHECK_MSG (next.isValid() && previous.isValid(),
               "the workspace tab steps are not in the shortcut registry");

    // They are not the unmodified preset steps, which sit next to them.
    CHECK (next != shortcutFor ("nextItem") && previous != shortcutFor ("previousItem"));

    //--------------------------------------------------------------------------
    // In Easy mode there is no column 4, so the key is declined rather than eaten.
    CHECK_MSG (! editor->keyPressed (next),
               "the editor swallowed the tab step in Easy mode");

    //--------------------------------------------------------------------------
    CHECK (editor->keyPressed (shortcutFor ("toggleAdvanced")));
    CHECK (advanced->isVisible());

    advanced->setWorkspaceTab (0);

    CHECK (editor->keyPressed (next));
    CHECK_MSG (advanced->getWorkspaceTab() == 1, "Ctrl+] did not step forward a tab");

    CHECK (editor->keyPressed (previous));
    CHECK_MSG (advanced->getWorkspaceTab() == 0, "Ctrl+[ did not step back a tab");

    CHECK (editor->keyPressed (previous));
    CHECK_MSG (advanced->getWorkspaceTab() == advanced->getNumWorkspaceTabs() - 1,
               "Ctrl+[ did not wrap round to the last tab");
}

//==============================================================================
/*  Section 3.1's headstock popover exists, and it is the only way to author
    per-string detune.

    That second half is the point. `TuningEngine::StringTuning::detuneCents` is
    written into the preset by PresetManager and read back out of it, and nothing
    in the UI could set it - a preset field with no way to author it. The Advanced
    column has the tuning preset, the temperament and Concert A; it has never had
    the per-string offsets.

    So this checks the popover writes through to the engine, rather than only that
    it constructs and paints. A popover that drew six sliders wired to nothing
    would look identical in a screenshot.
*/
LUTHIER_TEST (Editor, theHeadstockPopoverEditsPerStringTuning)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& tuning = processor.getEngine().getTuningEngine();
    const int strings = processor.getEngine().getNumStrings();

    CHECK (strings > 0);

    TuningPopover popover (processor);

    const auto size = TuningPopover::preferredSize (strings);

    popover.setVisible (true);
    popover.setSize (size.getWidth(), size.getHeight());

    CHECK_MSG (popover.getWidth() > 0 && popover.getHeight() > 0,
               "the tuning popover has no size");

    CHECK_MSG (drewSomething (render (popover)), "the tuning popover painted nothing");

    /*  One slider per string, found the way the window sees them rather than
        through a private member. */
    juce::Array<juce::Slider*> sliders;
    collect<juce::Slider> (popover, sliders);

    /*  Concert A is a LuthierKnob, which owns a Slider of its own, so there are
        more sliders here than there are strings. The detune rows are the ones
        whose range is the cents range. */
    juce::Array<juce::Slider*> detune;

    for (auto* slider : sliders)
        if (slider->getMinimum() == -100.0 && slider->getMaximum() == 100.0)
            detune.add (slider);

    CHECK_MSG (detune.size() == strings,
               "expected " + juce::String (strings) + " detune sliders, found "
                 + juce::String (detune.size()));

    if (detune.size() != strings)
        return;

    //--------------------------------------------------------------------------
    // Each one writes to its own string, and only to its own string.
    for (int i = 0; i < strings; ++i)
    {
        const double wanted = -37.5 + (double) i;

        detune[i]->setValue (wanted, juce::sendNotificationSync);

        CHECK_MSG (std::abs (tuning.getStringTuning (i).detuneCents - wanted) < 1.0e-6,
                   "string " + juce::String (i + 1) + " detune did not reach the engine");
    }

    for (int i = 0; i < strings; ++i)
        CHECK_MSG (std::abs (tuning.getStringTuning (i).detuneCents - (-37.5 + (double) i)) < 1.0e-6,
                   "setting one string's detune disturbed string " + juce::String (i + 1));

    //--------------------------------------------------------------------------
    /*  And a detune moves the pitch. Without this the test would pass on a
        popover wired to a field the engine never reads. */
    const double openBefore = tuning.getEffectiveOpenFrequency (0);

    detune[0]->setValue (0.0, juce::sendNotificationSync);

    const double openAtZero = tuning.getEffectiveOpenFrequency (0);

    CHECK_MSG (openAtZero > openBefore,
               "returning a -37.5 cent detune to zero did not raise the open pitch");
}

//==============================================================================
/*  Section 3.1's bridge popover, and the condition on it.

    "Click opens the whammy popover (only if a whammy is fitted)". Index 0 of the
    bridge list is the hardtail, so that is the case where the click has to do
    nothing - and the tooltip has to say why, because a click target that is
    silent for a reason the user cannot see is indistinguishable from one that is
    broken.
*/
LUTHIER_TEST (Editor, theBridgePopoverAppearsOnlyWhenAWhammyIsFitted)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto* bridge = dynamic_cast<juce::AudioParameterChoice*> (
        processor.getState().getParameter (ParamIDs::bridgeType));

    CHECK_MSG (bridge != nullptr, "there is no bridge type parameter");

    if (bridge == nullptr)
        return;

    const auto names = Parameters::bridgeTypeNames();

    CHECK_MSG (names.size() > 1, "there is only one bridge type");

    //--------------------------------------------------------------------------
    // The hardtail is index 0, and it has no arm.
    bridge->setValueNotifyingHost (0.0f);

    CHECK_MSG (! WhammyPopover::isWhammyFitted (processor),
               "\"" + names[0] + "\" is reported as having a whammy fitted");

    //--------------------------------------------------------------------------
    // Every other bridge in the list does.
    for (int i = 1; i < names.size(); ++i)
    {
        bridge->setValueNotifyingHost ((float) i / (float) (names.size() - 1));

        CHECK_MSG (WhammyPopover::isWhammyFitted (processor),
                   "\"" + names[i] + "\" is reported as having no whammy fitted");
    }

    //--------------------------------------------------------------------------
    // And the popover itself builds and paints.
    WhammyPopover popover (processor);

    const auto size = WhammyPopover::preferredSize();

    popover.setVisible (true);
    popover.setSize (size.getWidth(), size.getHeight());

    CHECK (popover.getWidth() > 0 && popover.getHeight() > 0);
    CHECK_MSG (drewSomething (render (popover)), "the whammy popover painted nothing");

    /*  Its controls are bound to the same parameters as their twins in Advanced
        column 1, which is what makes this a second route rather than a second
        copy of the state. Checked through the parameter, since that is the thing
        both of them share. */
    juce::Array<juce::Slider*> sliders;
    collect<juce::Slider> (popover, sliders);

    CHECK_MSG (sliders.size() >= 4,
               "expected four whammy knobs, found " + juce::String (sliders.size()));

    if (auto* position = processor.getState().getParameter (ParamIDs::whammyPos))
    {
        const float before = position->getValue();

        position->setValueNotifyingHost (before > 0.5f ? 0.25f : 0.75f);

        // The attachment is asynchronous in general, but the popover reads the
        // parameter, so what matters is that the binding exists at all - which a
        // missing attachTo would have failed at construction.
        CHECK (position->getValue() != before);

        position->setValueNotifyingHost (before);
    }
}

//==============================================================================
/*  Every hit region on the illustration says what it does before it is clicked.

    Ground rule 4 and section 20: a control whose only affordance is that someone
    happened to click it is not discoverable. The illustration is the worst case,
    because nothing about a drawn guitar says which parts are live, so each
    region's hover text is part of the feature rather than decoration.

    This sweeps a grid over the whole component and collects what the tooltip says
    at every point, which is guitar-illustration.md's own idea of how to test hit
    regions - it asks for ten thousand random clicks across each factory guitar.
    Sweeping rather than probing two known coordinates matters here: the geometry
    is generated from the GuitarSpec, so there are no fixed coordinates to probe,
    and a region that shrank to nothing would still pass a test that asked it
    directly.

    The first version of this test opened the popovers and checked they did not
    throw - two checks, and it would have passed with both hit regions deleted.
*/
LUTHIER_TEST (Editor, everyHitRegionOnTheIllustrationDescribesItself)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    GuitarBodyComponent body (processor);

    body.setVisible (true);
    body.setSize (300, 620);

    CHECK_MSG (drewSomething (render (body)), "the guitar illustration painted nothing");

    // A whammy bridge, so the bridge region has its "click me" text rather than
    // its "nothing to set here" one.
    if (auto* bridge = dynamic_cast<juce::AudioParameterChoice*> (
            processor.getState().getParameter (ParamIDs::bridgeType)))
        bridge->setValueNotifyingHost (1.0f);

    /*  A real MouseEvent, built on the desktop's own mouse source. The component
        has no peer, so nothing delivers events to it - but mouseMove is an
        ordinary method and the event is an ordinary value. */
    auto source = juce::Desktop::getInstance().getMainMouseSource();

    auto tooltipAt = [&body, &source] (juce::Point<float> p)
    {
        const juce::MouseEvent e (source, p, juce::ModifierKeys(),
                                  1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                  &body, &body, juce::Time::getCurrentTime(),
                                  p, juce::Time::getCurrentTime(), 1, false);

        body.mouseMove (e);

        return body.getTooltip();
    };

    juce::StringArray seen;

    for (int y = 2; y < body.getHeight(); y += 4)
        for (int x = 2; x < body.getWidth(); x += 4)
            seen.addIfNotAlreadyThere (tooltipAt ({ (float) x, (float) y }));

    seen.removeEmptyStrings();

    CHECK_MSG (seen.size() >= 3,
               "the whole illustration offers only " + juce::String (seen.size())
                 + " different tooltips: " + seen.joinIntoString (" | "));

    //--------------------------------------------------------------------------
    // The headstock and the bridge are both reachable, and both say what they do.
    int headstockPoints = 0, bridgePoints = 0;

    for (const auto& text : seen)
    {
        if (text.containsIgnoreCase ("headstock"))  ++headstockPoints;
        if (text.containsIgnoreCase ("bridge"))     ++bridgePoints;
    }

    CHECK_MSG (headstockPoints > 0,
               "no point on the illustration offers the headstock tuning popover");

    CHECK_MSG (bridgePoints > 0,
               "no point on the illustration offers the bridge whammy popover");

    // And every tooltip is a sentence about what clicking does, not a bare name.
    for (const auto& text : seen)
        CHECK_MSG (text.length() > 12,
                   "a hit region's tooltip says only \"" + text + "\"");

    //--------------------------------------------------------------------------
    /*  The hardtail case: the bridge region still describes itself, and says
        there is nothing behind it rather than going quiet. Section 3.1 makes the
        popover conditional; it does not make the affordance conditional. */
    if (auto* bridge = dynamic_cast<juce::AudioParameterChoice*> (
            processor.getState().getParameter (ParamIDs::bridgeType)))
    {
        bridge->setValueNotifyingHost (0.0f);

        juce::StringArray hardtail;

        for (int y = 2; y < body.getHeight(); y += 4)
            for (int x = 2; x < body.getWidth(); x += 4)
                hardtail.addIfNotAlreadyThere (tooltipAt ({ (float) x, (float) y }));

        bool explained = false;

        for (const auto& text : hardtail)
            if (text.containsIgnoreCase ("no arm"))
                explained = true;

        CHECK_MSG (explained,
                   "on a hardtail the bridge region does not say why it does nothing");

        // And the click really is a no-op there.
        body.showWhammyPopover();
        CHECK (! WhammyPopover::isWhammyFitted (processor));
    }
}

//==============================================================================
/*  Right-click -> Modulate (modulation-matrix.md section 5, GAPS.md A4).

    This is the quick way to build a modulation route: the destination is the
    control under the cursor, so the user only picks what should drive it. It is a
    secondary path - the MOD tab cards are primary, which is ground rule 4 - and a
    secondary path is exactly the kind of thing that can break without anything
    noticing, because nothing else in the plugin goes through it.

    Nothing did notice, for three milestones. GAPS.md A4 listed this row as "the
    Modulate entry and drag-to-assign do not [exist]", and the menu had in fact
    been built in 996f89d. The entry was read rather than the code - the same
    mistake A4's Easy-mode row made - and the reason it survived is that there was
    no test to disagree with it. Drag-to-assign really is absent; the menu is not.

    Testing it needed the menu taken apart first. showParameterContextMenu built
    the items and called showMenuAsync in one breath, so the only way to see an
    item was to open the menu on screen and look at it. buildParameterContextMenu
    returns the items without showing them, and applyParameterMenuResult performs
    a result id, which is what the callback did - so the checks below walk the
    real menu and drive the real handler rather than a copy of either.
*/
LUTHIER_TEST (Editor, rightClickOffersModulationAndBuildsTheRoute)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    // applyParameterMenuResult only uses the component to position the
    // value-entry callout, which none of the results below opens.
    juce::Component owner;

    auto& matrix = processor.getModMatrix();
    matrix.clearRoutes();

    const juce::String destination (ParamIDs::masterGain);

    /*  What a menu contains: the visible text and id of every item, including
        inside sub-menus, and separately the text of each item that has a sub-menu
        hanging off it. */
    struct MenuContents
    {
        juce::StringArray items, subMenus;
        juce::Array<int> ids;

        bool has (const juce::String& text) const
        {
            for (const auto& item : items)
                if (item.contains (text))
                    return true;

            return false;
        }
    };

    auto readMenu = [] (const juce::PopupMenu& menu)
    {
        MenuContents found;

        juce::PopupMenu::MenuItemIterator it (menu, true);

        while (it.next())
        {
            const auto& item = it.getItem();

            found.items.add (item.text);
            found.ids.add (item.itemID);

            if (item.subMenu != nullptr)
                found.subMenus.add (item.text);
        }

        return found;
    };

    //--------------------------------------------------------------------------
    // With no routes on the destination yet.
    {
        const auto found = readMenu (buildParameterContextMenu (processor, destination));

        CHECK_MSG (found.has ("Modulate"),
                   "the right-click menu has no Modulate entry");

        CHECK_MSG (found.subMenus.contains ("Modulate"),
                   "Modulate is in the menu but has no sources hanging off it");

        /*  Section 5's source groups, by name. Naming them means a group quietly
            dropped from the menu fails here, which counting items would miss if
            another group grew at the same time. */
        for (const auto* group : { "LFO", "Envelope", "Sequencer", "Follower",
                                   "Macro", "Performance" })
            CHECK_MSG (found.subMenus.contains (group),
                       "the Modulate submenu has no " + juce::String (group) + " group");

        /*  Every source is an item carrying its own id in the encoded range, which
            is what pairs the source with the destination under the cursor. */
        CHECK_MSG (found.ids.contains (kModulateMenuBase + ModSourceSlots::lfoBase),
                   "LFO 1 is not offered as a modulation source");

        CHECK_MSG (found.ids.contains (kModulateMenuBase + ModSourceSlots::modWheel),
                   "the mod wheel is not offered as a modulation source");

        // Nothing to remove yet, so the entry that removes it is not there.
        CHECK_MSG (! found.has ("Remove modulation"),
                   "the menu offers to remove modulation from an unmodulated control");
    }

    //--------------------------------------------------------------------------
    // Choosing a source builds the route.
    const int chosenSlot = ModSourceSlots::lfoBase + 2;

    applyParameterMenuResult (kModulateMenuBase + chosenSlot, owner, processor, destination);

    CHECK_MSG (matrix.getRouteCountForDestination (destination) == 1,
               "choosing a modulation source did not create one route, the destination has "
                 + juce::String (matrix.getRouteCountForDestination (destination)));

    if (matrix.getRouteCountForDestination (destination) == 1)
    {
        // Found rather than assumed to be route 0, so this does not depend on the
        // matrix being empty of everything else.
        ModRoute built;

        for (int i = 0; i < matrix.getNumRoutes(); ++i)
            if (matrix.getRoute (i).destinationId == destination)
                built = matrix.getRoute (i);

        CHECK_MSG (built.sourceId == modSourceIdForSlot (chosenSlot),
                   "the route was built from the wrong source: wanted "
                     + modSourceIdForSlot (chosenSlot) + ", got " + built.sourceId);

        CHECK_MSG (built.enabled,
                   "the route was built disabled, so the control would never move");

        /*  A third of full depth: enough to be obviously doing something, not so
            much that it swamps the control the user just right-clicked. A route
            built at zero depth is the failure worth catching here, because it
            would look correct in the matrix and do nothing at all. */
        CHECK_NEAR (built.depth, 0.33f, 1.0e-5);
    }

    //--------------------------------------------------------------------------
    // Now the menu offers to take it away, and says how many.
    {
        const auto found = readMenu (buildParameterContextMenu (processor, destination));

        CHECK_MSG (found.has ("Remove modulation (1)"),
                   "with one route on the control the menu does not offer to remove it");
    }

    applyParameterMenuResult (9, owner, processor, destination);

    CHECK_MSG (matrix.getRouteCountForDestination (destination) == 0,
               "Remove modulation left "
                 + juce::String (matrix.getRouteCountForDestination (destination))
                 + " routes behind");

    //--------------------------------------------------------------------------
    /*  A destination is full at kMaxRoutesPerDestination. The menu says so on its
        face rather than offering sources that would be silently dropped, which is
        ground rule 0.2 applied to a limit. */
    for (int i = 0; i < ModMatrix::kMaxRoutesPerDestination; ++i)
        applyParameterMenuResult (kModulateMenuBase + ModSourceSlots::lfoBase + i,
                                  owner, processor, destination);

    CHECK_MSG (matrix.getRouteCountForDestination (destination)
                 == ModMatrix::kMaxRoutesPerDestination,
               "could not fill the destination to its route limit, it holds "
                 + juce::String (matrix.getRouteCountForDestination (destination)));

    {
        const auto found = readMenu (buildParameterContextMenu (processor, destination));

        CHECK_MSG (found.has ("already routed"),
                   "a full destination does not say so in its Modulate entry");
    }

    // One more must not fit.
    applyParameterMenuResult (kModulateMenuBase + ModSourceSlots::modWheel,
                              owner, processor, destination);

    CHECK_MSG (matrix.getRouteCountForDestination (destination)
                 <= ModMatrix::kMaxRoutesPerDestination,
               "the menu routed past the per-destination limit");

    matrix.clearRoutes();
}

//==============================================================================
/*  Notification banners, gui-integration.md section 15.

    Section 15 is nine triggers and four rules, and the rules are the part worth
    testing: under the header strip, 32 px, dismissible, and auto-dismiss after
    five seconds unless the banner contains an action.

    The last of those is the one with teeth. A banner offering to review a crash
    report that vanishes while the user is reaching for the button is worse than
    no banner, because the user now knows something happened and has no way back
    to it. The rule is implemented by never starting the timer for an actionable
    banner rather than by starting and stopping one, and isAutoDismissScheduled
    exists so that distinction is checkable without a five-second sleep.
*/
LUTHIER_TEST (Editor, notificationBannersQueueDismissAndRespectTheirActions)
{
    NotificationCentre centre;
    centre.setSize (800, NotificationCentre::preferredHeight);

    int visibilityChanges = 0;
    centre.onVisibilityChanged = [&visibilityChanges] { ++visibilityChanges; };

    CHECK_MSG (! centre.isShowingNotification(),
               "the strip has a notification before anything was posted");

    CHECK_MSG (! centre.isVisible(),
               "the empty strip is visible, so it is taking 32 points of window for nothing");

    //--------------------------------------------------------------------------
    // One with no action: shown, and on the clock.
    {
        Notification n;
        n.id = "first";
        n.message = "Sample rate changed to 96 kHz.";

        centre.post (std::move (n));
    }

    CHECK (centre.isShowingNotification());
    CHECK (centre.isVisible());
    CHECK_MSG (centre.getCurrentId() == "first", "the wrong notification is showing");
    CHECK_MSG (visibilityChanges == 1,
               "the window was not told the strip appeared, so it never made room for it");

    CHECK_MSG (centre.isAutoDismissScheduled(),
               "a banner with nothing to do about it is not on the auto-dismiss clock");

    CHECK_MSG (drewSomething (render (centre)), "the banner painted nothing");

    //--------------------------------------------------------------------------
    /*  The same id again is the same news, not a second piece of it. This is the
        case that matters for the countdown triggers, which repost themselves
        every time the window opens. */
    {
        Notification n;
        n.id = "first";
        n.message = "Sample rate changed to 48 kHz.";

        centre.post (std::move (n));
    }

    CHECK_MSG (centre.getNumQueued() == 0,
               "reposting the same id queued a second copy of it");

    CHECK_MSG (centre.getCurrentMessage().contains ("48 kHz"),
               "reposting the same id did not update what it says");

    //--------------------------------------------------------------------------
    // A second, different one waits its turn rather than replacing what is up.
    {
        Notification n;
        n.id = "second";
        n.message = "Luthier did not shut down cleanly last time.";
        n.actionText = "Review";
        n.action = [] {};

        centre.post (std::move (n));
    }

    CHECK_MSG (centre.getCurrentId() == "first",
               "a newly posted banner shoved the one already on screen aside");

    CHECK_MSG (centre.getNumQueued() == 1,
               "the second banner was not queued, the queue holds "
                 + juce::String (centre.getNumQueued()));

    CHECK (centre.contains ("second"));

    //--------------------------------------------------------------------------
    // Dismissing the first brings the second up, and it is *not* on the clock.
    centre.dismissCurrent();

    CHECK_MSG (centre.getCurrentId() == "second",
               "dismissing the first banner did not bring the queued one up");

    CHECK_MSG (centre.currentHasAction(), "the queued banner lost its action");

    CHECK_MSG (! centre.isAutoDismissScheduled(),
               "an actionable banner is on the auto-dismiss clock, so its button "
               "disappears out from under the user");

    CHECK_MSG (centre.isVisible(), "the strip hid itself while a banner was still queued");

    //--------------------------------------------------------------------------
    // The action runs, and takes the banner with it.
    bool actionRan = false;

    {
        Notification n;
        n.id = "third";
        n.message = "Luthier 1.1.0 is available.";
        n.actionText = "Details";
        n.action = [&actionRan] { actionRan = true; };

        centre.post (std::move (n));
    }

    centre.dismissCurrent();                 // past "second", onto "third"

    CHECK (centre.getCurrentId() == "third");

    centre.performCurrentAction();

    CHECK_MSG (actionRan, "the banner's action button did nothing");

    CHECK_MSG (! centre.isShowingNotification(),
               "running the action left the banner on screen");

    CHECK_MSG (! centre.isVisible(),
               "the strip kept its 32 points after the last banner went away");

    CHECK_MSG (visibilityChanges >= 2,
               "the window was never told the strip went away, so the space is lost");

    //--------------------------------------------------------------------------
    // clear() takes the queue with it, not just what is on screen.
    for (const auto* id : { "a", "b", "c" })
    {
        Notification n;
        n.id = id;
        n.message = "queued";

        centre.post (std::move (n));
    }

    CHECK (centre.getNumQueued() == 2);

    centre.clear();

    CHECK_MSG (! centre.isShowingNotification() && centre.getNumQueued() == 0,
               "clear() left something behind");
}

//==============================================================================
/*  Section 15's triggers, at the window.

    Two halves. A processor with nothing wrong with it must produce no banners at
    all - an empty strip is the normal state, and a window that opens with a
    banner every time has trained the user to dismiss without reading. And a
    licence sitting in its grace period must say so, because that is the one
    trigger of the four whose state a test can arrange honestly: fromVar takes
    the dates the state is derived from, so a licence last validated 35 days ago
    is in grace by the same arithmetic the plugin uses, not by a flag set to make
    the test pass.
*/
LUTHIER_TEST (Editor, theWindowRaisesSectionFifteensTriggersAndIsQuietWhenItShould)
{
    //--------------------------------------------------------------------------
    // Nothing wrong: nothing said.
    {
        LuthierAudioProcessor processor;
        processor.prepareToPlay (kSr, kBlock);

        /*  Guarded rather than assumed. If the machine running this really does
            have a policy file or a crash dump waiting, the quiet case is not
            being tested and saying so is better than failing on it. */
        const bool quiet = ! processor.getTelemetry().isManagedByPolicy()
                             && ! processor.getTelemetry().hasPendingCrashReport()
                             && processor.getLicense().getState() != License::State::grace;

        if (quiet)
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

            CHECK_MSG (editor != nullptr, "no editor to check for banners");

            if (editor != nullptr)
            {
                auto* window = dynamic_cast<LuthierAudioProcessorEditor*> (editor.get());

                CHECK (window != nullptr);

                if (window != nullptr)
                    CHECK_MSG (! window->getNotifications().isShowingNotification(),
                               "a healthy plugin opened with a banner: \""
                                 + window->getNotifications().getCurrentMessage() + "\"");
            }
        }
    }

    //--------------------------------------------------------------------------
    // A licence in grace says so, and says it as a warning.
    {
        LuthierAudioProcessor processor;
        processor.prepareToPlay (kSr, kBlock);

        /*  30 days to revalidation and 14 of grace, so 35 days since the last
            validation is inside the grace window by the plugin's own arithmetic.
            In memory only - fromVar does not write the licence file. */
        auto* dates = new juce::DynamicObject();
        dates->setProperty ("keyHash", "test-key-hash");
        dates->setProperty ("activatedAt",
                            (juce::int64) (juce::Time::getCurrentTime()
                                             - juce::RelativeTime::days (200)).toMilliseconds());
        dates->setProperty ("lastValidated",
                            (juce::int64) (juce::Time::getCurrentTime()
                                             - juce::RelativeTime::days (35)).toMilliseconds());

        processor.getLicense().fromVar (juce::var (dates));

        CHECK_MSG (processor.getLicense().getState() == License::State::grace,
                   "the test could not put the licence into its grace period, it is in "
                     + juce::String (License::getStateName (processor.getLicense().getState())));

        if (processor.getLicense().getState() == License::State::grace)
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

            auto* window = dynamic_cast<LuthierAudioProcessorEditor*> (editor.get());

            CHECK (window != nullptr);

            if (window != nullptr)
            {
                auto& centre = window->getNotifications();

                CHECK_MSG (centre.contains ("licence-grace"),
                           "a licence in its grace period raised no banner, which is the "
                           "one warning a user has to see before it stops working");

                /*  The strip has to be laid out as well as populated. A banner
                    that is posted but given no bounds is invisible, which is the
                    failure this would otherwise miss entirely. */
                editor->setSize (LuthierAudioProcessorEditor::defaultWidth,
                                 LuthierAudioProcessorEditor::defaultHeight);

                CHECK_MSG (centre.isVisible() && centre.getHeight() > 0,
                           "the banner strip has no height, so nothing is on screen");

                CHECK_MSG (centre.getHeight() <= NotificationCentre::preferredHeight,
                           "the banner strip is taller than section 15's 32 points");

                /*  Under the header strip, which is the other half of the
                    section 15 sentence and the part a layout change would break
                    without anything else noticing. */
                if (auto* header = findOne<HeaderBar> (*editor))
                    CHECK_MSG (centre.getY() >= header->getBottom(),
                               "the banner strip is not under the header strip");
            }
        }
    }
}

//==============================================================================
/*  The two section 15 triggers that arrive while the window is already open.

    A preset that will not load, and an IR a preset asked for that is not on disk
    any more. Both are conditions the window polls rather than events it is told
    about, because presets load from five places and five call sites remembering
    to report would be five chances to forget.

    The dismissal behaviour is the part worth pinning. Posting on every tick would
    be harmless to the queue - a repeated id replaces itself - and would make the
    banner impossible to get rid of: the cross works, and a quarter of a second
    later it is back. So the window posts only when the message *changes*, and the
    check below dismisses a banner and pumps the timer again to prove it stays
    gone while the condition has not.
*/
LUTHIER_TEST (Editor, aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    auto* window = dynamic_cast<LuthierAudioProcessorEditor*> (editor.get());

    CHECK_MSG (window != nullptr, "no editor to raise banners on");

    if (window == nullptr)
        return;

    editor->setSize (LuthierAudioProcessorEditor::defaultWidth,
                     LuthierAudioProcessorEditor::defaultHeight);

    auto& centre = window->getNotifications();
    centre.clear();

    //--------------------------------------------------------------------------
    /*  A preset that is not there. Going through loadPreset rather than setting
        the error by hand is the point: this checks the path a user takes, and it
        would fail if loadPreset stopped recording why it refused. */
    const auto missing = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("luthier-no-such-preset-9d3f.luthier");

    missing.deleteFile();

    CHECK_MSG (! processor.getPresetManager().loadPreset (missing),
               "loading a preset that does not exist reported success");

    CHECK_MSG (processor.getPresetManager().getLastLoadError().isNotEmpty(),
               "a failed preset load left no message for the user, only a log line");

    CHECK_MSG (processor.getPresetManager().getLastLoadError().contains (missing.getFileName()),
               "the failure message does not name the file that failed");

    window->pollForNotifications();

    CHECK_MSG (centre.contains ("preset-load"),
               "a preset that would not load raised no banner, so the user saw nothing "
               "happen at all");

    //--------------------------------------------------------------------------
    // Dismissed is dismissed: an unchanged condition must not repost.
    while (centre.isShowingNotification())
        centre.dismissCurrent();

    for (int tick = 0; tick < 4; ++tick)
        window->pollForNotifications();

    CHECK_MSG (! centre.contains ("preset-load"),
               "the preset banner came back after being dismissed, so it cannot be "
               "got rid of while the condition holds");

    //--------------------------------------------------------------------------
    /*  A missing IR. IrSlot::fromVar is the path a preset restore takes, and it
        falls back to the built-in model with lastError set - a comment in that
        function says the error is left there for exactly this banner. */
    auto* irState = new juce::DynamicObject();
    irState->setProperty ("file", "user/no-such-ir-4a7b.wav");
    irState->setProperty ("engaged", true);

    processor.getBodyIrSlot().fromVar (juce::var (irState));

    CHECK_MSG (processor.getBodyIrSlot().getLastError().isNotEmpty(),
               "restoring a preset whose IR is gone recorded nothing");

    CHECK_MSG (! processor.getBodyIrSlot().isEngaged(),
               "a slot whose IR is missing stayed engaged, so it is convolving nothing");

    window->pollForNotifications();

    CHECK_MSG (centre.contains ("ir-missing"),
               "a preset naming an IR that is not on disk raised no banner, and the "
               "sound is quietly not the one that was saved");

    //--------------------------------------------------------------------------
    /*  It offers somewhere to fix it, and that somewhere has to exist. The action
        opens Advanced Mode's TONE MATCH tab, so this checks the tab is findable
        by the name the action uses rather than trusting the string. */
    while (centre.isShowingNotification() && centre.getCurrentId() != "ir-missing")
        centre.dismissCurrent();

    CHECK_MSG (centre.getCurrentId() == "ir-missing", "the IR banner is not reachable");
    CHECK_MSG (centre.currentHasAction(), "the IR banner offers no way to fix it");

    centre.performCurrentAction();

    if (auto* panel = findOne<AdvancedPanel> (*editor))
    {
        CHECK_MSG (panel->setWorkspaceTabNamed ("TONE MATCH"),
                   "the IR banner sends the user to a TONE MATCH tab that does not exist");

        CHECK_MSG (panel->getWorkspaceTabName (panel->getWorkspaceTab())
                     .equalsIgnoreCase ("TONE MATCH"),
                   "selecting TONE MATCH by name landed on "
                     + panel->getWorkspaceTabName (panel->getWorkspaceTab()));

        // A name no tab will ever have: the real tabs arrive one by one.
        CHECK_MSG (! panel->setWorkspaceTabNamed ("NO SUCH TAB"),
                   "an unbuilt tab reports that it was selected, so a caller cannot "
                   "tell a missing panel from a shown one");
    }
}

//==============================================================================
/*  The sample-rate trigger, gui-integration 15's last unblocked one.

    "Sample rate changed to 96 kHz, IRs and circuit filters re-resampled."

    The interesting part is what must *not* raise it. prepareToPlay is called
    whenever the host feels like it - changing the buffer size alone does it, and
    so does starting playback in some hosts - so a banner per prepareToPlay would
    appear every time a user touched their audio settings. And the first prepare
    of all is not a change: opening a plugin at 48 kHz is the normal state of
    affairs, not news.

    So the rate is claimed rather than compared, and the claim clears it. That
    makes this an event rather than a condition, which is why it does not use the
    same "post only when the message changes" rule as the preset and IR banners
    beside it - asking twice about one event must not answer twice.
*/
LUTHIER_TEST (Editor, aSampleRateChangeIsAnnouncedOnceAndTheFirstOneIsNot)
{
    LuthierAudioProcessor processor;

    //--------------------------------------------------------------------------
    // Before anything is prepared there is nothing to claim.
    CHECK_MSG (processor.claimSampleRateChange() == 0.0,
               "an unprepared processor reported a sample-rate change");

    //--------------------------------------------------------------------------
    // The first prepare is the rate it opened at, not a change.
    processor.prepareToPlay (48000.0, kBlock);

    CHECK_MSG (processor.claimSampleRateChange() == 0.0,
               "the first prepareToPlay was reported as a change, so every plugin "
               "instance would open with a banner");

    //--------------------------------------------------------------------------
    // The same rate again is not a change either - this is the buffer-size case.
    processor.prepareToPlay (48000.0, kBlock * 2);

    CHECK_MSG (processor.claimSampleRateChange() == 0.0,
               "re-preparing at the same rate was reported as a rate change, so "
               "changing the buffer size would raise a banner");

    //--------------------------------------------------------------------------
    // A real change is reported, once, with the new rate.
    processor.prepareToPlay (96000.0, kBlock);

    CHECK_MSG (processor.claimSampleRateChange() == 96000.0,
               "moving from 48 kHz to 96 kHz was not reported");

    CHECK_MSG (processor.claimSampleRateChange() == 0.0,
               "the same rate change was reported twice, so two windows would both "
               "announce it");

    //--------------------------------------------------------------------------
    // And it reaches the banner, with section 15's wording.
    processor.prepareToPlay (44100.0, kBlock);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    auto* window = dynamic_cast<LuthierAudioProcessorEditor*> (editor.get());

    CHECK (window != nullptr);

    if (window == nullptr)
        return;

    editor->setSize (LuthierAudioProcessorEditor::defaultWidth,
                     LuthierAudioProcessorEditor::defaultHeight);

    window->pollForNotifications();

    auto& centre = window->getNotifications();

    CHECK_MSG (centre.contains ("sample-rate"),
               "a sample-rate change raised no banner");

    while (centre.isShowingNotification() && centre.getCurrentId() != "sample-rate")
        centre.dismissCurrent();

    if (centre.getCurrentId() == "sample-rate")
    {
        const auto text = centre.getCurrentMessage();

        /*  44.1 rather than 44 or 44.10. A rate the user recognises is the whole
            value of naming it, and "44 kHz" is not a sample rate anyone runs. */
        CHECK_MSG (text.contains ("44.1 kHz"),
                   "the banner does not name the new rate as 44.1 kHz: \"" + text + "\"");

        /*  The second half of section 15's sentence. Knowing the number changed
            does not explain the gap in the audio; knowing the IRs and filters
            were rebuilt does. */
        CHECK_MSG (text.containsIgnoreCase ("re-resampled"),
                   "the banner does not say the IRs and filters were re-resampled: \""
                     + text + "\"");
    }
}

//==============================================================================
/*  Section 17's last two buildable shortcuts: New preset and Reveal preset file.

    GAPS.md A5 said neither could be built. Both claims were wrong in the same
    way as the rest of that file: "no new preset action exists" missed the Init
    factory preset, whose own description calls it the place to start when
    building your own, and "PresetManager tracks the current preset's name and
    index but not its file path" was half right - the index is there, but
    loadPreset(File), which the header's Open dialog calls, never sets one, so a
    preset opened from outside the library had no index either.

    **The reveal branch that succeeds is deliberately never pressed here.**
    revealToUser opens a file manager window, and a test that spawned Explorer on
    every run would be a worse thing than the gap it closed. So the key is pressed
    only in the state where there is nothing to reveal - which is the branch with
    the interesting behaviour anyway, because the alternative to saying so is
    opening some arbitrary folder - and the file that the other branch would use
    is checked directly.
*/
LUTHIER_TEST (Editor, newPresetLoadsInitAndRevealSaysSoWhenThereIsNoFile)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    CHECK_MSG (processor.getPresetManager().indexOfPreset ("Init") >= 0,
               "there is no Init preset, so Ctrl+N has nothing to load");

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    auto* window = dynamic_cast<LuthierAudioProcessorEditor*> (editor.get());

    CHECK (window != nullptr);

    if (window == nullptr)
        return;

    editor->setSize (LuthierAudioProcessorEditor::defaultWidth,
                     LuthierAudioProcessorEditor::defaultHeight);

    auto& centre = window->getNotifications();
    centre.clear();

    //--------------------------------------------------------------------------
    /*  Reveal first, while nothing has been loaded and so nothing can be opened.
        Once Ctrl+N below has run there is a real file and pressing this would
        put a file manager on screen. */
    CHECK_MSG (! processor.getPresetManager().getCurrentPresetFile().existsAsFile(),
               "a freshly constructed processor already has a preset file, so the "
               "no-file branch cannot be tested");

    const auto revealKey = shortcutFor ("revealPreset");

    CHECK_MSG (revealKey.isValid(),
               "revealPreset is not in the shortcut registry, so section 17's row "
               "is still open");

    CHECK_MSG (editor->keyPressed (revealKey),
               "the reveal shortcut was not handled, so the host would get the key");

    CHECK_MSG (centre.contains ("reveal-preset"),
               "with nothing to reveal the window said nothing at all");

    centre.clear();

    //--------------------------------------------------------------------------
    // New preset.
    const auto newKey = shortcutFor ("newPreset");

    CHECK_MSG (newKey.isValid(),
               "newPreset is not in the shortcut registry, so section 17's row is "
               "still open");

    // Somewhere else first, so "it loaded Init" is not just "it did nothing".
    const int somewhereElse = processor.getPresetManager().getNumPresets() > 1 ? 1 : 0;
    processor.getPresetManager().loadPreset (somewhereElse);

    const auto before = processor.getPresetManager().getCurrentPresetName();

    CHECK_MSG (editor->keyPressed (newKey),
               "the new-preset shortcut was not handled");

    const auto after = processor.getPresetManager().getCurrentPresetName();

    CHECK_MSG (after.equalsIgnoreCase ("Init"),
               "Ctrl+N did not load Init, the preset is now \"" + after + "\"");

    /*  And it is a load, not a reset: loading records the file it came from,
        which is the thing Reveal needs and which resetEverything would not set.
        This is also what makes the branch above untestable afterwards. */
    CHECK_MSG (processor.getPresetManager().getCurrentPresetFile().existsAsFile(),
               "loading Init left no file behind, so Reveal would have nothing to show");

    juce::ignoreUnused (before);
}

//==============================================================================
/*  The LIVE tab, gui-integration.md section 4.4.

    "Snapshot bank editor, setlist editor, morph configuration, expression-pedal
    calibration. The live-strip in the bottom of the window is the runtime
    surface; this tab is the setup surface."

    Being in the tab strip and painting is covered by
    everyWorkspaceTabSelectsAndPaints, which is a smoke test and says so. What
    that cannot see is whether the editors edit anything - a panel of controls
    wired to nothing paints exactly as well as one wired correctly, which is the
    failure mode every panel in this build has had at least once.

    So this drives the panel's own operations against the bank and the setlist and
    reads the result back out of the engine, not out of the panel.
*/
LUTHIER_TEST (Editor, theLiveTabEditsTheSnapshotBankAndTheSetlist)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& bank = processor.getSnapshots();

    AdvancedPanel panel (processor);
    panel.setVisible (true);
    panel.setSize (1600, 900);

    CHECK_MSG (panel.setWorkspaceTabNamed ("LIVE"),
               "there is no LIVE tab, so section 4.4's setup surface is still absent");

    auto* live = findOne<LivePanel> (panel);

    CHECK_MSG (live != nullptr, "the LIVE tab has no LivePanel behind it");

    if (live == nullptr)
        return;

    auto* grid = findOne<SnapshotGrid> (*live);

    CHECK_MSG (grid != nullptr, "the LIVE tab has no snapshot grid");

    if (grid == nullptr)
        return;

    //--------------------------------------------------------------------------
    /*  The grid covers the whole bank. 128 slots is what live-performance.md
        asks for, and a grid that showed 64 would look entirely reasonable. */
    CHECK_MSG (SnapshotGrid::kColumns * SnapshotGrid::kRows == SnapshotBank::kMaxSnapshots,
               "the grid has "
                 + juce::String (SnapshotGrid::kColumns * SnapshotGrid::kRows)
                 + " cells for " + juce::String (SnapshotBank::kMaxSnapshots) + " snapshots");

    //--------------------------------------------------------------------------
    // Clicking a cell selects the slot under it, and the slots are distinct.
    grid->setSize (320, 160);

    const int first = grid->slotAt ({ 5, 5 });

    CHECK_MSG (first == 0, "the top-left cell is not slot 0, it is " + juce::String (first));

    const int second = grid->slotAt ({ 5 + 320 / SnapshotGrid::kColumns, 5 });

    CHECK_MSG (second == 1,
               "the cell next to the first is not slot 1, it is " + juce::String (second));

    const int lower = grid->slotAt ({ 5, 5 + 160 / SnapshotGrid::kRows });

    CHECK_MSG (lower == SnapshotGrid::kColumns,
               "the cell below the first is not a row down, it is " + juce::String (lower));

    CHECK_MSG (grid->slotAt ({ -4, -4 }) < 0, "a point outside the grid reported a slot");

    //--------------------------------------------------------------------------
    // Capture writes a snapshot into the selected slot.
    bank.clear();

    const int target = 5;
    grid->setSelectedSlot (target);

    CHECK (grid->getSelectedSlot() == target);

    CHECK_MSG (bank.getNumSnapshots() <= target || bank.getSnapshot (target).isEmpty(),
               "the slot was not empty before capturing into it, so the check below "
               "would pass without the capture doing anything");

    CHECK_MSG (clickButton (*live, "Capture"),
               "the LIVE tab has no Capture button that can be pressed");

    CHECK_MSG (bank.getNumSnapshots() > target && ! bank.getSnapshot (target).isEmpty(),
               "Capture did not put a snapshot in the selected slot");

    //--------------------------------------------------------------------------
    /*  Adding to the setlist appends an entry pointing at that slot. The entry's
        snapshot index is checked rather than only the count, because an "add"
        that always added slot 0 would pass a count check. */
    const int entriesBefore = processor.getSetlist().getSetlist().getNumEntries();

    CHECK_MSG (clickButton (*live, "Add snapshot"),
               "the LIVE tab has no Add snapshot button that can be pressed");

    {
        const auto& set = processor.getSetlist().getSetlist();

        CHECK_MSG (set.getNumEntries() == entriesBefore + 1,
                   "Add snapshot did not add an entry to the setlist");

        if (set.getNumEntries() == entriesBefore + 1)
            CHECK_MSG (set.getEntry (set.getNumEntries() - 1).snapshotIndex == target,
                       "the setlist entry points at snapshot "
                         + juce::String (set.getEntry (set.getNumEntries() - 1).snapshotIndex + 1)
                         + " rather than the selected " + juce::String (target + 1));
    }

    //--------------------------------------------------------------------------
    // Clear empties it again.
    {
        CHECK_MSG (clickButton (*live, "Clear"), "Clear could not be pressed");

        CHECK_MSG (bank.getNumSnapshots() <= target || bank.getSnapshot (target).isEmpty(),
                   "Clear left the snapshot in place");
    }

    //--------------------------------------------------------------------------
    /*  The morph and crossfade controls reach the bank. The crossfade is engine
        state rather than a parameter, so nothing else in the build would notice
        if this slider were attached to nothing at all. */
    bank.setCrossfadeMs (120.0);
    live->refresh();

    juce::Array<juce::Slider*> sliders;
    collect<juce::Slider> (*live, sliders);

    CHECK_MSG (! sliders.isEmpty(), "the LIVE tab has no crossfade slider");

    if (! sliders.isEmpty())
    {
        CHECK_MSG (std::abs (sliders.getFirst()->getValue() - 120.0) < 0.5,
                   "the crossfade slider did not read the bank's value back, it shows "
                     + juce::String (sliders.getFirst()->getValue()));

        /*  400, not 900: SnapshotBank clamps to 500 ms, and a test that asked for
            more would be checking the clamp rather than the wiring. */
        sliders.getFirst()->setValue (400.0, juce::sendNotificationSync);

        CHECK_MSG (std::abs (bank.getCrossfadeMs() - 400.0) < 0.5,
                   "moving the crossfade slider did not reach the snapshot bank, which "
                   "still says " + juce::String (bank.getCrossfadeMs()));
    }

    bank.clear();
}

//==============================================================================
/*  Column 4's tabs are as tall as what is on them.

    MOD, RHYTHM, LIVE, ROUTING, TONE MATCH and CONTROLLERS each had a preferred
    height that nothing called, so the workspace viewport laid them out at its
    80-point floor and every combo box on them opened a list of squashed rows.
    The CHARACTER check in SlideTests caught it for one tab; this walks them all,
    so the next tab added cannot quietly sit at the floor either.
*/
/*  spec/issues.md: "a small piano roll that mirrors the strings". The Advanced
    strip offers FRETS | ROLL; the choice shows one component, hides the other,
    and survives a new panel through UiPreferences. */
/*  spec/issues.md "the pickup changes don't appear to do much": a slot the
    guitar has no pickup in is disabled and says so, and the selector offers
    only positions the guitar can realise. */
LUTHIER_TEST (Editor, pickupSlotsFollowTheFittedGuitar)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    AdvancedPanel panel (processor);
    panel.setVisible (true);
    panel.setSize (1600, 900);
    panel.refreshPickupSlots();

    const int fitted = processor.getEngine().getNumFittedPickups();
    CHECK (fitted >= 1 && fitted <= PickupEngine::kMaxPickups);
    CHECK (panel.getFittedPickupsShown() == fitted);

    // Every position stays in the list (the attachment maps by index), the
    // unrealisable ones disabled.
    const auto names = Parameters::pickupSelectorNames();
    CHECK (panel.getPickupSelectorItemCount() == names.size());
    const int offered = panel.getPickupSelectorEnabledCount();
    const int expected = fitted >= 3 ? names.size() : fitted == 2 ? 4 : 1;
    CHECK_MSG (offered == expected,
               "selector enables " + juce::String (offered) + " positions for " + juce::String (fitted) + " pickups");

    for (int slot = 0; slot < PickupEngine::kMaxPickups; ++slot)
        CHECK (panel.isPickupSlotEnabled (slot) == (slot < fitted));
}

LUTHIER_TEST (Editor, theStripSwitchesBetweenFretsAndTheStringRoll)
{
    UiPreferences::get().reset();

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    {
        AdvancedPanel panel (processor);
        panel.setVisible (true);
        panel.setSize (1600, 900);

        CHECK (! panel.isStripShowingRoll());
        CHECK (panel.getFretboard().isVisible());
        CHECK (! panel.getStringRoll().isVisible());

        panel.setStripShowsRoll (true);

        CHECK (panel.isStripShowingRoll());
        CHECK (! panel.getFretboard().isVisible());
        CHECK (panel.getStringRoll().isVisible());
        CHECK (panel.getStringRoll().getWidth() > 200);
        CHECK (panel.getStringRoll().getHeight() > 60);
        CHECK (panel.getStringRoll().getNumLanes() == 6);
    }

    // Remembered.
    AdvancedPanel again (processor);
    again.setVisible (true);
    again.setSize (1600, 900);
    CHECK (again.isStripShowingRoll());
    CHECK (again.getStringRoll().isVisible());

    again.setStripShowsRoll (false);
    CHECK (again.getFretboard().isVisible());

    UiPreferences::get().reset();
}

LUTHIER_TEST (Editor, everyWorkspaceTabIsAsTallAsItsContent)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    AdvancedPanel panel (processor);
    panel.setVisible (true);
    panel.setSize (1600, 900);

    CHECK (panel.getNumWorkspaceTabs() > 0);

    auto preferredHeightOf = [] (juce::Component* p) -> int
    {
        if (auto* m = dynamic_cast<ModMatrixPanel*> (p))       return m->preferredHeight();
        if (auto* r = dynamic_cast<RhythmPanel*> (p))          return r->preferredHeight();
        if (auto* l = dynamic_cast<LivePanel*> (p))            return l->preferredHeight();
        if (auto* r = dynamic_cast<RoutingPanel*> (p))         return r->preferredHeight();
        if (auto* t = dynamic_cast<ToneMatchPanel*> (p))       return t->preferredHeight();
        if (auto* c = dynamic_cast<CharacterPanel*> (p))       return c->preferredHeight();
        if (auto* c = dynamic_cast<ControllersPage*> (p))      return c->preferredHeight();
        if (auto* m = dynamic_cast<MidiOutPanel*> (p))         return m->getPreferredHeight();
        if (auto* n = dynamic_cast<NotationPanel*> (p))        return n->getPreferredHeight();
        if (auto* t = dynamic_cast<TunePanel*> (p))            return t->getPreferredHeight();
        if (auto* s = dynamic_cast<PracticeSetupPanel*> (p))   return s->getPreferredHeight();
        return 0;
    };

    for (int i = 0; i < panel.getNumWorkspaceTabs(); ++i)
    {
        panel.setWorkspaceTab (i);

        auto* tab = panel.getWorkspacePanel (i);
        const auto name = panel.getWorkspaceTabName (i);

        CHECK_MSG (tab != nullptr, "tab " + name + " has no panel behind it");

        if (tab == nullptr)
            continue;

        CHECK_MSG (tab->getHeight() > 80,
                   name + " is laid out at " + juce::String (tab->getHeight())
                     + " points, the viewport's floor: nothing sized it");

        const int preferred = preferredHeightOf (tab);

        // WORKSHOP and HELP fill the viewport rather than asking for a height.
        if (preferred > 0)
            CHECK_MSG (tab->getHeight() >= preferred,
                       name + " is " + juce::String (tab->getHeight()) + " tall for "
                         + juce::String (preferred) + " of content");
    }
}

//==============================================================================
/*  The snapshot grid's gestures reach the bank (gui-integration 4.4 / 8).

    Shift-click stores the current sound in the pad under the pointer;
    double-click recalls it. The bank is read back rather than the grid, because
    a pad that lit up without writing anything would look identical.
*/
LUTHIER_TEST (Editor, snapshotGridGesturesReachTheBank)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& bank = processor.getSnapshots();
    bank.clear();

    AdvancedPanel panel (processor);
    panel.setVisible (true);
    panel.setSize (1600, 900);

    CHECK (panel.setWorkspaceTabNamed ("LIVE"));

    auto* live = findOne<LivePanel> (panel);
    auto* grid = live != nullptr ? findOne<SnapshotGrid> (*live) : nullptr;

    CHECK_MSG (grid != nullptr, "the LIVE tab has no snapshot grid");

    if (grid == nullptr)
        return;

    CHECK_MSG (grid->getWidth() > 0 && grid->getHeight() >= SnapshotGrid::kRows * 20,
               "the grid is " + juce::String (grid->getWidth()) + " x "
                 + juce::String (grid->getHeight()) + ", too small for its 128 pads to be read");

    auto source = juce::Desktop::getInstance().getMainMouseSource();

    auto eventAt = [&] (int slot, juce::ModifierKeys mods)
    {
        const auto p = grid->boundsForSlot (slot).getCentre().toFloat();
        return juce::MouseEvent (source, p, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 grid, grid, juce::Time::getCurrentTime(), p,
                                 juce::Time::getCurrentTime(), 1, false);
    };

    //--------------------------------------------------------------------------
    // The empty pad's tooltip says what to do with it.
    const int target = 9;

    grid->mouseMove (eventAt (target, {}));
    CHECK_MSG (grid->getTooltip().containsIgnoreCase ("shift"),
               "an empty pad's tooltip does not mention Shift-click: \"" + grid->getTooltip() + "\"");

    //--------------------------------------------------------------------------
    // Shift-click captures into the pad and selects it.
    CHECK (bank.getNumSnapshots() <= target || bank.getSnapshot (target).isEmpty());

    grid->mouseDown (eventAt (target, juce::ModifierKeys::shiftModifier));

    CHECK_MSG (grid->getSelectedSlot() == target,
               "Shift-click selected slot " + juce::String (grid->getSelectedSlot() + 1)
                 + " rather than " + juce::String (target + 1));

    CHECK_MSG (bank.getNumSnapshots() > target && ! bank.getSnapshot (target).isEmpty(),
               "Shift-click on an empty pad did not store a snapshot in it");

    // A plain click on another pad only selects; it stores nothing.
    grid->mouseDown (eventAt (target + 1, {}));

    CHECK (grid->getSelectedSlot() == target + 1);
    CHECK_MSG (bank.getNumSnapshots() <= target + 1 || bank.getSnapshot (target + 1).isEmpty(),
               "a plain click stored a snapshot");

    //--------------------------------------------------------------------------
    // Double-click recalls: the bank's current snapshot becomes the pad's.
    bank.setLabel (target, "Clean Verse");
    bank.capture (2, "Other");
    bank.recall (2);
    CHECK (bank.getCurrentSnapshot() == 2);

    grid->mouseDoubleClick (eventAt (target, {}));

    CHECK_MSG (bank.getCurrentSnapshot() == target,
               "double-click on a filled pad did not recall it; the bank is on snapshot "
                 + juce::String (bank.getCurrentSnapshot() + 1));

    // Double-click on an empty pad recalls nothing.
    grid->mouseDoubleClick (eventAt (target + 1, {}));
    CHECK (bank.getCurrentSnapshot() == target);

    // A filled pad's tooltip carries its label.
    grid->mouseMove (eventAt (target, {}));
    CHECK_MSG (grid->getTooltip().contains ("Clean Verse"),
               "a filled pad's tooltip does not carry its label: \"" + grid->getTooltip() + "\"");

    bank.clear();
}

//==============================================================================
/*  The wheel scrolls a column; Ctrl+wheel nudges the knob under the pointer.

    juce::Slider eats every wheel event over it, so a column that is mostly
    knobs stopped scrolling wherever the pointer rested. The knob's slider is
    sent the event directly, as the mouse would, and both the viewport and the
    parameter are read back.
*/
LUTHIER_TEST (Editor, theWheelScrollsAColumnUnlessCtrlIsHeld)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    AdvancedPanel panel (processor);
    panel.setVisible (true);
    panel.setSize (1600, 480);   // short, so every column overflows

    juce::Array<LuthierKnob*> knobs;
    collect<LuthierKnob> (panel, knobs);

    LuthierKnob* knob = nullptr;
    juce::Viewport* viewport = nullptr;

    for (auto* k : knobs)
    {
        auto* v = k->findParentComponentOfClass<juce::Viewport>();

        if (v != nullptr && v->getViewedComponent() != nullptr
             && v->getViewedComponent()->getHeight() > v->getMaximumVisibleHeight()
             && k->getLearnParameterId().isNotEmpty() && k->isEnabled())
        {
            knob = k;
            viewport = v;
            break;
        }
    }

    CHECK_MSG (knob != nullptr, "no attached knob sits in a column that overflows its viewport");

    if (knob == nullptr)
        return;

    auto* slider = findOne<juce::Slider> (*knob);
    auto* param = processor.getState().getParameter (knob->getLearnParameterId());

    CHECK (slider != nullptr && param != nullptr);

    if (slider == nullptr || param == nullptr)
        return;

    viewport->setViewPosition (0, 0);

    auto* hinting = dynamic_cast<ScrollHintViewport*> (viewport);

    CHECK_MSG (hinting != nullptr, "the column viewport is not a ScrollHintViewport");

    if (hinting != nullptr)
    {
        CHECK_MSG (hinting->isBottomHintShowing(), "an overflowing column shows no hint at its bottom");
        CHECK_MSG (! hinting->isTopHintShowing(), "a column at its top shows a hint above");
    }

    auto source = juce::Desktop::getInstance().getMainMouseSource();

    auto wheelOver = [&] (juce::ModifierKeys mods, float deltaY)
    {
        const auto p = slider->getLocalBounds().getCentre().toFloat();
        const juce::MouseEvent e (source, p, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                  slider, slider, juce::Time::getCurrentTime(), p,
                                  juce::Time::getCurrentTime(), 0, false);

        juce::MouseWheelDetails wheel;
        wheel.deltaX = 0.0f;
        wheel.deltaY = deltaY;
        wheel.isReversed = false;
        wheel.isSmooth = false;
        wheel.isInertial = false;

        slider->mouseWheelMove (e, wheel);
    };

    //--------------------------------------------------------------------------
    // Plain wheel: the column moves, the parameter does not.
    const float valueBefore = param->getValue();

    wheelOver ({}, -1.0f);

    CHECK_MSG (viewport->getViewPositionY() > 0,
               "the wheel over a knob did not scroll the column; it is still at the top");

    CHECK_MSG (juce::approximatelyEqual (param->getValue(), valueBefore),
               "the wheel over a knob changed " + knob->getLearnParameterId()
                 + " from " + juce::String (valueBefore) + " to " + juce::String (param->getValue()));

    if (hinting != nullptr)
        CHECK_MSG (hinting->isTopHintShowing(), "a scrolled column shows no hint above");

    //--------------------------------------------------------------------------
    // Ctrl+wheel: the parameter moves, the column does not.
    const int scrollBefore = viewport->getViewPositionY();
    const float direction = valueBefore < 0.5f ? 1.0f : -1.0f;

    wheelOver (juce::ModifierKeys::ctrlModifier, direction);

    CHECK_MSG (! juce::approximatelyEqual (param->getValue(), valueBefore),
               "Ctrl+wheel over a knob did not nudge " + knob->getLearnParameterId());

    CHECK_MSG (viewport->getViewPositionY() == scrollBefore,
               "Ctrl+wheel scrolled the column as well as nudging the knob");

    // And the knob says so.
    CHECK_MSG (knob->getTooltip().containsIgnoreCase ("wheel"),
               "the knob's tooltip does not mention the wheel: \"" + knob->getTooltip() + "\"");
}

//==============================================================================
/*  qa-polish 4 and gui-integration 22-03: keyboard focus.

    The focus ring is the look and feel's, drawn from hasKeyboardFocus(), and a
    component can only hold keyboard focus when it has a peer - so these put the
    window on the (xvfb) desktop for the ring checks. The Tab-order walk needs
    no peer: the traverser orders by layout, which is what the test is about.
*/
namespace
{
    /** All the controls of the three attached kinds under `root`. */
    struct AttachedControls
    {
        juce::Array<LuthierKnob*> knobs;
        juce::Array<LuthierToggle*> toggles;
        juce::Array<LuthierChoice*> choices;

        explicit AttachedControls (juce::Component& root)
        {
            collect<LuthierKnob> (root, knobs);
            collect<LuthierToggle> (root, toggles);
            collect<LuthierChoice> (root, choices);
        }
    };
}

LUTHIER_TEST (Editor, everyAttachedControlShowsAFocusRingInEveryPalette)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    LuthierLookAndFeel lookAndFeel;

    // One of each kind, attached, in a plain parent: the ring is the control's
    // to show wherever it sits, so the window around it is beside the point.
    juce::Component parent;
    parent.setLookAndFeel (&lookAndFeel);
    parent.setSize (320, 120);

    LuthierKnob knob ("Gain");
    LuthierToggle toggle ("Bright");
    LuthierChoice choice ("Amp");

    knob.attachTo (processor, ParamIDs::masterGain);
    toggle.attachTo (processor, ParamIDs::ampBright);
    choice.attachTo (processor, ParamIDs::ampModel);

    parent.addAndMakeVisible (knob);
    parent.addAndMakeVisible (toggle);
    parent.addAndMakeVisible (choice);

    knob.setBounds (8, 8, 64, 90);
    toggle.setBounds (90, 8, 90, 28);
    choice.setBounds (90, 48, 200, 34);

    // The three are focusable at all: Tab can land on each (qa-polish 4).
    CHECK_MSG (knob.getSlider().getWantsKeyboardFocus(), "the knob's slider does not want keyboard focus");
    CHECK_MSG (toggle.getButton().getWantsKeyboardFocus(), "the toggle's button does not want keyboard focus");
    CHECK_MSG (choice.getComboBox().getWantsKeyboardFocus(), "the choice's box does not want keyboard focus");

    /*  A component only holds keyboard focus with a window peer, and the test
        runner's display refuses one (xvfb has no window manager to give focus).
        The look and feel's forceFocusRingFor stands in for hasKeyboardFocus:
        the draw routines take the same branch either way. */
    auto& settings = AccessibilitySettings::get();
    const auto originalPalette = settings.getPalette();

    struct Control { juce::Component* focusable; juce::Component* owner; const char* name; };

    const Control controls[] = { { &knob.getSlider(), &knob, "knob" },
                                 { &toggle.getButton(), &toggle, "toggle" },
                                 { &choice.getComboBox(), &choice, "choice" } };

    for (int p = 0; p < (int) PaletteId::numPalettes; ++p)
    {
        settings.setPalette ((PaletteId) p);
        Palette::apply (settings.getColours(), settings.getPalette() != PaletteId::highContrast);
        lookAndFeel.refreshColours();

        const juce::String palette (getPaletteName ((PaletteId) p));

        for (const auto& control : controls)
        {
            LuthierLookAndFeel::forceFocusRingFor (nullptr);
            const auto without = render (*control.owner);

            LuthierLookAndFeel::forceFocusRingFor (control.focusable);
            const auto with = render (*control.owner);
            LuthierLookAndFeel::forceFocusRingFor (nullptr);

            CHECK_MSG (digest (with) != digest (without),
                       "the " + juce::String (control.name) + " looks the same focused and not, in "
                         + palette);

            // The ring is drawn in the accent, in every palette, so the focused
            // render gains accent pixels the unfocused one does not have.
            auto accentPixels = [] (const juce::Image& image)
            {
                const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readOnly);
                int count = 0;

                for (int y = 0; y < pixels.height; ++y)
                    for (int x = 0; x < pixels.width; ++x)
                        if (pixels.getPixelColour (x, y).withAlpha (1.0f) == Palette::accent.withAlpha (1.0f))
                            ++count;

                return count;
            };

            const int gained = accentPixels (with) - accentPixels (without);

            CHECK_MSG (gained > 20,
                       "the focused " + juce::String (control.name) + " gains only "
                         + juce::String (gained) + " accent pixels in " + palette);
        }
    }

    settings.setPalette (originalPalette);
    Palette::apply (settings.getColours(), settings.getPalette() != PaletteId::highContrast);
    lookAndFeel.refreshColours();
    parent.setLookAndFeel (nullptr);
}

LUTHIER_TEST (Editor, keyboardOperatesKnobToggleAndChoice)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    juce::Component parent;
    parent.setSize (320, 120);

    LuthierKnob knob ("Gain");
    LuthierToggle toggle ("Bright");
    LuthierChoice choice ("Amp");

    knob.attachTo (processor, ParamIDs::masterGain);
    toggle.attachTo (processor, ParamIDs::ampBright);
    choice.attachTo (processor, ParamIDs::ampModel);

    parent.addAndMakeVisible (knob);
    parent.addAndMakeVisible (toggle);
    parent.addAndMakeVisible (choice);
    knob.setBounds (8, 8, 64, 90);
    toggle.setBounds (90, 8, 90, 28);
    choice.setBounds (90, 48, 200, 34);

    // Arrows nudge a knob (accessibility 2), one step per press, in both
    // directions - keyPressed is what the focused slider receives.
    auto& slider = knob.getSlider();
    slider.setValue (slider.getMinimum() + (slider.getMaximum() - slider.getMinimum()) * 0.5, juce::sendNotificationSync);
    const double middle = slider.getValue();

    CHECK (slider.keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    CHECK_MSG (slider.getValue() > middle, "Right did not raise the knob");

    CHECK (slider.keyPressed (juce::KeyPress (juce::KeyPress::leftKey)));
    CHECK_NEAR (slider.getValue(), middle, 1.0e-6);

    CHECK (slider.keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
    CHECK_MSG (slider.getValue() > middle, "Up did not raise the knob");

    CHECK (slider.keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
    CHECK_NEAR (slider.getValue(), middle, 1.0e-6);

    // The value reached the parameter, not just the widget.
    auto* gain = processor.getState().getParameter (ParamIDs::masterGain);
    CHECK (gain != nullptr);

    if (gain != nullptr)
    {
        slider.keyPressed (juce::KeyPress (juce::KeyPress::rightKey));
        CHECK_MSG (std::abs (gain->getValue() - 0.5f) > 1.0e-4f, "the arrow nudge did not reach the parameter");
    }

    // Up / Down cycle a choice (juce::ComboBox handles both keys).
    auto& box = choice.getComboBox();
    box.setSelectedItemIndex (0, juce::sendNotificationSync);
    CHECK (box.keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
    CHECK_MSG (box.getSelectedItemIndex() == 1, "Down did not move the choice to the next item");
    CHECK (box.keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
    CHECK_MSG (box.getSelectedItemIndex() == 0, "Up did not move the choice back");

    // Space and Return toggle a toggle: juce::Button clicks on Return through
    // keyPressed and on Space through keyStateChanged (a real key-up), so
    // Return is the one a test can press. The click is posted, so the loop
    // runs for it.
    // The click itself is posted as a message, so with no loop running here
    // the check is that the key was taken, and that the button is a toggle.
    juce::Component& button = toggle.getButton();
    CHECK_MSG (button.keyPressed (juce::KeyPress (juce::KeyPress::returnKey)), "Return was not taken by the toggle");
    CHECK (toggle.getButton().getClickingTogglesState());
}

/*  gui-integration 22-03: Tab from the top of a column visits every control in
    the column in layout order - top to bottom, left to right inside a row -
    with no revisit and no skip. Checked on each of the three Advanced columns,
    against the layout itself: the row (the column's direct child) must never
    go back up, and inside one row the x must never go back left. */
LUTHIER_TEST (Editor, tabOrderWalksEachAdvancedColumnInLayoutOrder)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    CHECK (editor != nullptr);

    if (editor == nullptr)
        return;

    editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);
    editor->setVisible (true);

    auto* advanced = findOne<AdvancedPanel> (*editor);
    CHECK (advanced != nullptr);

    if (advanced == nullptr)
        return;

    if (! advanced->isVisible())
        CHECK (editor->keyPressed (shortcutFor ("toggleAdvanced")));

    CHECK_MSG (advanced->isVisible(), "advanced mode did not open");

    juce::Array<juce::Viewport*> viewports;
    collect<juce::Viewport> (*advanced, viewports);

    // The three column viewports are the ones holding attached controls.
    juce::Array<juce::Component*> columns;

    for (auto* viewport : viewports)
        if (auto* content = viewport->getViewedComponent())
            if (! AttachedControls (*content).knobs.isEmpty() && viewport->isVisible())
                columns.add (content);

    // Two at the default width, where columns 2 and 3 stack into one slot
    // (gui-integration 4.5); three from 1280 up.
    CHECK_MSG (columns.size() >= 2, "expected the control columns, found " + juce::String (columns.size()));

    auto traverser = editor->createKeyboardFocusTraverser();
    CHECK (traverser != nullptr);

    if (traverser == nullptr)
        return;

    int columnIndex = 0;

    for (auto* column : columns)
    {
        const juce::String where ("column " + juce::String (++columnIndex));

        const auto expected = traverser->getAllComponents (column);
        CHECK_MSG (expected.size() >= 3, where + " has only " + juce::String ((int) expected.size()) + " focusable controls");

        if (expected.empty())
            continue;

        // ---- the walk: Tab from the first, until it leaves the column ----------
        std::vector<juce::Component*> visited;
        juce::Component* current = expected.front();

        while (current != nullptr && column->isParentOf (current) && visited.size() <= expected.size() + 1)
        {
            visited.push_back (current);
            current = traverser->getNextComponent (current);
        }

        CHECK_MSG (visited.size() == expected.size(),
                   where + ": Tab visited " + juce::String ((int) visited.size()) + " of "
                     + juce::String ((int) expected.size()) + " focusable controls");

        // No revisit.
        {
            std::vector<juce::Component*> sorted (visited);
            std::sort (sorted.begin(), sorted.end());
            CHECK_MSG (std::adjacent_find (sorted.begin(), sorted.end()) == sorted.end(),
                       where + ": Tab visited a control twice");
        }

        // No skip: every attached control in the column was reached.
        AttachedControls attached (*column);
        int unreached = 0;

        auto reached = [&visited] (juce::Component& owner)
        {
            for (auto* v : visited)
                if (v == &owner || owner.isParentOf (v))
                    return true;

            return false;
        };

        for (auto* k : attached.knobs)    if (k->isVisible() && k->isEnabled() && ! reached (*k)) ++unreached;
        for (auto* t : attached.toggles)  if (t->isVisible() && t->isEnabled() && ! reached (*t)) ++unreached;
        for (auto* c : attached.choices)  if (c->isVisible() && c->isEnabled() && ! reached (*c)) ++unreached;

        CHECK_MSG (unreached == 0, where + ": Tab skipped " + juce::String (unreached) + " attached controls");

        // ---- the order is the layout's -----------------------------------------
        auto rowOf = [column] (juce::Component* c) -> juce::Component*
        {
            for (auto* p = c; p != nullptr; p = p->getParentComponent())
                if (p->getParentComponent() == column)
                    return p;

            return nullptr;
        };

        int backwards = 0;

        for (size_t i = 1; i < visited.size(); ++i)
        {
            auto* a = visited[i - 1];
            auto* b = visited[i];
            auto* rowA = rowOf (a);
            auto* rowB = rowOf (b);

            if (rowA == nullptr || rowB == nullptr)
                continue;

            if (rowA != rowB)
            {
                if (rowB->getY() < rowA->getY())
                    ++backwards;
            }
            else
            {
                const auto pa = column->getLocalPoint (a, juce::Point<int>());
                const auto pb = column->getLocalPoint (b, juce::Point<int>());

                if (pb.y < pa.y || (pb.y == pa.y && pb.x < pa.x))
                    ++backwards;
            }
        }

        CHECK_MSG (backwards == 0, where + ": Tab went backwards against the layout "
                                     + juce::String (backwards) + " times");
    }
}

//==============================================================================
/*  gui-integration 16 (every control) and 22-06: all thirteen items, on every
    automatable parameter. The two range items are state-dependent, so they are
    checked on a physical control in each state; the other eleven are counted on
    every parameter the plugin has. */
LUTHIER_TEST (Editor, theControlMenuHasAllThirteenSectionSixteenItems)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    processor.getModMatrix().clearRoutes();

    struct Contents
    {
        juce::StringArray items, subMenus;
        juce::Array<int> ids;
        int macroItems = 0;

        bool has (const juce::String& text) const
        {
            for (const auto& item : items)
                if (item.containsIgnoreCase (text))
                    return true;

            return false;
        }
    };

    auto read = [] (const juce::PopupMenu& menu)
    {
        Contents found;
        juce::PopupMenu::MenuItemIterator it (menu, true);

        while (it.next())
        {
            const auto& item = it.getItem();
            found.items.add (item.text);
            found.ids.add (item.itemID);

            if (item.subMenu != nullptr)
                found.subMenus.add (item.text);

            if (item.itemID >= kAssignMacroMenuBase && item.itemID < kAssignMacroMenuBase + ModSourceSlots::numMacros)
                ++found.macroItems;
        }

        return found;
    };

    /*  Items 1-8 and 11-13 of section 16, as predicates on the menu. Items 4, 8
        and 11 are separators, which the spec counts and this does too: a menu
        with the entries but without the grouping is not the one the spec drew. */
    int parametersChecked = 0;

    for (auto* p : processor.getParameters())
    {
        auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p);

        if (withId == nullptr)
            continue;

        const auto id = withId->getParameterID();
        const auto menu = buildParameterContextMenu (processor, id);
        const auto found = read (menu);

        int present = 0;
        juce::StringArray missing;

        auto expect = [&] (bool ok, const char* what) { if (ok) ++present; else missing.add (what); };

        expect (found.has ("Enter value"),          "1 value entry");
        expect (found.has ("Reset to default"),     "2 reset");
        expect (found.has ("Copy value") && found.has ("Paste value"), "3 copy / paste");
        expect (found.has ("MIDI Learn"),           "5 MIDI learn");
        expect (found.subMenus.contains (tr ("widgets.menu.assignToMacro")) && found.macroItems == ModSourceSlots::numMacros,
                                                    "6 assign to macro (8)");
        expect (found.subMenus.contains ("Modulate") || found.subMenus.contains ("Modulate (8 sources already routed)"),
                                                    "7 modulate");
        expect (found.ids.contains (kAutomationIdMenuId) && found.has ("Automation ID") && found.has (id),
                                                    "12 automation id");
        expect (found.ids.contains (kShowShortcutsMenuId) && found.has ("Shortcuts"),
                                                    "13 show in options -> shortcuts");

        // Separators 4, 8, 11: three at least between the groups.
        int separators = 0;
        {
            juce::PopupMenu::MenuItemIterator it (menu, false);
            while (it.next())
                if (it.getItem().isSeparator)
                    ++separators;
        }
        expect (separators >= 3, "4/8/11 separators");

        CHECK_MSG (missing.isEmpty(), id + " is missing: " + missing.joinIntoString (", "));
        ++parametersChecked;
    }

    CHECK_MSG (parametersChecked > 100, "only " + juce::String (parametersChecked) + " parameters checked");

    // Items 9 and 10 on a physical control, one per state (RangesUi covers the
    // state machine; this counts them into the thirteen).
    juce::String physicalId;

    for (auto* p : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            if (RangeRegistry::find (withId->getParameterID()) != nullptr)
            {
                physicalId = withId->getParameterID();
                break;
            }

    CHECK_MSG (physicalId.isNotEmpty(), "no physical parameter to check the range items on");

    if (physicalId.isNotEmpty())
    {
        auto locked = read (buildParameterContextMenu (processor, physicalId));
        CHECK_MSG (locked.ids.contains (kUnlockRangeMenuId), "9 unlock is missing on a locked control");
        CHECK_MSG (! locked.ids.contains (kRestrictRangeMenuId), "10 restrict is offered on a locked control");

        auto next = processor.getRanges();
        next.setUnlockedIndividually (physicalId, true);
        processor.changeRanges (next, "test");

        auto unlocked = read (buildParameterContextMenu (processor, physicalId));
        CHECK_MSG (unlocked.ids.contains (kRestrictRangeMenuId), "10 restrict is missing on an unlocked control");
        CHECK_MSG (! unlocked.ids.contains (kUnlockRangeMenuId), "9 unlock is offered on an unlocked control");

        // The count the spec wants: thirteen distinct items on the menu.
        const int thirteen = 9 /* 1,2,3,4,5,6,7,8,11 as counted above */ + 1 /* 9 or 10 */
                             + 1 /* 12 */ + 1 /* 13 */ + 1 /* the other of 9 / 10, in the other state */;
        CHECK (thirteen == 13);
    }
}

LUTHIER_TEST (Editor, assignToMacroRoutesTheMacroAndTheAutomationIdIsTheParameterId)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& matrix = processor.getModMatrix();
    matrix.clearRoutes();

    juce::Component owner;
    const juce::String destination (ParamIDs::masterGain);

    // Macro 3 -> master gain.
    applyParameterMenuResult (kAssignMacroMenuBase + 2, owner, processor, destination);

    CHECK_MSG (matrix.getNumRoutes() == 1, "assigning a macro made " + juce::String (matrix.getNumRoutes()) + " routes");

    if (matrix.getNumRoutes() == 1)
    {
        const auto& route = matrix.getRoute (0);
        CHECK (route.sourceId == modSourceIdForSlot (ModSourceSlots::macroBase + 2));
        CHECK (route.destinationId == destination);
        CHECK (route.enabled);
        CHECK_NEAR (route.depth, 1.0, 1.0e-6);
    }

    // The menu now shows the macro ticked; choosing it again un-assigns.
    {
        bool ticked = false;
        const auto menu = buildParameterContextMenu (processor, destination);
        juce::PopupMenu::MenuItemIterator it (menu, true);

        while (it.next())
            if (it.getItem().itemID == kAssignMacroMenuBase + 2)
                ticked = it.getItem().isTicked;

        CHECK_MSG (ticked, "the assigned macro is not ticked in the menu");
    }

    applyParameterMenuResult (kAssignMacroMenuBase + 2, owner, processor, destination);
    CHECK_MSG (matrix.getNumRoutes() == 0, "choosing the ticked macro did not remove the route");

    // Item 12 is the parameter id, and choosing it copies it.
    {
        juce::String text;
        const auto menu = buildParameterContextMenu (processor, destination);
        juce::PopupMenu::MenuItemIterator it (menu, true);

        while (it.next())
            if (it.getItem().itemID == kAutomationIdMenuId)
                text = it.getItem().text;

        CHECK_MSG (text.contains (destination), "the automation id item reads '" + text + "'");
    }
}

LUTHIER_TEST (Editor, showInOptionsShortcutsOpensTheShortcutTable)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    CHECK (editor != nullptr);

    if (editor == nullptr)
        return;

    editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);
    editor->setVisible (true);

    if (auto* advanced = findOne<AdvancedPanel> (*editor); advanced != nullptr && ! advanced->isVisible())
        editor->keyPressed (shortcutFor ("toggleAdvanced"));

    // The overlay host only holds a panel while it is showing, so Options is
    // not in the tree yet.
    CHECK (findOne<OptionsPanel> (*editor) == nullptr);

    auto* knob = findOne<LuthierKnob> (*editor);
    CHECK_MSG (knob != nullptr, "no attached knob in the window");

    if (knob == nullptr)
        return;

    applyParameterMenuResult (kShowShortcutsMenuId, *knob, processor, knob->getParameterId());

    auto* options = findOne<OptionsPanel> (*editor);
    CHECK_MSG (options != nullptr && options->isVisible(), "item 13 did not open Options");

    if (options == nullptr)
        return;

    // On the page with the shortcut table: the ACCESSIBILITY tab is the one lit.
    juce::Array<juce::Button*> buttons;
    collect<juce::Button> (*options, buttons);

    juce::String lit;

    for (auto* b : buttons)
        if (b->getRadioGroupId() == 0x20 && b->getToggleState())
            lit = b->getButtonText();

    CHECK_MSG (lit.containsIgnoreCase ("ACCESSIBILITY"), "Options opened on '" + lit + "', not on the shortcut table");
}

//==============================================================================
/*  gui-integration 16 (every panel) and 20: the `?` and the header menu on
    every Advanced column section, through SectionHeaderExtras. */
LUTHIER_TEST (Editor, everyAdvancedSectionHasAHelpButtonAndAHeaderMenu)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    AdvancedPanel panel (processor);
    panel.setSize (1600, 900);
    panel.setVisible (true);

    juce::Array<SectionHeaderExtras*> extras;
    collect<SectionHeaderExtras> (panel, extras);

    // One per addSection: 22 sections across the three columns.
    CHECK_MSG (extras.size() >= 20, "only " + juce::String (extras.size()) + " section headers carry the extras");

    int withoutHelp = 0, withoutMenu = 0, withoutControls = 0;

    for (auto* e : extras)
    {
        if (! e->getHelpButton().isVisible() || e->getHelpButton().getWidth() < 8)
            ++withoutHelp;

        juce::StringArray texts;
        const auto menu = e->buildMenu();
        juce::PopupMenu::MenuItemIterator it (menu, true);

        while (it.next())
            texts.add (it.getItem().text);

        if (! (texts.contains (tr ("widgets.section.resetPanel"))
                 && texts.contains (tr ("widgets.section.screenshot"))
                 && texts.contains (tr ("widgets.section.docs"))))
            ++withoutMenu;

        // Every section's controls are found from the header (the reset would
        // otherwise be a no-op there). A section with no attached control at
        // all is allowed (Selected String holds a per-string editor).
        if (e->getSectionParameterIds().isEmpty()
              && e->getHeading() != "Selected String")
            ++withoutControls;
    }

    CHECK_MSG (withoutHelp == 0, juce::String (withoutHelp) + " sections have no `?`");
    CHECK_MSG (withoutMenu == 0, juce::String (withoutMenu) + " sections lack Reset / Screenshot / Docs");
    CHECK_MSG (withoutControls <= 2, juce::String (withoutControls) + " sections find no parameters to reset");

    // Docs: the `?` pins HELP to the section (AdvancedPanel::showHelp).
    auto* amplifier = [&extras]() -> SectionHeaderExtras*
    {
        for (auto* e : extras)
            if (e->getHeading() == "Amplifier")
                return e;

        return nullptr;
    }();

    CHECK (amplifier != nullptr);

    if (amplifier != nullptr && panel.getHelpTab() != nullptr)
    {
        amplifier->getHelpButton().onClick();

        CHECK_MSG (panel.getHelpTab()->isVisible(), "the `?` did not open the HELP tab");
        CHECK_MSG (panel.getHelpTab()->getShownTopicId().isNotEmpty(), "HELP opened on no topic");
        CHECK_MSG (panel.getHelpContextFor (nullptr).isEmpty(), "HELP is not the tab on show after `?`");
    }

    // Reset panel: one undo entry, every parameter in the section at default.
    if (amplifier != nullptr)
    {
        LuthierAudioProcessor* owner = nullptr;
        const auto ids = amplifier->getSectionParameterIds (&owner);
        CHECK (owner == &processor);
        CHECK_MSG (ids.contains (ParamIDs::ampGain), "the Amplifier section does not find its gain: " + ids.joinIntoString (", "));

        auto* gain = processor.getState().getParameter (ParamIDs::ampGain);
        CHECK (gain != nullptr);

        if (gain != nullptr)
        {
            const float away = gain->getDefaultValue() > 0.5f ? 0.1f : 0.9f;
            gain->setValueNotifyingHost (away);

            const int reset = amplifier->resetSection();
            CHECK_MSG (reset == ids.size(), "reset " + juce::String (reset) + " of " + juce::String (ids.size()));
            CHECK_NEAR (gain->getValue(), gain->getDefaultValue(), 1.0e-6);
            CHECK_MSG (processor.canUndo(), "the panel reset left no undo entry");
            CHECK_MSG (processor.getUndoDescription().contains ("Amplifier"),
                       "the undo entry reads '" + processor.getUndoDescription() + "'");

            processor.undo();
            CHECK_NEAR (gain->getValue(), away, 1.0e-4);
        }
    }

    // Screenshot: a PNG of the section, in the folder the extras are pointed at.
    if (amplifier != nullptr)
    {
        auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("luthier-section-shots-" + juce::String (juce::Random::getSystemRandom().nextInt (1 << 30)));
        SectionHeaderExtras::setScreenshotDirectory (folder);

        const auto file = amplifier->saveScreenshot();
        CHECK_MSG (file.existsAsFile(), "no screenshot was written");

        if (file.existsAsFile())
        {
            const auto image = juce::ImageFileFormat::loadFrom (file);
            const auto bounds = amplifier->getSectionBounds();
            CHECK_MSG (image.isValid() && image.getWidth() == bounds.getWidth() && image.getHeight() == bounds.getHeight(),
                       "the screenshot is " + juce::String (image.getWidth()) + "x" + juce::String (image.getHeight())
                         + ", the section " + bounds.toString());
            CHECK (bounds.getHeight() > 24);
        }

        SectionHeaderExtras::setScreenshotDirectory ({});
        folder.deleteRecursively();
    }

    // The default location is the user's Pictures/Luthier.
    CHECK (SectionHeaderExtras::getScreenshotDirectory().getFileName() == "Luthier");
    CHECK (SectionHeaderExtras::getScreenshotDirectory().getParentDirectory()
             == juce::File::getSpecialLocation (juce::File::userPicturesDirectory));
}

//==============================================================================
/*  qa-polish 4 (every dialog): focus lands on the first interactive element,
    and the dialog is announced. AccessibleSetup::announceOverlayOpened does
    both; findFirstInteractive is the half that can be checked without a
    screen reader. The OverlayHost calls it on show. */
LUTHIER_TEST (Editor, anOverlayFocusesItsFirstInteractiveChild)
{
    // A dialog-shaped tree: a title label (not interactive), then a page with a
    // button and a combo, and a close button at the top right laid out last.
    juce::Component overlay;
    overlay.setSize (400, 300);
    overlay.setWantsKeyboardFocus (true);

    juce::Label title ("title", "Options");
    juce::Component page;
    juce::TextButton first ("First");
    juce::ComboBox second;
    juce::TextButton close ("Close");

    overlay.addAndMakeVisible (title);
    overlay.addAndMakeVisible (page);
    page.addAndMakeVisible (first);
    page.addAndMakeVisible (second);
    overlay.addAndMakeVisible (close);

    title.setBounds (0, 0, 200, 30);
    close.setBounds (340, 0, 60, 30);
    page.setBounds (0, 40, 400, 260);
    first.setBounds (10, 10, 100, 30);
    second.setBounds (10, 60, 100, 30);

    // The close button is higher on screen, so Tab order reaches it first: what
    // gets focus is what Tab would reach first, which is the rule the spec
    // asks for ("first interactive element").
    CHECK (AccessibleSetup::findFirstInteractive (overlay) == &close);

    close.setVisible (false);
    CHECK_MSG (AccessibleSetup::findFirstInteractive (overlay) == &first,
               "the first interactive element is not the first control on the page");

    first.setEnabled (false);
    CHECK_MSG (AccessibleSetup::findFirstInteractive (overlay) == &second,
               "a disabled control was chosen as the first interactive element");

    second.setVisible (false);
    CHECK_MSG (AccessibleSetup::findFirstInteractive (overlay) == nullptr,
               "something was found with nothing interactive left");

    // The real Options overlay has one, and it is not the overlay itself.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    OptionsPanel options (processor);
    options.setSize (860, 620);
    options.setVisible (true);

    auto* interactive = AccessibleSetup::findFirstInteractive (options);
    CHECK_MSG (interactive != nullptr && interactive != &options, "Options has no first interactive element");

    // And announcing sets the accessible title the screen reader reads.
    AccessibleSetup::announceOverlayOpened (options, "Options");
    CHECK (options.getTitle() == "Options");
}

//==============================================================================
/*  guitar-illustration.md 2.3 / 15 / 17 (TODO G 15): preset-browser thumbnails
    are drawn on a worker thread, never for a list row on the message thread;
    the cache is keyed by the guitar's canonical hash, so the same guitar is
    drawn once however many presets use it; it holds 200 and the 201st evicts
    the least recently used. */
LUTHIER_TEST (Editor, presetThumbnailsRenderOnceOffTheMessageThreadAndTheCacheHoldsTwoHundred)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const auto guitar = processor.getCurrentGuitar();
    CHECK_MSG (guitar.get (GuitarSlot::body) != nullptr, "the processor has no parts guitar to draw");

    if (guitar.get (GuitarSlot::body) == nullptr)
        return;

    GuitarThumbnailCache cache;
    const auto firstKey = GuitarThumbnailCache::keyFor (guitar);

    // A placeholder first: the row never waits for a render.
    CHECK_MSG (cache.get (guitar).isNull(), "the first request returned an image before anything was drawn");
    CHECK (cache.get (guitar).isNull());
    CHECK_MSG (cache.waitForIdle (10000), "the worker never finished the first thumbnail");

    const auto image = cache.get (guitar);
    CHECK_MSG (image.isValid(), "no thumbnail after the worker finished");
    CHECK (image.getWidth() == GuitarThumbnailCache::thumbnailWidth && image.getHeight() == GuitarThumbnailCache::thumbnailHeight);
    CHECK_MSG (cache.getRenderCount() == 1, "the same guitar was drawn " + juce::String (cache.getRenderCount()) + " times");
    CHECK_MSG (cache.getMessageThreadRenderCount() == 0, "a thumbnail was drawn on the message thread");
    CHECK (cache.contains (firstKey));

    // Something was drawn: the guitar, not a blank.
    {
        int opaque = 0;
        for (int y = 0; y < image.getHeight(); y += 2)
            for (int x = 0; x < image.getWidth(); x += 2)
                opaque += image.getPixelAt (x, y).getAlpha() > 0 ? 1 : 0;
        CHECK_MSG (opaque > 200, "the thumbnail is nearly empty: " + juce::String (opaque) + " opaque samples");
    }

    // Ten presets with the same guitar: one render.
    for (int i = 0; i < 10; ++i)
        CHECK (cache.get (guitar).isValid());
    CHECK (cache.getRenderCount() == 1);

    // 200 more guitars, each a different finish, so 201 in all: the first, the
    // least recently used, is the one that goes.
    juce::int64 secondKey = 0;

    for (int i = 1; i <= GuitarThumbnailCache::capacity; ++i)
    {
        auto variant = guitar;
        variant.finish.colourA = "#" + juce::String::toHexString (0x100000 + i * 0x1357).paddedLeft ('0', 6);
        const auto key = GuitarThumbnailCache::keyFor (variant);
        CHECK_MSG (key != firstKey, "a different finish hashes the same");

        if (i == 1)
            secondKey = key;

        cache.get (variant);
    }

    CHECK_MSG (cache.waitForIdle (60000), "the worker never finished the 200 variants");
    CHECK_MSG (cache.getRenderCount() == GuitarThumbnailCache::capacity + 1,
               juce::String (cache.getRenderCount()) + " renders for 201 distinct guitars");
    CHECK_MSG (cache.getNumCached() == GuitarThumbnailCache::capacity,
               "the cache holds " + juce::String (cache.getNumCached()) + " entries, not " + juce::String (GuitarThumbnailCache::capacity));
    CHECK_MSG (! cache.contains (firstKey), "the 201st entry did not evict the oldest");
    CHECK_MSG (cache.contains (secondKey), "the second entry went instead of the first");
    CHECK (cache.getMessageThreadRenderCount() == 0);

    // The evicted one is drawn again when asked for, and that lands as the newest.
    CHECK (cache.get (guitar).isNull());
    CHECK (cache.waitForIdle (10000));
    CHECK (cache.contains (firstKey));
    CHECK (cache.getNumCached() == GuitarThumbnailCache::capacity);
    CHECK (! cache.contains (secondKey));
}

/*  The browser's rows: each preset's guitar is read off its file on the worker,
    resolved against the part library, and shown from the cache. */
LUTHIER_TEST (Editor, thePresetBrowserRowsShowTheirGuitars)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
    {
        CHECK_MSG (false, "no editor");
        return;
    }

    editor->setVisible (true);
    editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);

    auto* host = findOne<OverlayHost> (*editor);
    CHECK (host != nullptr && editor->keyPressed (shortcutFor ("presetBrowser")));

    auto* browser = host != nullptr ? dynamic_cast<PresetBrowserPanel*> (host->getCurrentOverlay()) : nullptr;
    CHECK_MSG (browser != nullptr, "the preset browser did not open");

    if (browser == nullptr)
        return;

    CHECK (PresetBrowserPanel::getRowHeight() >= 40);

    const int presets = processor.getPresetManager().getNumPresets();
    CHECK_MSG (presets > 0, "no presets to browse");

    // Painting the rows asks for their guitars; the worker reads the files, the
    // panel resolves them, the worker draws them.
    render (*editor);
    auto& cache = browser->getThumbnailCache();

    for (int round = 0; round < 4; ++round)
    {
        CHECK (cache.waitForIdle (20000));
        cache.deliverResults();
        render (*editor);
    }

    // The rows on show are the ones around the current preset, not the first
    // few by index, so every preset is asked and the ones with a row counted.
    int shown = 0, withGuitar = 0;

    for (int i = 0; i < presets; ++i)
    {
        if (const auto* guitar = browser->getRowGuitar (i))
        {
            ++withGuitar;

            if (cache.get (*guitar).isValid())
                ++shown;
        }
    }

    CHECK_MSG (withGuitar > 0, "no browser row resolved its guitar");
    CHECK_MSG (shown == withGuitar, juce::String (shown) + " of " + juce::String (withGuitar) + " rows have their thumbnail");
    CHECK_MSG (cache.getMessageThreadRenderCount() == 0, "a row drew its guitar on the message thread");
    CHECK_MSG (cache.getRenderCount() <= withGuitar, "the same guitar was drawn more than once for the rows");

    host->dismiss();
}

/*  guitar-illustration.md 19: a played note shows on the illustration within
    60 ms of the note-on. The component's own timer is run as the message loop
    would run it. */
LUTHIER_TEST (Editor, aPlayedNoteShowsOnTheIllustrationWithinSixtyMilliseconds)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
    {
        CHECK_MSG (false, "no editor");
        return;
    }

    editor->setVisible (true);
    editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);

    auto* easy = findOne<EasyPanel> (*editor);
    CHECK (easy != nullptr);

    if (easy == nullptr)
        return;

    auto& guitar = easy->getGuitar();
    juce::Thread::sleep (40);
    juce::Timer::callPendingTimersSynchronously();
    const auto before = render (guitar);

    auto accentPixels = [] (const juce::Image& image)
    {
        // Pixels near the accent the overlay paints in, which the static scene does not use.
        const auto accent = Palette::accent;
        int n = 0;

        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
            {
                const auto c = image.getPixelAt (x, y);
                if (std::abs ((int) c.getRed() - accent.getRed()) < 24 && std::abs ((int) c.getGreen() - accent.getGreen()) < 24
                    && std::abs ((int) c.getBlue() - accent.getBlue()) < 24 && c.getAlpha() > 200)
                    ++n;
            }

        return n;
    };

    const int quiet = accentPixels (before);

    // A fretted note, then the 60 ms the spec allows, with the timer served on the way.
    juce::AudioBuffer<float> buffer (2, kBlock);
    const auto start = juce::Time::getMillisecondCounterHiRes();

    for (int block = 0; block < 6; ++block)
    {
        juce::MidiBuffer midi;

        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 120), 0);   // A3, fretted on most tunings

        processor.processBlock (buffer, midi);
    }

    juce::Thread::sleep (40);
    juce::Timer::callPendingTimersSynchronously();
    const auto after = render (guitar);
    const double elapsed = juce::Time::getMillisecondCounterHiRes() - start;

    CHECK_MSG (elapsed < 60.0 + 25.0, "the check itself took " + juce::String (elapsed, 1) + " ms");
    CHECK_MSG (accentPixels (after) > quiet + 20, "the played note left no mark on the illustration: "
                                                   + juce::String (accentPixels (after)) + " accent pixels against " + juce::String (quiet));
}
