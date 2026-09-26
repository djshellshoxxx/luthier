/*  SPEC-SWEEP: advanced-ranges.md tests the audit found missing (AR-11, AR-12,
    AR-24, AR-T10 with the AR-21 fix). */

#include "TestFramework.h"

#include "../Parameters.h"
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
