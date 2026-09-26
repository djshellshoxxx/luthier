/*  SPEC-SWEEP: routing-io.md sidechain tests the audit found missing (RIO-4,
    RIO-13, RIO-14), run through the real processor so the sidechain arrives the
    way a host delivers it. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Routing/RoutingMatrix.h"
#include "../Parameters.h"
#include "../DSP/Effects/Pedal.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    /** Enables the sidechain input, and the aux pairs when asked (layout B). */
    void enableSidechain (LuthierAudioProcessor& processor, bool withAux)
    {
        auto layout = processor.getBusesLayout();
        layout.inputBuses.getReference (0) = juce::AudioChannelSet::stereo();

        if (withAux)
            for (int bus = 1; bus <= kNumAuxBuses; ++bus)
                layout.outputBuses.getReference (bus) = juce::AudioChannelSet::stereo();

        processor.setBusesLayout (layout);
        processor.prepareToPlay (kSr, kBlock);
    }

    /** One block with `amplitude` of a sine on the sidechain; returns the left
        main output. */
    std::vector<float> runBlock (LuthierAudioProcessor& processor, double amplitude, int blockIndex)
    {
        juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumInputChannels(),
                                                     processor.getTotalNumOutputChannels()), kBlock);
        buffer.clear();

        auto sidechain = processor.getBusBuffer (buffer, true, 0);

        for (int ch = 0; ch < sidechain.getNumChannels(); ++ch)
            for (int i = 0; i < kBlock; ++i)
                sidechain.setSample (ch, i, (float) (amplitude * std::sin (2.0 * juce::MathConstants<double>::pi
                                                                        * 220.0 * (blockIndex * kBlock + i) / kSr)));

        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);

        auto main = processor.getBusBuffer (buffer, false, 0);
        std::vector<float> out ((size_t) kBlock);

        for (int i = 0; i < kBlock; ++i)
            out[(size_t) i] = main.getSample (0, i);

        return out;
    }
}

//==============================================================================
/*  RIO-14: the sidechain meter reads the sidechain, and 0 without one. */
LUTHIER_TEST (Routing, sidechainMeterReadsTheSidechain)
{
    {
        LuthierAudioProcessor processor;
        processor.prepareToPlay (kSr, kBlock);
        CHECK (! processor.hasSidechainInput());

        for (int b = 0; b < 4; ++b)
            runBlock (processor, 0.5, b);

        CHECK (processor.getRouting().getSidechainLevel() == 0.0);
    }

    LuthierAudioProcessor processor;
    enableSidechain (processor, false);
    CHECK (processor.hasSidechainInput());

    for (int b = 0; b < 8; ++b)
        runBlock (processor, 0.5, b);   // -6 dBFS

    CHECK_NEAR (processor.getRouting().getSidechainLevel(), 0.5, 0.01);
}

//==============================================================================
/*  RIO-4: a follower pointed at the sidechain follows it, and falls when it
    stops. */
LUTHIER_TEST (Routing, sidechainDrivesTheEnvelopeFollower)
{
    LuthierAudioProcessor processor;
    enableSidechain (processor, false);

    auto& follower = processor.getModMatrix().getFollower (0);
    follower.setSource (ModEnvelopeFollower::Source::sidechain);
    follower.setAttackMs (5.0);
    follower.setReleaseMs (50.0);

    for (int b = 0; b < 20; ++b)
        runBlock (processor, 0.5, b);

    const double driven = processor.getModMatrix().getSourceValue (ModSourceSlots::followerBase);
    CHECK_MSG (driven > 0.3, "the follower read " + juce::String (driven, 3) + " from a 0.5 sidechain");

    for (int b = 0; b < 60; ++b)
        runBlock (processor, 0.0, b);

    const double released = processor.getModMatrix().getSourceValue (ModSourceSlots::followerBase);
    CHECK_MSG (released < 0.05, "the follower stayed at " + juce::String (released, 3) + " after the sidechain stopped");
}

//==============================================================================
/*  RIO-13: with sidechain-to-amp off, the sidechain never reaches the main out:
    a loud sidechain moves the main output by less than -120 dBFS. */
LUTHIER_TEST (Routing, sidechainNeverReachesTheMainOut)
{
    auto render = [] (double amplitude)
    {
        LuthierAudioProcessor processor;
        enableSidechain (processor, true);
        processor.getRouting().setSidechainToAmp (false);

        std::vector<float> out;

        for (int b = 0; b < 20; ++b)
        {
            const auto block = runBlock (processor, amplitude, b);
            out.insert (out.end(), block.begin(), block.end());
        }

        return out;
    };

    const auto quiet = render (0.0);
    const auto loud = render (0.9);

    double worst = 0.0;
    for (size_t i = 0; i < quiet.size(); ++i)
        worst = juce::jmax (worst, std::abs ((double) loud[i] - (double) quiet[i]));

    CHECK_MSG (worst < std::pow (10.0, -120.0 / 20.0),
               "the sidechain leaked into the main output: "
               + juce::String (20.0 * std::log10 (juce::jmax (1.0e-30, worst)), 1) + " dBFS");
}

//==============================================================================
/*  RIO-6: in layout B every aux carries its own signal: the amp before the
    cab, each mic, the room, the wet tails and the monitor all sound, the two
    mics differ, the room goes silent with the room off and the wet bus with no
    wet pedal. */
LUTHIER_TEST (Routing, everyAuxTapCarriesItsOwnSignal)
{
    auto setParam = [] (LuthierAudioProcessor& p, const juce::String& id, float plain)
    {
        if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id)))
            prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
    };

    auto run = [&] (bool roomOn, bool reverb)
    {
        LuthierAudioProcessor processor;
        enableSidechain (processor, true);

        setParam (processor, ParamIDs::dualMic, 1.0f);
        setParam (processor, ParamIDs::roomOn, roomOn ? 1.0f : 0.0f);
        setParam (processor, ParamIDs::slotType (true, 0), reverb ? (float) (int) PedalType::Reverb : 0.0f);
        processor.getParameterBridge().applyAllNow();   // the pedal type is structural

        std::array<double, kNumAuxBuses> energy {};
        std::array<std::vector<float>, kNumAuxBuses> left;

        for (int b = 0; b < 60; ++b)
        {
            juce::AudioBuffer<float> buffer (processor.getTotalNumOutputChannels(), kBlock);
            buffer.clear();

            juce::MidiBuffer midi;
            if (b == 2)
                for (int note : { 40, 47, 52 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);

            processor.processBlock (buffer, midi);

            for (int bus = 0; bus < kNumAuxBuses; ++bus)
            {
                auto aux = processor.getBusBuffer (buffer, false, 1 + bus);

                for (int i = 0; i < kBlock; ++i)
                {
                    const float v = aux.getSample (0, i);
                    energy[(size_t) bus] += (double) v * v;
                    left[(size_t) bus].push_back (v);
                }
            }
        }

        return std::make_pair (energy, left);
    };

    const auto [all, allLeft] = run (true, true);

    for (auto bus : { AuxBus::ampPreCab, AuxBus::cabMic1, AuxBus::cabMic2, AuxBus::roomMic, AuxBus::wetFx })
        CHECK_MSG (all[(size_t) bus] > 1.0e-6, juce::String (getAuxBusName ((int) bus)) + " is silent");

    double micDiff = 0.0;
    for (size_t i = 0; i < allLeft[(size_t) AuxBus::cabMic1].size(); ++i)
        micDiff += std::abs ((double) allLeft[(size_t) AuxBus::cabMic1][i] - allLeft[(size_t) AuxBus::cabMic2][i]);
    CHECK_MSG (micDiff > 1.0e-3, "the two mic taps carry the same signal");

    const auto [dry, dryLeft] = run (false, false);
    juce::ignoreUnused (dryLeft);
    CHECK_MSG (dry[(size_t) AuxBus::roomMic] < 1.0e-12, "the room tap sounds with the room off");
    CHECK_MSG (dry[(size_t) AuxBus::wetFx] < 1.0e-12, "the wet tap sounds with no wet pedal");
}
