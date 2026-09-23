/*  The plugin's own output buses, as a host negotiates them (routing-io.md 1-3,
    pick-noise.md 1.3). RoutingTests drives the matrix through a harness that
    declares only one layout's buses; this drives the real processor, which
    declares every bus and has the host disable the ones it does not want. */

#include "TestFramework.h"

#include "../PluginProcessor.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    int noiseBusIndex() { return 1 + kNumAuxBuses + kNumPerStringBuses; }
    int stringBusIndex (int s) { return 1 + kNumAuxBuses + s; }

    /** Enables exactly the buses asked for, as a host would. */
    bool enable (LuthierAudioProcessor& processor, bool aux, bool perString, bool noise)
    {
        auto layout = processor.getBusesLayout();

        for (int bus = 1; bus < layout.outputBuses.size(); ++bus)
        {
            const bool isAux = bus - 1 < kNumAuxBuses;
            const bool isString = ! isAux && bus < noiseBusIndex();
            const bool on = isAux ? aux : (isString ? perString : noise);

            layout.outputBuses.getReference (bus) = ! on ? juce::AudioChannelSet::disabled()
                                                  : isString ? juce::AudioChannelSet::mono()
                                                             : juce::AudioChannelSet::stereo();
        }

        return processor.setBusesLayout (layout);
    }

    /** Plays a low E and returns the peak of each output bus's first channel. */
    juce::Array<float> play (LuthierAudioProcessor& processor)
    {
        processor.prepareToPlay (kSr, kBlock);

        juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(),
                                                     processor.getTotalNumInputChannels()), kBlock);
        juce::Array<float> peaks;
        peaks.insertMultiple (0, 0.0f, processor.getBusCount (false));

        for (int block = 0; block < 24; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 115), 0);

            processor.processBlock (buffer, midi);

            for (int bus = 0; bus < processor.getBusCount (false); ++bus)
            {
                auto out = processor.getBusBuffer (buffer, false, bus);

                if (out.getNumChannels() > 0)
                    peaks.set (bus, juce::jmax (peaks[bus], out.getMagnitude (0, 0, kBlock)));
            }
        }

        return peaks;
    }
}

//==============================================================================
LUTHIER_TEST (PluginBuses, aux8NoiseIsDeclaredLastSoNoBusNumberMoved)
{
    LuthierAudioProcessor processor;

    CHECK (processor.getBusCount (false) == 2 + kNumAuxBuses + kNumPerStringBuses);
    CHECK (processor.getBus (false, 1)->getName() == getAuxBusName (0));
    CHECK (processor.getBus (false, stringBusIndex (0))->getName() == "String 1");
    CHECK (processor.getBus (false, noiseBusIndex())->getName() == getAuxBusName (kNoiseAux));

    CHECK_MSG (enable (processor, false, false, true), "a host could not enable Aux 8 on its own");

    auto layout = processor.getBusesLayout();
    layout.outputBuses.getReference (noiseBusIndex()) = juce::AudioChannelSet::mono();
    CHECK_MSG (! processor.checkBusesLayoutSupported (layout), "Aux 8 is a stereo pair, like every aux");
}

LUTHIER_TEST (PluginBuses, perStringLayoutPutsEachStringOnItsOwnBus)
{
    // Layout C: the per-string buses on, every aux off. The low E (string 5 in
    // engine order) has to come out of "String 6", not seven buses further on.
    LuthierAudioProcessor processor;

    if (auto* p = processor.getState().getParameter (ParamIDs::macroHumanize))
        p->setValueNotifyingHost (0.0f);

    CHECK (enable (processor, false, true, false));
    const auto peaks = play (processor);

    CHECK_MSG (peaks[stringBusIndex (5)] > 1.0e-4f,
               "the low E's bus is silent: " + juce::String (peaks[stringBusIndex (5)]));

    for (int s = 0; s < 5; ++s)
        CHECK_MSG (peaks[stringBusIndex (s)] < peaks[stringBusIndex (5)] * 0.5f,
                   "String " + juce::String (s + 1) + " carries as much as the string that was played");
}

LUTHIER_TEST (PluginBuses, aux8CarriesThePlayingNoiseAndObeysItsStrip)
{
    LuthierAudioProcessor processor;

    if (auto* p = processor.getState().getParameter (ParamIDs::pickNoise))
        p->setValueNotifyingHost (1.0f);

    CHECK (enable (processor, true, false, true));
    CHECK (processor.getRouting().getActiveLayout() == BusLayout::studio
           || processor.getNegotiatedLayout() == BusLayout::studio);

    auto peaks = play (processor);
    CHECK_MSG (peaks[noiseBusIndex()] > 1.0e-6f, "Aux 8 is silent under a picked note");

    processor.getRouting().setAuxMuted (kNoiseAux, true);
    peaks = play (processor);
    CHECK_MSG (peaks[noiseBusIndex()] < 1.0e-6f, "a muted Aux 8 still carries the noise");

    // The strip is saved with the session, and a session from before Aux 8 loads.
    const auto state = processor.getRouting().toVar();
    RoutingMatrix restored;
    restored.fromVar (state);
    CHECK (restored.isAuxMuted (kNoiseAux));

    auto old = juce::JSON::parse (juce::JSON::toString (state));

    if (auto* aux = old.getProperty ("aux", {}).getArray())
        aux->removeLast();

    restored.fromVar (old);
    CHECK (! restored.isAuxMuted (kNoiseAux));
}
