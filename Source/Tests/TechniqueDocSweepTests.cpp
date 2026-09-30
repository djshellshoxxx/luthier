/*  SPEC-SWEEP docs/PLAYING_TECHNIQUES.md checks the Phase-1 audit found
    untested (PT-4, PT-8). */

#include "TestFramework.h"

#include "../DSP/String/StringEngine.h"
#include "../PluginProcessor.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;

    Excitation::Params pluckAt (double velocity)
    {
        Excitation::Params p;
        p.material = Excitation::Material::PickCelluloid;
        p.kind = Excitation::Kind::Pluck;
        p.pluckPosition = 0.18;
        p.velocity = velocity;
        p.brightness = 0.5;
        return p;
    }
}

/*  PT-4 ("Pluck: a re-pluck on a ringing string mutes the old note over 5 ms
    first"): the steal ramp lasts round (0.005 * sr) samples, and the fade does
    not itself click - no step while it runs is bigger than the ringing note's
    own largest step. */
LUTHIER_TEST (StringEngine, aRepluckMutesTheRingingStringOverFiveMs)
{
    StringEngine s;
    s.prepare (kSr, 512);

    StringEngine::Physical physical;
    physical.sustainSeconds = 6.0;
    physical.openBrightnessHz = 5000.0;
    physical.inharmonicityB = 0.00005;
    s.setPhysical (physical);
    s.snapToFrequency (196.0);

    s.excite (pluckAt (0.8));

    double previous = 0.0, ringingMaxStep = 0.0;

    for (int i = 0; i < (int) (kSr * 0.3); ++i)
    {
        const double y = s.processSample (0.0);

        if (i > (int) (kSr * 0.2))
            ringingMaxStep = std::max (ringingMaxStep, std::abs (y - previous));

        previous = y;
    }

    s.excite (pluckAt (0.8));
    CHECK_MSG (s.isStealPending(), "a re-pluck on a ringing string did not steal");

    int fadeSamples = 0;
    double fadeMaxStep = 0.0;

    while (s.isStealPending() && fadeSamples < (int) kSr)
    {
        const double y = s.processSample (0.0);
        fadeMaxStep = std::max (fadeMaxStep, std::abs (y - previous));
        previous = y;
        ++fadeSamples;
    }

    CHECK_MSG (std::abs (fadeSamples - (int) std::lround (0.005 * kSr)) <= 1,
               "the steal fade took " + juce::String (fadeSamples) + " samples");
    CHECK_MSG (fadeMaxStep <= ringingMaxStep * 1.05 + 1.0e-9,
               "the fade stepped " + juce::String (fadeMaxStep, 5) + " against the note's own "
                 + juce::String (ringingMaxStep, 5));
}

/*  PT-8 (Vibrato): finger vibrato rises faster than it falls; the depth ramps
    in rather than arriving at once; two strings do not wobble in lockstep. */
LUTHIER_TEST (StringEngine, fingerVibratoIsAsymmetricAndRampsIn)
{
    // ---- the shape: rise time < fall time over one cycle ----------------------
    {
        Lfo lfo;
        lfo.prepare (kSr);
        lfo.setShape (Lfo::Shape::FingerVibrato);
        lfo.setRate (5.0);
        lfo.setPhase (0.0);

        const int period = (int) (kSr / 5.0);
        std::vector<double> cycle ((size_t) period);

        for (auto& v : cycle)
            v = lfo.next();

        const auto top = (int) (std::max_element (cycle.begin(), cycle.end()) - cycle.begin());
        const auto bottom = (int) (std::min_element (cycle.begin(), cycle.end()) - cycle.begin());

        const int rise = top - bottom >= 0 ? top - bottom : top - bottom + period;
        const int fall = period - rise;

        CHECK_MSG (rise < fall, "finger vibrato rose over " + juce::String (rise)
                                  + " samples and fell over " + juce::String (fall));
    }

    // ---- through the engine: ramp-in and per-string phase ---------------------
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, 512);

    juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumInputChannels(),
                                                 processor.getTotalNumOutputChannels()), 512);
    auto& engine = processor.getEngine();

    auto render = [&] (juce::MidiBuffer& midi) { buffer.clear(); processor.processBlock (buffer, midi); };

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 45, 0.8f), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 64, 0.8f), 0);
    render (midi);

    for (int i = 0; i < 10; ++i) { juce::MidiBuffer none; render (none); }

    int a = -1, b = -1;

    for (int s = 0; s < engine.getNumStrings(); ++s)
        if (engine.getMidiInterpreter().getStringMidiNote (s) >= 0)
            (a < 0 ? a : b) = s;

    CHECK (a >= 0 && b >= 0);

    if (a < 0 || b < 0)
        return;

    auto centsOff = [&engine] (int s, double baseHz)
    {
        return 1200.0 * std::log2 (engine.getString (s).getCurrentFrequency() / baseHz);
    };

    const double baseA = engine.getString (a).getCurrentFrequency();
    const double baseB = engine.getString (b).getCurrentFrequency();

    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 0);
    render (midi);

    const double firstBlock = std::abs (centsOff (a, baseA));

    double laterMax = 0.0, phaseDifference = 0.0;

    for (int i = 0; i < 200; ++i)
    {
        juce::MidiBuffer none;
        render (none);

        if (i > 100)
        {
            const double ca = centsOff (a, baseA), cb = centsOff (b, baseB);
            laterMax = std::max (laterMax, std::abs (ca));
            phaseDifference = std::max (phaseDifference, std::abs (ca - cb));
        }
    }

    CHECK_MSG (laterMax > 5.0, "full vibrato only reached " + juce::String (laterMax, 2) + " cents");
    CHECK_MSG (firstBlock < laterMax * 0.25, "the depth arrived at once: " + juce::String (firstBlock, 2)
                                                + " cents in the first block of " + juce::String (laterMax, 2));
    CHECK_MSG (phaseDifference > 1.0, "the two strings wobbled in lockstep");
}
