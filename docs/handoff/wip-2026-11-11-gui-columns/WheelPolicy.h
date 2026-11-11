#pragma once

/*  spec/issues.md ISS-7: the one mouse-wheel policy for scrolling columns.

    Inside a scrolling column the plain wheel scrolls the column, whatever
    control the pointer happens to rest on; Ctrl (Cmd on macOS) plus the wheel
    adjusts the control under the pointer instead. The owner reported the
    Advanced columns as broken because a wheel over a control changed its value
    and the column never moved.

    LuthierKnob (through WheelPassSlider) follows this by construction. Every
    other control that eats the wheel - juce::ComboBox and plain juce::Slider
    above all - is routed here by the column's viewport, and a custom control
    with its own wheel handling asks passToScrollingColumn() first. A viewport
    that wants this behaviour marks itself with markScrollingColumn(). Outside
    such a viewport nothing changes: a control that is not in a scrolling
    column keeps its own wheel.
*/

#include <juce_gui_basics/juce_gui_basics.h>

namespace luthier
{
namespace WheelPolicy
{
    /** True when this wheel event should change the value rather than scroll. */
    inline bool wheelAdjustsValue (const juce::MouseEvent& e) noexcept
    {
        return e.mods.isCtrlDown() || e.mods.isCommandDown();
    }

    /** Marks a viewport as a scrolling column that owns the plain wheel. */
    void markScrollingColumn (juce::Viewport& viewport);

    /** The scrolling column holding `c`, or nullptr. */
    juce::Viewport* findScrollingColumn (const juce::Component& c);

    /** Hands every juce::ComboBox and juce::Slider under `root` that consumes
        the wheel itself over to the policy: its own wheel handling is switched
        off, so a plain wheel reaches the column. Sliders that already follow
        the policy (WheelPassSlider, so every LuthierKnob) are left alone.
        Cheap and idempotent: call it whenever the content may have grown. */
    void routeToScroll (juce::Component& root);

    /** Routes the single control `c` belongs to (itself or the nearest
        ComboBox or Slider above it), for controls created after the last
        routeToScroll(). */
    void routeControlUnder (juce::Component& c);

    /** For the column's own mouseWheelMove: a Ctrl/Cmd wheel that came up
        from a routed control is delivered back to that control, which then
        nudges its value. Returns true when it did. */
    bool deliverValueWheel (juce::Viewport& column, const juce::MouseEvent&, const juce::MouseWheelDetails&);

    /** For a custom control's mouseWheelMove: inside a scrolling column and
        without Ctrl/Cmd, passes the wheel on to the column and returns true;
        the control should then do nothing else with it. */
    bool passToScrollingColumn (juce::Component& control, const juce::MouseEvent&, const juce::MouseWheelDetails&);

    /** True if `c` has been routed (tests). */
    bool isRouted (const juce::Component& c);
}
} // namespace luthier
