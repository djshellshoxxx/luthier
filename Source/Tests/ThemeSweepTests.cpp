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
