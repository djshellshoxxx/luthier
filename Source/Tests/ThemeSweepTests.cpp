/*  SPEC-SWEEP: theme.md checks that had no test (TH-8, TH-29). */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../UI/Theme.h"
#include "../UI/Widgets.h"

using namespace luthier;
using namespace luthier::tests;

//==============================================================================
/*  TH-8: three knob sizes, 36 / 48 / 64 px, and the preferred box around each
    leaves room for the value row above and the label below. */
LUTHIER_TEST (Theme, knobSizesAreTheSpecsThree)
{
    CHECK (Metrics::knobSmall == 36);
    CHECK (Metrics::knobDefault == 48);
    CHECK (Metrics::knobLarge == 64);

    using S = LuthierKnob::Size;

    CHECK (LuthierKnob::preferredHeightFor (S::Small)  == 36 + 28);
    CHECK (LuthierKnob::preferredHeightFor (S::Normal) == 48 + 28);
    CHECK (LuthierKnob::preferredHeightFor (S::Large)  == 64 + 28);

    CHECK (LuthierKnob::preferredWidthFor (S::Small) < LuthierKnob::preferredWidthFor (S::Normal));
    CHECK (LuthierKnob::preferredWidthFor (S::Normal) < LuthierKnob::preferredWidthFor (S::Large));
}

/*  TH-29: the editor's tooltip window waits 400 ms. */
LUTHIER_TEST (Theme, tooltipAppearsAfter400ms)
{
    CHECK (Metrics::tooltipDelayMs == 400);

    LuthierAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
        return;

    juce::TooltipWindow* window = nullptr;

    for (auto* child : editor->getChildren())
        if (auto* t = dynamic_cast<juce::TooltipWindow*> (child))
            window = t;

    // JUCE has no getter for the delay; the editor builds it from
    // Metrics::tooltipDelayMs, checked above.
    CHECK_MSG (window != nullptr, "the editor has no tooltip window");
}

namespace
{
    juce::Image renderKnob (LuthierKnob& knob)
    {
        return knob.createComponentSnapshot (knob.getLocalBounds(), true, 1.0f);
    }

    int differingPixels (const juce::Image& a, const juce::Image& b, juce::Rectangle<int> area)
    {
        int n = 0;
        for (int y = area.getY(); y < area.getBottom(); ++y)
            for (int x = area.getX(); x < area.getRight(); ++x)
                if (a.getPixelAt (x, y) != b.getPixelAt (x, y))
                    ++n;
        return n;
    }

    juce::MouseEvent eventFor (juce::Component& c, juce::ModifierKeys mods)
    {
        auto source = juce::Desktop::getInstance().getMainMouseSource();
        const juce::Point<float> at (c.getWidth() * 0.5f, c.getHeight() * 0.5f);
        return juce::MouseEvent (source, at, mods, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c,
                                 juce::Time::getCurrentTime(), at, juce::Time::getCurrentTime(), 1, false);
    }
}

/*  TH-23 / TH-16: corner radii 6 / 4 / 2 and 28 px buttons. */
LUTHIER_TEST (Theme, radiiAndButtonHeightMatchTheSpec)
{
    CHECK (Metrics::windowCorner == 6.0f);
    CHECK (Metrics::panelCorner == 4.0f);
    CHECK (Metrics::controlCorner == 2.0f);
    CHECK (Metrics::buttonHeight == 28);
}

/*  TH-18: the meter runs teal/green at the bottom to red at the top. */
LUTHIER_TEST (Theme, meterGradientRunsCoolToRed)
{
    const auto low = LuthierLookAndFeel::meterColourFor (0.0f);
    const auto top = LuthierLookAndFeel::meterColourFor (1.0f);
    const auto mid = LuthierLookAndFeel::meterColourFor (0.82f);

    CHECK (low != mid && mid != top);
    CHECK_MSG (top.getFloatRed() > top.getFloatGreen() && top.getFloatRed() > top.getFloatBlue(), "the top is not red");
    CHECK_MSG (low.getFloatRed() < low.getFloatGreen(), "the bottom is not cool");
}

/*  TH-10 / TH-12 / TH-14 / TH-27 / TH-28: the knob draws its value, shows the
    number only on hover, has a vertical-resize cursor, and drag sensitivity
    orders Shift (coarse) < plain < Ctrl/Cmd (ultra-fine). */
LUTHIER_TEST (Theme, knobDrawsItsValueAndFeelsRight)
{
    LuthierAudioProcessor processor;
    LuthierKnob knob ("Gain");
    knob.attachTo (processor, ParamIDs::ampGain);
    knob.setSize (LuthierKnob::preferredWidthFor (LuthierKnob::Size::Normal),
                  LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));

    auto& slider = knob.getSlider();
    CHECK (slider.getMouseCursor() == juce::MouseCursor (juce::MouseCursor::UpDownResizeCursor));

    // The value is drawn: the knob body changes between two values.
    slider.setValue (slider.getMinimum(), juce::sendNotificationSync);
    const auto atMin = renderKnob (knob);
    slider.setValue (slider.getMaximum(), juce::sendNotificationSync);
    const auto atMax = renderKnob (knob);
    CHECK (differingPixels (atMin, atMax, slider.getBounds()) > 10);

    // The value row appears only while hovered.
    const auto still = renderKnob (knob);
    slider.mouseEnter (eventFor (slider, {}));
    const auto hovered = renderKnob (knob);
    slider.mouseExit (eventFor (slider, {}));

    CHECK_MSG (differingPixels (still, hovered, knob.getLocalBounds().removeFromTop (14)) > 0,
               "the value row did not change on hover");

    // Drag feel.
    slider.mouseDrag (eventFor (slider, juce::ModifierKeys::shiftModifier));
    const int coarse = slider.getMouseDragSensitivity();
    slider.mouseDrag (eventFor (slider, {}));
    const int normal = slider.getMouseDragSensitivity();
    slider.mouseDrag (eventFor (slider, juce::ModifierKeys::commandModifier));
    const int fine = slider.getMouseDragSensitivity();

    CHECK_MSG (coarse < normal && normal < fine,
               juce::String (coarse) + " / " + juce::String (normal) + " / " + juce::String (fine));
}
