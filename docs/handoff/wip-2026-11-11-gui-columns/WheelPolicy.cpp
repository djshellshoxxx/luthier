#include "WheelPolicy.h"
#include "Widgets.h"

namespace luthier
{
namespace
{
    constexpr const char* columnProperty = "luthier.wheel.column";
    constexpr const char* routedProperty = "luthier.wheel.routed";

    /** A control whose own wheel handling the policy has to switch off. */
    bool consumesWheel (const juce::Component& c)
    {
        if (dynamic_cast<const juce::ComboBox*> (&c) != nullptr)
            return true;

        // WheelPassSlider (and so LuthierKnob's slider) already scrolls.
        return dynamic_cast<const juce::Slider*> (&c) != nullptr
            && dynamic_cast<const WheelPassSlider*> (&c) == nullptr;
    }

    void setWheel (juce::Component& c, bool enabled)
    {
        if (auto* box = dynamic_cast<juce::ComboBox*> (&c))
            box->setScrollWheelEnabled (enabled);
        else if (auto* slider = dynamic_cast<juce::Slider*> (&c))
            slider->setScrollWheelEnabled (enabled);
    }

    void route (juce::Component& c)
    {
        if (! consumesWheel (c) || c.getProperties().contains (routedProperty))
            return;

        setWheel (c, false);
        c.getProperties().set (routedProperty, true);
    }

    /** The routed control `c` sits in, below `stop`. */
    juce::Component* routedControlFor (juce::Component* c, const juce::Component* stop)
    {
        for (auto* p = c; p != nullptr && p != stop; p = p->getParentComponent())
            if (p->getProperties().contains (routedProperty))
                return p;

        return nullptr;
    }
}

namespace WheelPolicy
{
    void markScrollingColumn (juce::Viewport& viewport)
    {
        viewport.getProperties().set (columnProperty, true);
    }

    juce::Viewport* findScrollingColumn (const juce::Component& c)
    {
        for (auto* p = c.getParentComponent(); p != nullptr; p = p->getParentComponent())
            if (auto* viewport = dynamic_cast<juce::Viewport*> (p))
                if (viewport->getProperties().contains (columnProperty))
                    return viewport;

        return nullptr;
    }

    void routeToScroll (juce::Component& root)
    {
        route (root);

        for (auto* child : root.getChildren())
            routeToScroll (*child);
    }

    void routeControlUnder (juce::Component& c)
    {
        for (auto* p = &c; p != nullptr; p = p->getParentComponent())
        {
            if (consumesWheel (*p))
            {
                route (*p);
                return;
            }

            if (dynamic_cast<juce::Viewport*> (p) != nullptr)
                return;
        }
    }

    bool deliverValueWheel (juce::Viewport& column, const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        if (! wheelAdjustsValue (e) || e.originalComponent == nullptr)
            return false;

        auto* control = routedControlFor (e.originalComponent, &column);

        if (control == nullptr)
            return false;

        // The control's own handler, switched on for this one event.
        setWheel (*control, true);
        control->mouseWheelMove (e.getEventRelativeTo (control), wheel);
        setWheel (*control, false);
        return true;
    }

    bool passToScrollingColumn (juce::Component& control, const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        if (wheelAdjustsValue (e) || findScrollingColumn (control) == nullptr)
            return false;

        // Component's own handler hands the wheel to the parent, and on up to
        // the column's viewport.
        control.juce::Component::mouseWheelMove (e, wheel);
        return true;
    }

    bool isRouted (const juce::Component& c)
    {
        return c.getProperties().contains (routedProperty);
    }
}
} // namespace luthier
