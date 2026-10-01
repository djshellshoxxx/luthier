/*  The Doubler pedal (ambiguity-resolutions.md 3, tests 3.2). */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../DSP/Effects/PedalsMod.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    enum { kDelay = 0, kPitch, kPan, kWidth, kMix, kHp, kLp };

    /** A second of broadband signal, the same every run. */
    std::vector<double> noise (int n)
    {
        std::vector<double> v ((size_t) n);
        juce::Random random (0x0d0b1e);

        for (auto& x : v)
            x = random.nextDouble() * 0.5 - 0.25;

        return v;
    }

    /** Runs a mono signal through the pedal in blocks; returns left and right. */
    std::pair<std::vector<double>, std::vector<double>> run (DoublerPedal& pedal, const std::vector<double>& in)
    {
        std::vector<double> l (in), r (in);

        for (size_t at = 0; at < in.size(); at += kBlock)
        {
            const int n = (int) juce::jmin ((size_t) kBlock, in.size() - at);
            pedal.processWithBypass (l.data() + at, r.data() + at, n);
        }

        return { l, r };
    }
}

//==============================================================================
LUTHIER_TEST (Doubler, mixZeroIsTheDrySignal)
{
    DoublerPedal pedal;
    pedal.prepare (kSr, kBlock);
    pedal.setParameterValue (kMix, 0.0);

    const auto in = noise ((int) kSr);
    const auto [l, r] = run (pedal, in);

    double error = 0.0, signal = 0.0;

    for (size_t i = 0; i < in.size(); ++i)
    {
        error += (l[i] - in[i]) * (l[i] - in[i]) + (r[i] - in[i]) * (r[i] - in[i]);
        signal += 2.0 * in[i] * in[i];
    }

    const double nullDb = juce::Decibels::gainToDecibels (std::sqrt (error / (2.0 * (double) in.size())), -400.0);
    CHECK_MSG (nullDb < -80.0, "mix 0 nulls at only " + juce::String (nullDb, 1) + " dBFS");
}

LUTHIER_TEST (Doubler, mixFullIsACopyTwentyTwoMillisecondsLate)
{
    DoublerPedal pedal;
    pedal.prepare (kSr, kBlock);
    pedal.setParameterValue (kMix, 100.0);
    pedal.setParameterValue (kDelay, 22.0);
    pedal.setParameterValue (kPitch, 0.0);
    pedal.setParameterValue (kWidth, 0.0);   // one take
    pedal.setParameterValue (kPan, 0.0);
    pedal.setParameterValue (kHp, 20.0);
    pedal.setParameterValue (kLp, 20000.0);

    const auto in = noise ((int) kSr);
    const auto [l, r] = run (pedal, in);

    // Cross-correlate the output with the input over 0..40 ms.
    int best = -1;
    double bestValue = 0.0;

    for (int lag = 0; lag < (int) (0.040 * kSr); ++lag)
    {
        double c = 0.0;

        for (size_t i = (size_t) lag; i < in.size(); ++i)
            c += (l[i] + r[i]) * in[i - (size_t) lag];

        if (c > bestValue)
        {
            bestValue = c;
            best = lag;
        }
    }

    const int expected = (int) std::round (0.022 * kSr);
    CHECK_MSG (std::abs (best - expected) <= 1,
               "the copy correlates at " + juce::String (best) + " samples, not " + juce::String (expected));
}

LUTHIER_TEST (Doubler, panPutsTheTakesToTheSides)
{
    auto sides = [] (bool stereo)
    {
        DoublerPedal pedal;
        pedal.prepare (kSr, kBlock);
        pedal.setParameterValue (kMix, 100.0);
        pedal.setParameterValue (kWidth, stereo ? 1.0 : 0.0);
        pedal.setParameterValue (kPan, -0.7);

        const auto [l, r] = run (pedal, noise ((int) kSr));
        double el = 0.0, er = 0.0;

        for (size_t i = 0; i < l.size(); ++i)
        {
            el += l[i] * l[i];
            er += r[i] * r[i];
        }

        return juce::Decibels::gainToDecibels (std::sqrt (el / er));
    };

    CHECK_MSG (sides (false) > 6.0, "a mono take panned -0.7 is not on the left: L/R "
                                        + juce::String (sides (false), 1) + " dB");
    CHECK_MSG (std::abs (sides (true)) < 1.0, "stereo's mirrored takes are not balanced: L/R "
                                                + juce::String (sides (true), 1) + " dB");
}

LUTHIER_TEST (Doubler, isAPostAmpRackPedal)
{
    CHECK (Pedal::isPostAmpPedal (PedalType::Doubler));
    CHECK (! Pedal::isPreAmpPedal (PedalType::Doubler));
    CHECK (Parameters::pedalTypeNames().contains ("Doubler"));

    // Appended: every pedal saved before it keeps its index.
    CHECK ((int) PedalType::Doubler == (int) PedalType::ParametricEQ + 1);
}

/*  B-07: the migration is for files from before the pedal. A file this build
    saved says so, and a session that has doubler_on automated on is left alone
    rather than growing a pedal on every load. */
LUTHIER_TEST (Doubler, theLegacyDoublerMigratesOnceNotOnEveryLoad)
{
    LuthierAudioProcessor processor;
    auto state = processor.getPresetManager().toVar();

    CHECK (state.getProperty ("doublerMigrated", false));

    if (auto* params = state.getProperty ("parameters", {}).getDynamicObject())
        params->setProperty (ParamIDs::doublerOn, 1.0);

    CHECK (processor.getPresetManager().fromVar (state));

    for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
        if (auto* type = processor.getState().getParameter (ParamIDs::slotType (true, slot)))
            CHECK (juce::roundToInt (type->convertFrom0to1 (type->getValue())) != (int) PedalType::Doubler);
}

LUTHIER_TEST (Doubler, presetsWithTheOldDoublerGetThePedal)
{
    LuthierAudioProcessor processor;
    auto state = processor.getPresetManager().toVar();

    if (auto* params = state.getProperty ("parameters", {}).getDynamicObject())
        params->setProperty (ParamIDs::doublerOn, 1.0);

    // A file from before the pedal has no migration marker (B-07: every save since has it).
    if (auto* root = state.getDynamicObject())
        root->removeProperty ("doublerMigrated");

    CHECK (processor.getPresetManager().fromVar (state));

    bool found = false;

    for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
        if (auto* type = processor.getState().getParameter (ParamIDs::slotType (true, slot)))
            found = found || juce::roundToInt (type->convertFrom0to1 (type->getValue())) == (int) PedalType::Doubler;

    CHECK_MSG (found, "no Doubler pedal replaced the old doubler");

    auto* old = processor.getState().getParameter (ParamIDs::doublerOn);
    CHECK (old != nullptr && old->getValue() < 0.5f);
}
