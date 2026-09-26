/*  SPEC-SWEEP: advanced-ranges.md tests the audit found missing (AR-11, AR-12,
    AR-24, AR-T10 with the AR-21 fix). */

#include "TestFramework.h"

#include "../Parameters.h"
#include "../DSP/Common/DspCommon.h"
#include "../PhysicalRange.h"
#include "../PluginProcessor.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    juce::AudioParameterFloat* gainOf (LuthierAudioProcessor& p)
    {
        return dynamic_cast<juce::AudioParameterFloat*> (p.getState().getParameter (ParamIDs::ampGain));
    }

    RangeState ampUnlocked()
    {
        RangeState s;
        s.setFamilyAdvanced (RangeFamily::amp, true);
        return s;
    }
}

//==============================================================================
/*  AR-T10 / AR-21: advanced-ranges.md 5 - a snapshot carries values, not the
    mode. Captured at gain 1.8 with amp unlocked, recalled after locking, it
    clamps to the stock top (1.0) rather than re-mapping, and amp stays locked. */
LUTHIER_TEST (Ranges, snapshotsDoNotCarryMode)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    processor.getSnapshots().setCrossfadeMs (0.0);

    processor.setRanges (ampUnlocked());
    auto* gain = gainOf (processor);
    CHECK (gain != nullptr);
    if (gain == nullptr) return;

    *gain = 1.8f;
    CHECK_NEAR (gain->get(), 1.8f, 1.0e-4f);
    CHECK (processor.captureSnapshot (0, "Loud"));

    processor.setRanges (RangeState());
    CHECK_NEAR (gain->get(), 1.0f, 1.0e-4f);   // the narrowing clamped it

    *gain = 0.2f;
    CHECK (processor.recallSnapshot (0));

    CHECK_MSG (std::abs (gain->get() - 1.0f) < 1.0e-4f,
               "recall should clamp 1.8 to the stock 1.0, got " + juce::String (gain->get()));
    CHECK (! processor.getRanges().isFamilyAdvanced (RangeFamily::amp));

    // Unlocked again, the same snapshot brings back its 1.8.
    processor.setRanges (ampUnlocked());
    CHECK (processor.recallSnapshot (0));
    CHECK_NEAR (gain->get(), 1.8f, 1.0e-3f);
}

//==============================================================================
/*  AR-24: resetting to defaults writes the plain default against whatever
    range is live, and does not change the range mode. */
LUTHIER_TEST (Ranges, resetToDefaultsKeepsTheModeAndThePlainDefault)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    processor.setRanges (ampUnlocked());

    auto* gain = gainOf (processor);
    CHECK (gain != nullptr);
    if (gain == nullptr) return;

    *gain = 1.6f;
    processor.getPresetManager().resetToDefaults();

    CHECK (processor.getRanges().isFamilyAdvanced (RangeFamily::amp));
    CHECK_MSG (std::abs (gain->get() - RangeRegistry::find (ParamIDs::ampGain)->defaultValue) < 1.0e-3f,
               "reset left amp_gain at " + juce::String (gain->get()));
}

//==============================================================================
/*  AR-11 / AR-12: automation and modulation sweep the live range. Host
    automation at normalised 1.0 is the stock top while locked and the advanced
    top once the family is unlocked; a full-depth mod route does the same. */
LUTHIER_TEST (Ranges, automationAndModulationFollowTheLiveRange)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const auto* range = RangeRegistry::find (ParamIDs::ampGain);
    auto* gain = gainOf (processor);
    CHECK (range != nullptr && gain != nullptr);
    if (range == nullptr || gain == nullptr) return;

    gain->setValueNotifyingHost (1.0f);
    CHECK_NEAR (gain->get(), range->stockMax, 1.0e-4f);

    processor.setRanges (ampUnlocked());
    CHECK_NEAR (gain->get(), range->stockMax, 1.0e-4f);   // widening keeps the plain value

    gain->setValueNotifyingHost (1.0f);
    CHECK_NEAR (gain->get(), range->advancedMax, 1.0e-4f);

    // Modulation: a macro at full depth pushes to the top of the live range.
    auto modulatedTop = [&processor, gain] (float base)
    {
        gain->setValueNotifyingHost (gain->convertTo0to1 (base));

        const auto& params = processor.getParameters();
        int index = -1;
        for (int i = 0; i < params.size(); ++i)
            if (params[i] == gain) index = i;

        auto& m = processor.getModMatrix();
        m.clearRoutes();

        ModRoute route;
        route.sourceId = "macro1";
        route.destinationId = ParamIDs::ampGain;
        route.depth = 1.0f;
        m.addRoute (route);
        m.setMacroValue (0, 1.0);

        ModBlockContext context;
        for (int i = 0; i < 64; ++i)
            m.processBlock (kBlock, context);

        return m.apply (index, base);
    };

    CHECK_NEAR (modulatedTop (0.5f), range->advancedMax, 0.05f);

    processor.setRanges (RangeState());
    CHECK_NEAR (modulatedTop (0.5f), range->stockMax, 0.05f);
}

//==============================================================================
/*  AR-22: advanced-ranges.md 5 - each A/B slot carries its own ranges: A
    stock, B with amp unlocked and gain 1.8. Switching restores each slot's
    mode and value. */
LUTHIER_TEST (Ranges, abSlotsCarryTheirOwnRanges)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    auto* gain = gainOf (processor);
    CHECK (gain != nullptr);
    if (gain == nullptr) return;

    *gain = 0.4f;
    processor.storeToSlot (false);

    processor.setSlotBActive (true);
    processor.setRanges (ampUnlocked());
    *gain = 1.8f;

    processor.setSlotBActive (false);   // stores B, recalls A
    CHECK (! processor.getRanges().isFamilyAdvanced (RangeFamily::amp));
    CHECK_NEAR (gain->get(), 0.4f, 1.0e-3f);

    processor.setSlotBActive (true);
    CHECK (processor.getRanges().isFamilyAdvanced (RangeFamily::amp));
    CHECK_NEAR (gain->get(), 1.8f, 1.0e-3f);
}

//==============================================================================
/*  AR-T2: advanced-ranges.md 10 - widening is silent: the same state and MIDI
    render the same before and after every family is unlocked. Not bit for
    bit: re-deriving a normalised value against the wider range moves some
    plain values by a float ULP, which reaches the output at around -140 dBFS.
    The bound is -120 dBFS (sweep-notes/dsp1.md). */
LUTHIER_TEST (Ranges, wideningRendersIdentically)
{
    auto render = [] (bool widen)
    {
        LuthierAudioProcessor processor;
        processor.prepareToPlay (kSr, kBlock);

        if (auto* gain = gainOf (processor))
            *gain = 0.7f;

        if (widen)
            for (int f = 0; f < (int) RangeFamily::numFamilies; ++f)
            {
                RangeState s = processor.getRanges();
                s.setFamilyAdvanced ((RangeFamily) f, true);
                processor.setRanges (s);
            }

        processor.getParameterBridge().applyAllNow();
        processor.getEngine().reset();

        std::vector<float> out;
        for (int b = 0; b < 20; ++b)
        {
            juce::AudioBuffer<float> buffer (processor.getTotalNumOutputChannels(), kBlock);
            buffer.clear();
            juce::MidiBuffer midi;
            if (b == 1)
                midi.addEvent (juce::MidiMessage::noteOn (1, 52, 0.8f), 0);
            processor.processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + kBlock);
        }

        return out;
    };

    const auto stock = render (false);
    const auto wide = render (true);

    double worst = 0.0, peak = 0.0;
    for (size_t i = 0; i < stock.size(); ++i)
    {
        worst = juce::jmax (worst, (double) std::abs (stock[i] - wide[i]));
        peak = juce::jmax (peak, (double) std::abs (stock[i]));
    }

    CHECK (peak > 1.0e-3);
    CHECK_MSG (worst < 1.0e-6, "unlocking every family moved the render by " + juce::String (gainToDb (worst), 1) + " dBFS");
}
