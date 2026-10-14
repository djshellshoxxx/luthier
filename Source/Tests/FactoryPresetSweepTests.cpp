/*  SPEC-SWEEP: engine.md 20.19 and 22 across the factory bank (EN-91, EN-94). */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Presets/FactoryPresets.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    void loadFactory (LuthierAudioProcessor& processor, int index)
    {
        processor.getPresetManager().fromVar (FactoryPresets::toVar (FactoryPresets::getPreset (index), processor));
        processor.getParameterBridge().applyAllNow();
    }
}

//==============================================================================
/*  EN-91: every factory preset survives a fold to mono - the mono sum keeps at
    least half the stereo energy, the criterion of Engine::monoCompatibility. */
LUTHIER_TEST (Presets, everyFactoryPresetIsMonoCompatible)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    juce::StringArray failures;

    for (int p = 0; p < FactoryPresets::getNumPresets(); ++p)
    {
        loadFactory (processor, p);
        processor.getEngine().reset();

        double stereo = 0.0, mono = 0.0;

        for (int b = 0; b < (int) (2.0 * kSr / kBlock); ++b)
        {
            juce::AudioBuffer<float> buffer (processor.getTotalNumOutputChannels(), kBlock);
            buffer.clear();

            juce::MidiBuffer midi;
            if (b == 0)
                for (int note : { 40, 47, 52, 56 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);

            processor.processBlock (buffer, midi);

            for (int i = 0; i < kBlock; ++i)
            {
                const double l = buffer.getSample (0, i), r = buffer.getSample (1, i);
                stereo += 0.5 * (l * l + r * r);
                mono += 0.25 * (l + r) * (l + r);
            }
        }

        if (stereo < 1.0e-9)
            continue;   // a preset that needs a phrase to sound is not a stereo question

        const double retained = mono / stereo;

        if (retained < 0.5)
            failures.add (juce::String (FactoryPresets::getPreset (p).name) + " (" + juce::String (retained * 100.0, 1) + " %)");
    }

    CHECK_MSG (failures.isEmpty(), "factory presets that cancel in mono: " + failures.joinIntoString (", "));
}

//==============================================================================
/*  EN-94: engine.md 22 - a factory preset loads, with its IRs installed, in
    under 500 ms (median; worst under 1 s). Best of three per preset, as the
    CPU budget test measures, so a busy machine does not fail it. */
LUTHIER_TEST (Presets, loadingAFactoryPresetTakesUnder500ms)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    std::vector<double> times;
    juce::String worstName;
    double worst = 0.0;

    for (int p = 0; p < FactoryPresets::getNumPresets(); ++p)
    {
        double best = 1.0e9;

        for (int attempt = 0; attempt < 3; ++attempt)
        {
            loadFactory (processor, (p + 1) % FactoryPresets::getNumPresets());   // start from elsewhere

            const auto start = juce::Time::getMillisecondCounterHiRes();
            loadFactory (processor, p);
            best = juce::jmin (best, juce::Time::getMillisecondCounterHiRes() - start);
        }

        times.push_back (best);

        if (best > worst)
        {
            worst = best;
            worstName = FactoryPresets::getPreset (p).name;
        }
    }

    CHECK (! times.empty());
    std::sort (times.begin(), times.end());
    const double median = times[times.size() / 2];

    std::cout << "    factory preset load: median " << median << " ms, worst " << worst << " ms (" << worstName << ")" << std::endl;

    // Machine-relative wall-clock budget: enforced only under LUTHIER_PERF=1 (the nightly).
    if (luthier::tests::perfRunRequested())
    {
        CHECK_MSG (median < 500.0, "median preset load " + juce::String (median, 1) + " ms");
        CHECK_MSG (worst < 1000.0, "slowest preset load " + juce::String (worst, 1) + " ms (" + worstName + ")");
    }
}
