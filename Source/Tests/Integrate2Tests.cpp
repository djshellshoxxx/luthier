/*  INTEGRATE-2: checks that belong to no one feature but to the five merged
    together (FEAT-SEARCH, FEAT-ASSIST, FEAT-RIFFS, FEAT-MIC, FEAT-BROWSER on
    the integration branch with FEAT-JAM, FEAT-CPU, FEAT-NORMALIZE and
    FEAT-STRINGS). docs/coverage/INTEGRATE-2.md lists what each one settles.
*/

#include "TestFramework.h"
#include "../PluginProcessor.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/Theme.h"
#include "../UI/WorkspaceTabStrip.h"

using namespace luthier;
using namespace luthier::tests;

//==============================================================================
/*  Concern (d): with JAM (FEAT-JAM) and RIFFS (FEAT-RIFFS) both between TUNE
    and LIVE, column 4's strip still lays out at its 480-point minimum: every
    tab can be selected and is then whole and on screen, visible tabs never
    overlap each other or the scroll controls, and the overflow menu lists
    every tab. */
LUTHIER_TEST (Integrate2, columnFourTabStripFitsAt480WithJamAndRiffs)
{
    LuthierAudioProcessor processor;
    juce::StringArray names;

    {
        AdvancedPanel panel (processor);
        panel.setSize (1600, 900);

        for (int i = 0; i < panel.getNumWorkspaceTabs(); ++i)
            names.add (panel.getWorkspaceTabName (i));
    }

    const int tune = names.indexOf ("TUNE"), jam = names.indexOf ("JAM"), riffs = names.indexOf ("RIFFS"),
              live = names.indexOf ("LIVE");
    CHECK_MSG (tune >= 0 && jam == tune + 1 && riffs == jam + 1 && live == riffs + 1,
               "column 4 should read TUNE, JAM, RIFFS, LIVE: " + names.joinIntoString (", "));

    juce::OwnedArray<juce::TextButton> buttons;
    juce::Array<juce::Button*> raw;

    for (const auto& name : names)
        raw.add (buttons.add (new juce::TextButton (name)));

    WorkspaceTabStrip strip;
    strip.setTabs (raw);

    // AdvancedPanel::resized: the strip is column 4 less the workspace's ?.
    strip.setSize (480 - Metrics::buttonHeight - Metrics::gridHalf, Metrics::buttonHeight);

    CHECK (strip.isOverflowing());
    CHECK (strip.getMenuItems() == names);

    for (int tab = 0; tab < names.size(); ++tab)
    {
        strip.setSelectedIndex (tab);
        CHECK_MSG (strip.isTabVisible (tab), names[tab] + " is not on screen when selected at 480 px");

        auto* selected = buttons[tab];
        CHECK_MSG (strip.getLocalBounds().contains (selected->getBounds()), names[tab] + " is clipped at 480 px");
        CHECK_MSG (selected->getWidth() + 1 >= juce::jmin (strip.getNaturalWidth (tab), strip.getWidth()),
                   names[tab] + " is narrower than its label at 480 px");

        juce::Array<juce::Rectangle<int>> shown;

        for (int i = 0; i < names.size(); ++i)
        {
            if (! strip.isTabVisible (i))
                continue;

            const auto r = buttons[i]->getBounds();

            for (const auto& other : shown)
                CHECK_MSG (! other.intersects (r), names[i] + " overlaps a tab at 480 px");

            CHECK_MSG (! r.intersects (strip.getMenuButton().getBounds())
                         && ! r.intersects (strip.getLeftArrow().getBounds())
                         && ! r.intersects (strip.getRightArrow().getBounds()),
                       names[i] + " sits under the strip's arrows or menu at 480 px");

            shown.add (r);
        }
    }

    for (auto* control : { static_cast<juce::Component*> (&strip.getMenuButton()),
                           static_cast<juce::Component*> (&strip.getLeftArrow()),
                           static_cast<juce::Component*> (&strip.getRightArrow()) })
        CHECK (control->isVisible() && strip.getLocalBounds().contains (control->getBounds()));
}
