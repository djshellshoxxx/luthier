/*  SPEC-SWEEP rhythm-engine UI checks (RE-34, RE-37, RE-38): the RHYTHM tab's
    editors and readouts, driven the way a click drives them. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/RhythmPanel.h"
#include "../Rhythm/Patterns.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    juce::MouseEvent clickAt (juce::Component& target, juce::Point<float> p)
    {
        auto source = juce::Desktop::getInstance().getMainMouseSource();
        return juce::MouseEvent (source, p, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, juce::Time::getCurrentTime(), p,
                                 juce::Time::getCurrentTime(), 1, false);
    }
}

/*  RE-34: clicking a fingerpick cell assigns that step to that finger; clicking
    it again clears the step. */
LUTHIER_TEST (RhythmPanelUi, fingerpickGridTogglesAFingerStep)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& rhythm = processor.getEngine().getRhythmEngine();

    RhythmPattern pattern;
    pattern.setName ("Sweep pick");
    pattern.setKind (RhythmPattern::Kind::fingerpick);
    pattern.setSubdivision (Subdivision::sixteenth);
    pattern.setLength (16);
    rhythm.setPattern (pattern);

    FingerpickGrid grid (processor);
    grid.setSize (FingerpickGrid::headerWidth + 16 * 20, FingerpickGrid::preferredHeight);
    grid.refresh();

    const int step = 5, finger = 2;
    const juce::Point<float> cell ((float) (FingerpickGrid::headerWidth + step * 20 + 10),
                                   (float) (finger * FingerpickGrid::rowHeight + FingerpickGrid::rowHeight / 2));

    grid.mouseDown (clickAt (grid, cell));

    auto after = rhythm.getPattern().getFingerpickStep (step);
    CHECK (after.active);
    CHECK ((int) after.finger == finger);

    grid.mouseDown (clickAt (grid, cell));
    CHECK (! rhythm.getPattern().getFingerpickStep (step).active);
}
