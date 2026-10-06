/*  spec/issues.md ISS-7 (ported from PR #2, claude/clever-hopper-07uz7t):
    the wheel scrolls an Advanced column; Ctrl+wheel nudges the knob under the
    pointer; an overflowing column says so with a chevron at the end that has
    more; popup rows never shrink below a readable height.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/Theme.h"
#include "../UI/Widgets.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    template <typename T>
    void collect (juce::Component& root, juce::Array<T*>& out)
    {
        if (auto* t = dynamic_cast<T*> (&root))
            out.add (t);

        for (auto* child : root.getChildren())
            collect<T> (*child, out);
    }

    template <typename T>
    T* findOne (juce::Component& root)
    {
        if (auto* found = dynamic_cast<T*> (&root))
            return found;

        for (auto* child : root.getChildren())
            if (auto* found = findOne<T> (*child))
                return found;

        return nullptr;
    }
}

LUTHIER_TEST (ScrollWheel, theWheelScrollsAColumnUnlessCtrlIsHeld)
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

    // Plain wheel: the column moves, the parameter does not.
    const float valueBefore = param->getValue();
    wheelOver ({}, -1.0f);

    CHECK_MSG (viewport->getViewPositionY() > 0,
               "the wheel over a knob did not scroll the column; it is still at the top");
    CHECK_MSG (juce::approximatelyEqual (param->getValue(), valueBefore),
               "the wheel over a knob changed " + knob->getLearnParameterId());

    if (hinting != nullptr)
        CHECK_MSG (hinting->isTopHintShowing(), "a scrolled column shows no hint above");

    // Ctrl+wheel: the parameter moves, the column does not.
    const int scrollBefore = viewport->getViewPositionY();
    const float direction = valueBefore < 0.5f ? 1.0f : -1.0f;

    wheelOver (juce::ModifierKeys::ctrlModifier, direction);

    CHECK_MSG (! juce::approximatelyEqual (param->getValue(), valueBefore),
               "Ctrl+wheel over a knob did not nudge " + knob->getLearnParameterId());
    CHECK_MSG (viewport->getViewPositionY() == scrollBefore,
               "Ctrl+wheel scrolled the column as well as nudging the knob");

    // A click on the bottom hint pages the column down.
    if (hinting != nullptr)
    {
        hinting->setViewPosition (0, 0);
        hinting->pageBy (1);
        CHECK (hinting->getViewPositionY() > 0);
    }
}

LUTHIER_TEST (ScrollWheel, popupRowsAndScrollbarButtonsAreReadable)
{
    LuthierLookAndFeel lnf;

    int w = 0, h = 0;
    lnf.getIdealPopupMenuItemSize ("Item", false, 4, w, h);
    CHECK (h >= LuthierLookAndFeel::minimumPopupItemHeight);

    lnf.getIdealPopupMenuItemSize ({}, true, 4, w, h);   // separators keep their own height
    CHECK (h < LuthierLookAndFeel::minimumPopupItemHeight);

    CHECK (lnf.areScrollbarButtonsVisible());
}
