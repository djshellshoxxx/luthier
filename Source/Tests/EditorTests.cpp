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

    const juce::StringArray tabNames { "GENERAL", "CONTROLLERS", "EXPRESSION",
                                       "ACCESSIBILITY", "PRIVACY" };

    juce::Array<OptionsPage*> pages;
    collect<OptionsPage> (options, pages);

    CHECK_MSG (pages.size() == tabNames.size() - 1,
               "expected " + juce::String (tabNames.size() - 1) + " Options pages, found "
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

        if (tab == 0)
        {
            // General is the panel's own controls, so no page should be up at all.
            CHECK_MSG (showing.isEmpty(), "GENERAL left an extension page on screen");
            continue;
        }

        CHECK_MSG (showing.size() == 1,
                   name + " put " + juce::String (showing.size()) + " pages on screen");

        if (showing.size() != 1)
            continue;

        CHECK_MSG (showing.getFirst() == pages[tab - 1],
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
