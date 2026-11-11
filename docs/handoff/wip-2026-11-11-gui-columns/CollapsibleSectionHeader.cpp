#include "CollapsibleSectionHeader.h"
#include "Theme.h"

namespace luthier
{
namespace
{
    class HeaderAccessibility : public juce::AccessibilityHandler
    {
    public:
        explicit HeaderAccessibility (CollapsibleSectionHeader& h)
            : juce::AccessibilityHandler (h, juce::AccessibilityRole::button,
                                          juce::AccessibilityActions()
                                              .addAction (juce::AccessibilityActionType::press, [&h] { h.clicked(); })
                                              .addAction (juce::AccessibilityActionType::toggle, [&h] { h.clicked(); })),
              header (h)
        {
        }

        juce::AccessibleState getCurrentState() const override
        {
            auto state = juce::AccessibilityHandler::getCurrentState().withExpandable();
            return header.isExpanded() ? state.withExpanded() : state.withCollapsed();
        }

        juce::String getHelp() const override { return header.getTooltip(); }

    private:
        CollapsibleSectionHeader& header;
    };
}

CollapsibleSectionHeader::CollapsibleSectionHeader (const juce::String& h)
    : juce::Button (h)
{
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setHeading (h);
}

void CollapsibleSectionHeader::setHeading (const juce::String& h)
{
    heading = h;
    setName (heading);
    updateDescriptions();
}

void CollapsibleSectionHeader::setExpanded (bool shouldBeExpanded)
{
    if (expanded == shouldBeExpanded)
        return;

    expanded = shouldBeExpanded;
    updateDescriptions();
    repaint();

    if (auto* handler = getAccessibilityHandler())
        handler->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
}

void CollapsibleSectionHeader::updateDescriptions()
{
    setTitle (heading + " section");
    setTooltip (expanded ? "Click to fold the " + heading + " section away (Enter or Space when focused)"
                         : "Folded: click to show the " + heading + " controls (Enter or Space when focused)");
    setHelpText (expanded ? "Expanded" : "Collapsed");
}

void CollapsibleSectionHeader::clicked()
{
    setExpanded (! expanded);

    if (onToggle != nullptr)
        onToggle (expanded);
}

bool CollapsibleSectionHeader::keyPressed (const juce::KeyPress& key)
{
    if (isEnabled() && (key.isKeyCode (juce::KeyPress::returnKey) || key.isKeyCode (juce::KeyPress::spaceKey)))
    {
        // Synchronous, unlike triggerClick(): the column relayouts before the
        // next key arrives.
        clicked();
        return true;
    }

    return juce::Button::keyPressed (key);
}

void CollapsibleSectionHeader::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat();

    if (highlighted || down)
    {
        g.setColour (Palette::accent.withAlpha (down ? 0.16f : 0.08f));
        g.fillRoundedRectangle (bounds, Metrics::controlCorner);
    }

    if (hasKeyboardFocus (false))
    {
        g.setColour (Palette::accentBright);
        g.drawRoundedRectangle (bounds.reduced (1.0f), Metrics::controlCorner, 1.5f);
    }

    // Down while open, right while folded: the usual disclosure triangle.
    const auto glyph = juce::Point<float> (bounds.getRight() - 10.0f, bounds.getCentreY());
    LuthierLookAndFeel::drawChevron (g, glyph, 4.0f, expanded ? 2 : 1,
                                     highlighted || ! expanded ? Palette::accentBright : Palette::accent, 1.6f);
}

std::unique_ptr<juce::AccessibilityHandler> CollapsibleSectionHeader::createAccessibilityHandler()
{
    return std::make_unique<HeaderAccessibility> (*this);
}

} // namespace luthier
