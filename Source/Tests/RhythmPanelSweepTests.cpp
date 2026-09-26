/*  SPEC-SWEEP rhythm-engine UI checks (RE-34, RE-37, RE-38): the RHYTHM tab's
    editors and readouts, driven the way a click drives them. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/RhythmPanel.h"
#include "../Rhythm/Patterns.h"
#include "../Rhythm/RhythmEngine.h"

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

/*  RE-37 (rhythm-engine 8.7): with a chord held and the rhythm engine playing,
    the indicators name the chord and show its voicing as dots. */
LUTHIER_TEST (RhythmPanelUi, indicatorsShowChordVoicingAndNextStroke)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& rhythm = processor.getEngine().getRhythmEngine();
    PatternLibrary patterns;
    const auto strums = patterns.findByKind (RhythmPattern::Kind::strum);
    CHECK (! strums.isEmpty());

    if (strums.isEmpty())
        return;

    rhythm.setPattern (patterns.getPattern (strums[0]));
    rhythm.setFreeRun (true);
    rhythm.setEnabled (true);

    juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumInputChannels(),
                                                 processor.getTotalNumOutputChannels()), kBlock);

    for (int block = 0; block < 20; ++block)
    {
        juce::MidiBuffer midi;

        if (block == 0)
            for (int n : { 48, 52, 55 })
                midi.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0);

        buffer.clear();
        processor.processBlock (buffer, midi);
    }

    RhythmIndicators indicators (processor);
    indicators.refresh();

    CHECK_MSG (indicators.getChordText().startsWith ("C"), "indicator chord was '" + indicators.getChordText() + "'");
    CHECK (indicators.getNumVoicedDots() >= 3);
}

/*  RE-5 (rhythm-engine 0.5): the instrument's Humanize macro is the one
    humanise control - it scales the rhythm engine's (kit) feel, which stays as
    written at the macro's default. */
LUTHIER_TEST (RhythmPatterns, macroHumanizeScalesTheRhythmFeel)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& rhythm = processor.getEngine().getRhythmEngine();
    RhythmHumanise kit;
    kit.amount = 0.8;
    rhythm.setHumanise (kit);
    rhythm.setEnabled (true);

    auto* macro = processor.getState().getParameter (ParamIDs::macroHumanize);
    CHECK (macro != nullptr);

    if (macro == nullptr)
        return;

    juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumInputChannels(),
                                                 processor.getTotalNumOutputChannels()), kBlock);

    auto amountAt = [&] (float macroValue)
    {
        macro->setValueNotifyingHost (macro->convertTo0to1 (macroValue));

        for (int i = 0; i < 2; ++i)
        {
            juce::MidiBuffer midi;
            buffer.clear();
            processor.processBlock (buffer, midi);
        }

        return rhythm.getBlockHumanise().amount;
    };

    CHECK_NEAR (amountAt (0.4f), 0.8, 1.0e-6);    // the default leaves the kit alone
    CHECK_NEAR (amountAt (0.0f), 0.0, 1.0e-6);    // a machine
    CHECK_NEAR (amountAt (0.8f), 1.6, 1.0e-6);    // twice as loose
    CHECK_NEAR (rhythm.getHumanise().amount, 0.8, 1.0e-9);   // the kit's own value is untouched
}
