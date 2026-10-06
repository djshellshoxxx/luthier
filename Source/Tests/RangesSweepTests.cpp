/*  SPEC-SWEEP ui-wiring UW-T8 (ui-wiring 23): range toggles under load. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PhysicalRange.h"
#include "../ParamMeta.h"

#include <atomic>
#include <thread>

#if defined (LUTHIER_ALLOCATION_COUNTER)
namespace luthier::tests
{
    long allocationsOnThisThread() noexcept;
}
#endif

using namespace luthier;
using namespace luthier::tests;

/*  UW-T8: for every family, stock/advanced toggled a hundred times while an
    audio thread renders. Each widening keeps a value where it is; each
    narrowing clamps a value beyond stock back to the stock bound; nothing
    tears the render. */
LUTHIER_TEST (Ranges, hundredTogglesClampCorrectlyWithoutAllocating)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    std::atomic<bool> stop { false };
    std::atomic<int> blocks { 0 };
    std::atomic<bool> finite { true };
    std::atomic<long> renderAllocations { 0 };
    std::atomic<bool> measuring { false };

    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels(),
                                                     processor.getTotalNumInputChannels()), 512);

        long before = 0;
        bool started = false;

        while (! stop.load())
        {
           #if defined (LUTHIER_ALLOCATION_COUNTER)
            if (! started && measuring.load())
            {
                started = true;
                before = allocationsOnThisThread();
            }
           #endif

            juce::MidiBuffer midi;

            if (blocks.load() % 200 == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 45, 0.8f), 0);

            buffer.clear();
            processor.processBlock (buffer, midi);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < 512; ++i)
                    if (! std::isfinite (buffer.getSample (ch, i)))
                        finite.store (false);

            blocks.fetch_add (1);
        }

       #if defined (LUTHIER_ALLOCATION_COUNTER)
        if (started)
            renderAllocations.store (allocationsOnThisThread() - before);
       #endif
        juce::ignoreUnused (before, started);
    });

    // Let the render warm up (first notes, first voicings) before counting.
    while (blocks.load() < 20)
        std::this_thread::yield();

    measuring.store (true);

    int widenFailures = 0, narrowFailures = 0, checked = 0;

    for (int f = 0; f < (int) RangeFamily::numFamilies; ++f)
    {
        const auto family = (RangeFamily) f;
        const auto ids = RangeRegistry::idsInFamily (family);

        if (ids.isEmpty() || family == RangeFamily::modulation)
            continue;   // the modulation family is setter clamps in the matrix, tested there

        const auto id = ids[0];
        const auto* spec = RangeRegistry::find (id);
        auto* param = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id));

        if (spec == nullptr || param == nullptr || spec->advancedMax <= spec->stockMax)
            continue;

        const float beyond = spec->stockMax + 0.5f * (spec->advancedMax - spec->stockMax);

        for (int toggle = 0; toggle < 100; ++toggle)
        {
            RangeState wide = processor.getRanges();
            wide.setFamilyAdvanced (family, true);
            processor.setRanges (wide);

            param->setValueNotifyingHost (param->convertTo0to1 (beyond));

            // Widening again keeps the plain value.
            processor.setRanges (wide);

            if (std::abs (param->convertFrom0to1 (param->getValue()) - beyond) > 1.0e-3f * (spec->advancedMax - spec->advancedMin))
                ++widenFailures;

            RangeState narrow = processor.getRanges();
            narrow.setFamilyAdvanced (family, false);
            processor.setRanges (narrow);

            if (std::abs (param->convertFrom0to1 (param->getValue()) - spec->stockMax) > 1.0e-3f * (spec->stockMax - spec->stockMin))
                ++narrowFailures;

            ++checked;
        }
    }

    stop.store (true);
    audio.join();

    CHECK (checked >= 300);
    CHECK_MSG (widenFailures == 0, juce::String (widenFailures) + " widenings moved a value");
    CHECK_MSG (narrowFailures == 0, juce::String (narrowFailures) + " narrowings did not clamp to stock");
    CHECK (blocks.load() > 10);
    CHECK_MSG (finite.load(), "the render produced a non-finite sample during the toggles");
    CHECK_MSG (renderAllocations.load() == 0,
               juce::String (renderAllocations.load()) + " allocations on the render thread while ranges toggled");
}

/*  UW-10 (ui-wiring 1): every parameter has a unit and a category. */
LUTHIER_TEST (Parameters, everyParameterHasAUnitAndACategory)
{
    LuthierAudioProcessor processor;
    juce::StringArray unknown;

    for (auto* p : processor.getParameters())
    {
        auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p);

        if (withId == nullptr)
            continue;

        if (ParamMeta::getUnit (*p) == ParamUnit::unknown)
            unknown.add (withId->paramID + " (label '" + p->getLabel() + "')");

        CHECK (ParamMeta::getCategory (withId->paramID).isNotEmpty());
    }

    CHECK_MSG (unknown.isEmpty(), "no unit for: " + unknown.joinIntoString (", "));

    if (auto* gain = processor.getState().getParameter (ParamIDs::masterGain))
    {
        CHECK (ParamMeta::getUnit (*gain) == ParamUnit::db);
        CHECK (ParamMeta::getUnitName (ParamUnit::db) == "decibels");
    }

    CHECK (ParamMeta::getCategory (ParamIDs::ampGain) == "amp");
}
