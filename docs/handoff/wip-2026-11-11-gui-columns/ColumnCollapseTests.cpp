/*  Collapsible sections in the Advanced columns.

    Each titled section in columns 1 to 3 has a header that folds it away and
    back: a click, or Enter or Space on the focused header. Folded sections
    are saved with the rest of the editor's UI state in the host session; a
    session saved before sections could fold opens with every one expanded,
    and so does a fresh instance, which is what keeps every automatable
    parameter's control on screen by default (GuiReach).
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/CollapsibleSectionHeader.h"
#include "../UI/Search/LiveControls.h"
#include "../UI/Widgets.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    bool shownUnder (juce::Component& c, juce::Component& root)
    {
        for (auto* p = &c; p != nullptr; p = p->getParentComponent())
        {
            if (! p->isVisible())
                return false;

            if (p == &root)
                return true;
        }

        return false;
    }

    int knobsShownUnder (juce::Component& c, juce::Component& root)
    {
        int n = 0;

        if (dynamic_cast<LuthierKnob*> (&c) != nullptr && shownUnder (c, root))
            ++n;

        for (auto* child : c.getChildren())
            n += knobsShownUnder (*child, root);

        return n;
    }

    struct Rig
    {
        LuthierAudioProcessor processor;
        std::unique_ptr<AdvancedPanel> panel;

        Rig()
        {
            processor.prepareToPlay (kSr, kBlock);
            open();
        }

        void open()
        {
            panel = std::make_unique<AdvancedPanel> (processor);
            panel->setVisible (true);
            panel->setSize (1600, 900);
            panel->setWorkspaceTabNamed ("MOD");
        }

        int columnHeight (int column)
        {
            auto* viewport = dynamic_cast<juce::Viewport*> (panel->getColumnViewport (column));
            return viewport != nullptr && viewport->getViewedComponent() != nullptr
                     ? viewport->getViewedComponent()->getHeight() : 0;
        }

        int foldedCount() const
        {
            int n = 0;

            for (int c = 1; c <= 3; ++c)
                for (const auto& heading : panel->getColumnSections (c))
                    if (panel->isColumnSectionCollapsed (heading))
                        ++n;

            return n;
        }
    };

    juce::var stateOf (LuthierAudioProcessor& p)
    {
        juce::MemoryBlock block;
        p.getStateInformation (block);
        return juce::JSON::parse (LuthierAudioProcessor::stateBlobToText (block.getData(), (int) block.getSize()));
    }

    void restore (LuthierAudioProcessor& p, const juce::var& state)
    {
        const auto text = juce::JSON::toString (state, false);
        p.setStateInformation (text.toRawUTF8(), (int) text.getNumBytesAsUTF8());
    }
}

LUTHIER_TEST (ColumnCollapse, everySectionHasAFocusableHeaderThatFoldsIt)
{
    Rig rig;
    auto& panel = *rig.panel;

    int headers = 0;

    for (int c = 1; c <= 3; ++c)
        for (const auto& heading : panel.getColumnSections (c))
        {
            auto* header = panel.getColumnSectionHeader (heading);
            CHECK_MSG (header != nullptr, heading + " has no header");

            if (header == nullptr)
                continue;

            ++headers;
            CHECK_MSG (header->isExpanded(), heading + " does not start expanded");
            CHECK_MSG (header->getWantsKeyboardFocus(), heading + "'s header cannot take focus");
            CHECK_MSG (header->getTooltip().isNotEmpty(), heading + "'s header has no tooltip");
            CHECK_MSG (header->isVisible() && header->getWidth() > 0, heading + "'s header is not on screen");

            auto handler = header->createAccessibilityHandler();
            CHECK (handler != nullptr);

            if (handler != nullptr)
            {
                CHECK_MSG (handler->getTitle().containsIgnoreCase (heading), heading + "'s header is not named for it");
                CHECK (handler->getRole() == juce::AccessibilityRole::button);
                CHECK (handler->getCurrentState().isExpandable());
                CHECK (handler->getCurrentState().isExpanded());
            }
        }

    CHECK_MSG (headers > 10, "only " + juce::String (headers) + " section headers");
}

LUTHIER_TEST (ColumnCollapse, enterSpaceAndAPressFoldAndUnfold)
{
    Rig rig;
    auto& panel = *rig.panel;

    auto* header = panel.getColumnSectionHeader ("Body");
    auto* body = panel.getColumnSectionBody ("Body");
    CHECK (header != nullptr && body != nullptr);

    if (header == nullptr || body == nullptr)
        return;

    const int knobs = knobsShownUnder (*body, panel);
    const int height = rig.columnHeight (0);
    CHECK_MSG (knobs >= 5, "the Body section shows " + juce::String (knobs) + " knobs");

    // Enter folds it: its controls leave the screen and the column shortens.
    CHECK (header->keyPressed (juce::KeyPress (juce::KeyPress::returnKey)));
    CHECK (! header->isExpanded());
    CHECK (panel.isColumnSectionCollapsed ("Body"));
    CHECK (knobsShownUnder (*body, panel) == 0);
    CHECK_MSG (rig.columnHeight (0) < height, "folding Body did not shorten column 1");
    CHECK (header->isVisible());   // the header stays, so it can be unfolded

    if (auto handler = header->createAccessibilityHandler())
        CHECK (handler->getCurrentState().isCollapsed());

    // Space unfolds it, back to exactly where it was.
    CHECK (header->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    CHECK (header->isExpanded());
    CHECK (knobsShownUnder (*body, panel) == knobs);
    CHECK (rig.columnHeight (0) == height);

    // The screen reader's press does the same as a click.
    if (auto handler = header->createAccessibilityHandler())
    {
        CHECK (handler->getActions().invoke (juce::AccessibilityActionType::press));
        CHECK (! header->isExpanded());
        CHECK (handler->getActions().invoke (juce::AccessibilityActionType::press));
        CHECK (header->isExpanded());
    }

    // The neighbours are untouched.
    CHECK (! panel.isColumnSectionCollapsed ("Strings"));
}

LUTHIER_TEST (ColumnCollapse, foldedSectionsSurviveASaveAndReload)
{
    juce::var saved;

    {
        Rig rig;
        rig.panel->setColumnSectionCollapsed ("Body", true);
        rig.panel->setColumnSectionCollapsed ("Room", true);
        CHECK (rig.foldedCount() == 2);
        CHECK (rig.processor.getUiState().collapsedColumnSections.size() == 2);
        saved = stateOf (rig.processor);
    }

    Rig other;
    CHECK (other.foldedCount() == 0);

    restore (other.processor, saved);
    other.open();   // the editor is rebuilt after a host restores a session

    CHECK (other.panel->isColumnSectionCollapsed ("Body"));
    CHECK (other.panel->isColumnSectionCollapsed ("Room"));
    CHECK (other.foldedCount() == 2);
    CHECK (other.panel->getColumnSectionHeader ("Room") != nullptr
           && ! other.panel->getColumnSectionHeader ("Room")->isExpanded());
}

LUTHIER_TEST (ColumnCollapse, anOlderSessionOpensWithEverySectionExpanded)
{
    // A session written before sections could fold has no such key.
    auto state = juce::JSON::parse ("{}");

    {
        LuthierAudioProcessor writer;
        writer.prepareToPlay (kSr, kBlock);
        state = stateOf (writer);
    }

    auto* ui = state.getProperty ("ui", {}).getDynamicObject();
    CHECK (ui != nullptr);

    if (ui == nullptr)
        return;

    CHECK_MSG (ui->hasProperty ("collapsedColumnSections"), "the session does not save the folded sections");
    ui->removeProperty ("collapsedColumnSections");

    Rig rig;
    rig.panel->setColumnSectionCollapsed ("Body", true);
    CHECK (rig.processor.getUiState().collapsedColumnSections.size() == 1);

    restore (rig.processor, state);
    CHECK (rig.processor.getUiState().collapsedColumnSections.isEmpty());

    rig.open();
    CHECK (rig.foldedCount() == 0);

    // Junk where the list should be is ignored, not trusted.
    ui->setProperty ("collapsedColumnSections", juce::var (42));
    restore (rig.processor, state);
    CHECK (rig.processor.getUiState().collapsedColumnSections.isEmpty());

    juce::Array<juce::var> mixed { juce::var ("Instrument/Body"), juce::var (7), juce::var() };
    ui->setProperty ("collapsedColumnSections", mixed);
    restore (rig.processor, state);
    CHECK (rig.processor.getUiState().collapsedColumnSections == juce::StringArray { "Instrument/Body" });
}

LUTHIER_TEST (ColumnCollapse, aResetAndAFreshInstanceShowEverySection)
{
    Rig rig;
    CHECK (rig.foldedCount() == 0);

    for (int c = 1; c <= 3; ++c)
        for (const auto& heading : rig.panel->getColumnSections (c))
            rig.panel->setColumnSectionCollapsed (heading, true);

    CHECK (rig.foldedCount() > 10);

    rig.processor.resetEverything();
    CHECK (rig.processor.getUiState().collapsedColumnSections.isEmpty());

    rig.open();
    CHECK (rig.foldedCount() == 0);
}

LUTHIER_TEST (ColumnCollapse, goingToAFoldedSectionUnfoldsIt)
{
    Rig rig;
    auto& panel = *rig.panel;

    // Search's section jump.
    panel.setColumnSectionCollapsed ("Humanise", true);
    CHECK (panel.revealColumnSection ("Humanise"));
    CHECK (! panel.isColumnSectionCollapsed ("Humanise"));

    // Search's control jump: the section body is a place with an opener, so
    // walking to a control inside it unfolds it first.
    panel.setColumnSectionCollapsed ("Body", true);
    auto* body = panel.getColumnSectionBody ("Body");
    CHECK (body != nullptr);

    if (body != nullptr)
    {
        CHECK (search::SearchAnchors::hasOpener (*body));
        search::SearchAnchors::open (*body);
        CHECK (! panel.isColumnSectionCollapsed ("Body"));
    }
}
