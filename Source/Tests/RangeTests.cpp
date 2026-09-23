/*  Stock and advanced parameter ranges (advanced-ranges.md 10).

    The mechanism is small and the things that can go wrong with it are
    subtle, so these tests are mostly about the two transitions - widening
    must be inaudible, narrowing must clamp exactly once and say so - and
    about the invariant sweep that stops a later realism spec adding a
    malformed pair.
*/

#include "TestFramework.h"

#include "../PhysicalRange.h"
#include "../PluginProcessor.h"
#include "../Parameters.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    juce::AudioParameterFloat* floatParam (LuthierAudioProcessor& p, const juce::String& id)
    {
        return dynamic_cast<juce::AudioParameterFloat*> (p.getState().getParameter (id));
    }
}

//==============================================================================
/*  Every PhysicalRange in the build satisfies its invariants.

    A sweep rather than a spot check, because the registry is meant to grow:
    each later realism spec adds rows, and a bad pair should fail here rather
    than as a strange sound six modules later. */
LUTHIER_TEST (Ranges, everyPhysicalRangeIsValid)
{
    const auto ids = RangeRegistry::allIds();

    CHECK_MSG (! ids.isEmpty(), "the range registry is empty");

    for (const auto& id : ids)
    {
        const auto* range = RangeRegistry::find (id);

        CHECK_MSG (range != nullptr, id + " is listed but cannot be found");

        if (range == nullptr)
            continue;

        CHECK_MSG (range->isValid(),
                   id + " has an invalid range pair: stock "
                     + juce::String (range->stockMin) + ".." + juce::String (range->stockMax)
                     + ", advanced " + juce::String (range->advancedMin) + ".."
                     + juce::String (range->advancedMax)
                     + ", default " + juce::String (range->defaultValue));

        CHECK_MSG (range->family != RangeFamily::none,
                   id + " has a PhysicalRange but no family");
    }
}

//==============================================================================
/*  advanced-ranges.md 1.0: stock is the range the parameter already ships
    with.

    This is the one that protects every saved preset. Presets store normalised
    values, so if a registry entry's stock pair disagreed with the declared
    range, every preset ever saved would silently re-map - a 0.8 that meant
    12.1 m would start meaning 8.2 m, with no error anywhere. */
LUTHIER_TEST (Ranges, stockMatchesTheDeclaredRange)
{
    // Constructing the processor builds the layout, which records what each
    // float parameter was actually declared with.
    LuthierAudioProcessor processor;

    const auto mismatches = RangeRegistry::findDeclarationMismatches();

    for (const auto& mismatch : mismatches)
        CHECK_MSG (false, mismatch + " - every saved preset would re-map");

    CHECK_MSG (mismatches.isEmpty(),
               juce::String (mismatches.size())
                 + " registered stock ranges disagree with their declarations");

    /*  And the check is not vacuous. An earlier version compared the
        parameter's live range against the registry - but `floatParam` had
        copied that range *from* the registry, so it compared the registry with
        itself and passed with a deliberately wrong stock pair. The declaration
        is recorded at the call site now and is the source of truth. */
    CHECK_MSG (! RangeRegistry::allIds().isEmpty(), "the registry is empty");

    for (const auto& id : RangeRegistry::allIds())
        CHECK_MSG (floatParam (processor, id) != nullptr,
                   id + " is registered but is not a float parameter in the layout");
}

//==============================================================================
/*  Widening is silent (advanced-ranges.md 1.2).

    The whole point of switching to advanced being free: the range grows, the
    normalised value moves, and the plain value - which is what is audible -
    does not. */
LUTHIER_TEST (Ranges, wideningPreservesEveryPlainValue)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    for (int i = 0; i < (int) RangeFamily::numFamilies; ++i)
    {
        const auto family = (RangeFamily) i;
        const auto ids = RangeRegistry::idsInFamily (family);

        if (ids.isEmpty())
            continue;   // a family with no members is legal and reads as stock

        for (double position : { 0.0, 0.5, 1.0 })
        {
            RangeState stock;
            stock.applyTo (processor.getState());

            // Put every member of the family at a known point of its stock range.
            for (const auto& id : ids)
                if (auto* p = floatParam (processor, id))
                    p->setValueNotifyingHost ((float) position);

            juce::Array<float> before;

            for (const auto& id : ids)
                if (auto* p = floatParam (processor, id))
                    before.add (p->get());

            RangeState advanced;
            advanced.setFamilyAdvanced (family, true);

            const int clamped = advanced.applyTo (processor.getState());

            CHECK_MSG (clamped == 0,
                       juce::String (getRangeFamilyName (family))
                         + ": widening clamped " + juce::String (clamped) + " values");

            for (int n = 0; n < ids.size(); ++n)
            {
                auto* p = floatParam (processor, ids[n]);

                if (p == nullptr)
                    continue;

                // Relative: a 1 M resistor stored as a float is only good to
                // about 0.06 ohm, which is still one part in ten million.
                CHECK_MSG (std::abs (p->get() - before[n]) <= 1.0e-4f * juce::jmax (1.0f, std::abs (before[n])),
                           ids[n] + " moved when its family was unlocked: "
                             + juce::String (before[n]) + " -> " + juce::String (p->get()));
            }
        }
    }

    RangeState().applyTo (processor.getState());
}

//==============================================================================
/*  Narrowing clamps, exactly once, and reports how many (advanced-ranges.md
    1.3). A silent clamp here is the failure ground rule 0.2 exists to
    prevent. */
LUTHIER_TEST (Ranges, narrowingClampsAndReportsTheCount)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const juce::String id (ParamIDs::ampGain);

    const auto* range = RangeRegistry::find (id);

    CHECK_MSG (range != nullptr, "amp_gain has no PhysicalRange");

    if (range == nullptr)
        return;

    // Unlock, then go past what stock allows.
    RangeState advanced;
    advanced.setFamilyAdvanced (RangeFamily::amp, true);
    advanced.applyTo (processor.getState());

    auto* parameter = floatParam (processor, id);

    CHECK (parameter != nullptr);

    if (parameter == nullptr)
        return;

    const float beyond = range->stockMax + (range->advancedMax - range->stockMax) * 0.5f;
    parameter->setValueNotifyingHost (parameter->range.convertTo0to1 (beyond));

    CHECK_MSG (parameter->get() > range->stockMax,
               "could not set a value outside stock while unlocked");

    CHECK_MSG (range->isOutsideStock (parameter->get()),
               "a value past stockMax does not report as outside stock");

    // Lock it back.
    RangeState stock;
    const int clamped = stock.applyTo (processor.getState());

    CHECK_MSG (clamped == 1,
               "locking reported " + juce::String (clamped)
                 + " clamped values, expected exactly 1");

    CHECK_MSG (std::abs (parameter->get() - range->stockMax) < 1.0e-4f,
               "the clamped value is " + juce::String (parameter->get())
                 + ", expected stockMax " + juce::String (range->stockMax));

    // And clamping again is a no-op rather than a second report.
    CHECK_MSG (stock.applyTo (processor.getState()) == 0,
               "re-applying stock reported another clamp");
}

//==============================================================================
/*  Normalisation follows the live range (advanced-ranges.md 1.1).

    A host automation lane at 1.0 means "this parameter's maximum", and the
    maximum moves when the range does. The plain value written before the
    switch must not. */
LUTHIER_TEST (Ranges, normalisationFollowsTheLiveRange)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const juce::String id (ParamIDs::ampMaster);
    const auto* range = RangeRegistry::find (id);

    RangeState stock;
    stock.applyTo (processor.getState());

    auto* parameter = floatParam (processor, id);

    CHECK (parameter != nullptr && range != nullptr);

    if (parameter == nullptr || range == nullptr)
        return;

    parameter->setValueNotifyingHost (1.0f);

    CHECK_MSG (std::abs (parameter->get() - range->stockMax) < 1.0e-4f,
               "normalised 1.0 in stock mode is not stockMax");

    // Half the stock range, which must survive the widening.
    const float midStock = (range->stockMin + range->stockMax) * 0.5f;
    parameter->setValueNotifyingHost (parameter->range.convertTo0to1 (midStock));

    RangeState advanced;
    advanced.setFamilyAdvanced (RangeFamily::amp, true);
    advanced.applyTo (processor.getState());

    CHECK_MSG (std::abs (parameter->get() - midStock) < 1.0e-4f,
               "the plain value moved when the range widened");

    parameter->setValueNotifyingHost (1.0f);

    CHECK_MSG (std::abs (parameter->get() - range->advancedMax) < 1.0e-4f,
               "normalised 1.0 in advanced mode is not advancedMax");

    RangeState().applyTo (processor.getState());
}

//==============================================================================
/*  The `ranges` block round-trips, and a missing one derives per family
    (advanced-ranges.md 4, 4.1). */
LUTHIER_TEST (Ranges, theRangesBlockRoundTripsAndDerivesWhenAbsent)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    //--------------------------------------------------------------------------
    // Round trip.
    {
        RangeState written;
        written.setFamilyAdvanced (RangeFamily::circuit, true);
        written.setUnlockedIndividually (ParamIDs::ampGain, true);

        RangeState read;
        read.fromVar (written.toVar(), {});

        CHECK_MSG (read.isFamilyAdvanced (RangeFamily::circuit),
                   "the circuit family did not survive the round trip");

        CHECK_MSG (! read.isFamilyAdvanced (RangeFamily::amp),
                   "a family that was stock came back advanced");

        CHECK_MSG (read.isUnlockedIndividually (ParamIDs::ampGain),
                   "the per-control unlock did not survive the round trip");

        CHECK_MSG (read.isParameterAdvanced (ParamIDs::ampGain),
                   "an individually unlocked control does not report as advanced");

        CHECK_MSG (read.isParameterAdvanced (ParamIDs::cableLength),
                   "a control in an advanced family does not report as advanced");
    }

    //--------------------------------------------------------------------------
    /*  Unlocking a family drops redundant per-control unlocks inside it, so
        locking the family later cannot leave stragglers behind. */
    {
        RangeState state;
        state.setUnlockedIndividually (ParamIDs::ampGain, true);
        CHECK (state.isUnlockedIndividually (ParamIDs::ampGain));

        state.setFamilyAdvanced (RangeFamily::amp, true);

        CHECK_MSG (! state.isUnlockedIndividually (ParamIDs::ampGain),
                   "unlocking a family left a redundant per-control unlock behind");

        state.setFamilyAdvanced (RangeFamily::amp, false);

        CHECK_MSG (! state.isParameterAdvanced (ParamIDs::ampGain),
                   "locking the family left the control advanced");
    }

    //--------------------------------------------------------------------------
    /*  An ordinary legacy preset - every value inside stock - derives as all
        stock. Because of 1.0 this is every preset written before this landed. */
    {
        RangeState stock;
        stock.applyTo (processor.getState());

        RangeState derived;
        derived.deriveFromCurrentValues (processor.getState());

        CHECK_MSG (! derived.isAnythingAdvanced(),
                   "a preset with every value inside stock derived as advanced");
    }

    //--------------------------------------------------------------------------
    // One value outside stock derives that family, and only that family.
    {
        RangeState advanced;
        advanced.setFamilyAdvanced (RangeFamily::amp, true);
        advanced.applyTo (processor.getState());

        const auto* range = RangeRegistry::find (ParamIDs::ampGain);
        auto* parameter = floatParam (processor, ParamIDs::ampGain);

        if (parameter != nullptr && range != nullptr)
        {
            parameter->setValueNotifyingHost (1.0f);   // advancedMax

            RangeState derived;
            derived.deriveFromCurrentValues (processor.getState());

            CHECK_MSG (derived.isFamilyAdvanced (RangeFamily::amp),
                       "a value outside stock did not derive its family as advanced");

            CHECK_MSG (! derived.isFamilyAdvanced (RangeFamily::circuit),
                       "an untouched family derived as advanced");
        }
    }

    RangeState().applyTo (processor.getState());
}

//==============================================================================
/*  Marking follows the value, not the mode (advanced-ranges.md 6.1).

    A family that is unlocked but whose values all sit inside stock shows no
    warning arcs: there is nothing unusual about the sound, so nothing is
    marked. */
LUTHIER_TEST (Ranges, markingFollowsTheValueNotTheMode)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    RangeState advanced;
    advanced.setFamilyAdvanced (RangeFamily::amp, true);
    advanced.applyTo (processor.getState());

    const auto* range = RangeRegistry::find (ParamIDs::ampGain);
    auto* parameter = floatParam (processor, ParamIDs::ampGain);

    CHECK (range != nullptr && parameter != nullptr);

    if (range == nullptr || parameter == nullptr)
        return;

    // Unlocked, but sitting inside stock: not marked.
    const float insideStock = (range->stockMin + range->stockMax) * 0.5f;
    parameter->setValueNotifyingHost (parameter->range.convertTo0to1 (insideStock));

    CHECK_MSG (! range->isOutsideStock (parameter->get()),
               "a value inside stock is marked merely because its family is unlocked");

    CHECK_MSG (advanced.findValuesOutsideStock (processor.getState()).isEmpty(),
               "the out-of-stock summary lists a value that is inside stock");

    // Moved past stock: marked, and listed in the summary.
    parameter->setValueNotifyingHost (1.0f);

    CHECK_MSG (range->isOutsideStock (parameter->get()),
               "a value past stockMax is not marked");

    CHECK_MSG (advanced.findValuesOutsideStock (processor.getState())
                 .contains (ParamIDs::ampGain),
               "the out-of-stock summary omits a value past stockMax");

    RangeState().applyTo (processor.getState());
}
