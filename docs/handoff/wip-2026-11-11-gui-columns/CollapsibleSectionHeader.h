#pragma once

/*  The clickable heading row of a section in an Advanced column.

    The owner asked for collapsible interface sections: a column of fifteen
    titled sections is easier to work in when the ones not in use fold away.
    This is the button laid over a heading's row. A click, or Enter or Space
    while it has focus, folds or unfolds the section; a chevron at the right
    shows which (down open, right folded). The heading text itself stays with
    the column's cached background paint.

    Screen readers get a button that is expandable and says whether it is
    expanded, with the section's name as its title.
*/

#include <juce_gui_basics/juce_gui_basics.h>

namespace luthier
{

class CollapsibleSectionHeader : public juce::Button
{
public:
    explicit CollapsibleSectionHeader (const juce::String& heading);

    void setExpanded (bool shouldBeExpanded);
    bool isExpanded() const noexcept { return expanded; }

    /** Renames the section the header speaks for (titles, tooltip). */
    void setHeading (const juce::String& heading);
    const juce::String& getHeading() const noexcept { return heading; }

    /** Called after a click or key toggled it, with the new state. */
    std::function<void (bool expanded)> onToggle;

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void clicked() override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override   { repaint(); }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    void updateDescriptions();

    juce::String heading;
    bool expanded = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CollapsibleSectionHeader)
};

} // namespace luthier
