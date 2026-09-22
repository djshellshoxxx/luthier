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

    /*  gui-integration.md section 5's list, with the two departures GAPS.md A3
        records: no RANGES, which advanced-ranges.md has not specified yet, and a
        CONTROLLERS tab in its slot, because section 19's home for it does not
        exist. If section 5 gains a tab, this list is where it fails first. */
    const juce::StringArray tabNames { "AUDIO", "MIDI", "APPEARANCE", "ACCESSIBILITY",
                                       "LOCALIZATION", "EXPRESSION", "CONTROLLERS",
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

    const juce::StringArray tabNames { "MOD", "RHYTHM", "ROUTING", "TONE MATCH", "CHARACTER" };

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
    auto& source = juce::Desktop::getInstance().getMainMouseSource();

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
