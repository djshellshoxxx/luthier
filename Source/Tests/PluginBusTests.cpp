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

    CHECK (processor.getBusCount (false) == 2 + kNumAuxBuses + kNumPerStringBuses + 2 + 3);   // + Aux 9 and 10, FEAT-JAM; + Pedal B, Wet only, Dry only (appended pedalboard buses)
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
        while (aux->size() > kNumAuxBuses)   // FEAT-JAM: Aux 9 and 10 come after it
            aux->removeLast();

    restored.fromVar (old);
    CHECK (! restored.isAuxMuted (kNoiseAux));
}

//==============================================================================
// HI-2/HI-31: NEEDS_MIDI_OUTPUT was FALSE although producesMidi() is true (the
// MidiOutRouter feeds a real MIDI-out path), so VST3/AU exposed no MIDI-out port
// in any host. CMakeLists.txt now announces it.
LUTHIER_TEST (PluginBuses, midiOutputIsAnnounced)
{
    LuthierAudioProcessor processor;
    CHECK (processor.producesMidi());

#if defined (JucePlugin_ProducesMidiOutput)
    CHECK (JucePlugin_ProducesMidiOutput == 1);
#else
    CHECK_MSG (false, "JucePlugin_ProducesMidiOutput is not defined");
#endif
}

//==============================================================================
// HI-10: host-integration.md section 2 requires the main output to be stereo;
// a mono main out used to be accepted and silently summed.
LUTHIER_TEST (PluginBuses, monoMainOutputIsRefused)
{
    LuthierAudioProcessor processor;

    auto layout = processor.getBusesLayout();
    CHECK (layout.outputBuses.size() > 0);

    layout.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
    CHECK (! processor.checkBusesLayoutSupported (layout));

    layout.outputBuses.getReference (0) = juce::AudioChannelSet::stereo();
    CHECK (processor.checkBusesLayoutSupported (layout));
}

// BOARD_CORE6.8/E54 appends optional sends after the complete existing host bus
// prefix. Literal names/indices below freeze the session-facing legacy mapping.
LUTHIER_TEST (PluginBuses, pedalboardSendsPreserveEveryLegacyOutputIndex)
{
    LuthierAudioProcessor processor;
    const char* legacyNames[] {
        "Output", "DI", "Amp (pre-cab)", "Cab Mic 1", "Cab Mic 2", "Room", "Wet FX", "Monitor",
        "String 1", "String 2", "String 3", "String 4", "String 5", "String 6",
        "String 7", "String 8", "String 9", "String 10", "String 11", "String 12",
        "Noise", "Jam Drums", "Jam Bass"
    };
    constexpr int legacyCount = 23;
    CHECK (kNumAuxBuses == 7 && kNumPerStringBuses == 12);
    CHECK (processor.getBusCount (false) == legacyCount + 3);
    for (int index = 0; index < legacyCount; ++index)
    {
        const auto* bus = processor.getBus (false, index);
        CHECK_MSG (bus != nullptr, "missing legacy bus " + juce::String (index));
        if (bus == nullptr) continue;
        CHECK_MSG (bus->getName() == legacyNames[index], "legacy output index " + juce::String (index));
        const bool perString = index >= 8 && index < 20;
        CHECK (bus->getDefaultLayout() == (perString ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo()));
        CHECK (bus->isEnabled() == (index == 0));
    }
    const char* appendedNames[] { "Pedal B", "Wet only", "Dry only" };
    for (int send = 0; send < 3; ++send)
    {
        const auto* bus = processor.getBus (false, legacyCount + send);
        CHECK (bus != nullptr);
        if (bus == nullptr) continue;
        CHECK (bus->getName() == appendedNames[send]);
        CHECK (bus->getDefaultLayout() == juce::AudioChannelSet::stereo());
        CHECK (! bus->isEnabled());
    }
    CHECK (processor.getBusCount (true) == 1);
    const auto* sidechain = processor.getBus (true, 0);
    CHECK (sidechain != nullptr);
    if (sidechain != nullptr)
    {
        CHECK (sidechain->getName() == "Sidechain");
        CHECK (sidechain->getDefaultLayout() == juce::AudioChannelSet::stereo());
        CHECK (! sidechain->isEnabled());
    }
}

LUTHIER_TEST (PluginBuses, appendedSendsSupportStereoIndependentlyAndKeepLegacyLayouts)
{
    LuthierAudioProcessor processor;
    constexpr int legacyCount = 23;
    const auto original = processor.getBusesLayout();
    CHECK (original.outputBuses.size() == legacyCount + 3);
    if (original.outputBuses.size() != legacyCount + 3) return;

    for (int mask = 0; mask < 8; ++mask)
    {
        auto layout = original;
        for (int send = 0; send < 3; ++send)
            layout.outputBuses.getReference (legacyCount + send) = (mask & (1 << send)) != 0
                ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::disabled();
        CHECK (processor.checkBusesLayoutSupported (layout));
        CHECK (processor.setBusesLayout (layout));
        const auto accepted = processor.getBusesLayout();
        for (int index = 0; index < legacyCount; ++index)
            CHECK (accepted.outputBuses[index] == original.outputBuses[index]);
        for (int send = 0; send < 3; ++send)
            CHECK (accepted.outputBuses[legacyCount + send] == layout.outputBuses[legacyCount + send]);
    }
    for (int send = 0; send < 3; ++send)
    {
        auto layout = original;
        layout.outputBuses.getReference (legacyCount + send) = juce::AudioChannelSet::mono();
        CHECK_MSG (! processor.checkBusesLayoutSupported (layout), "an appended send accepted mono");
        layout.outputBuses.getReference (legacyCount + send) = juce::AudioChannelSet::quadraphonic();
        CHECK_MSG (! processor.checkBusesLayoutSupported (layout), "an appended send accepted quad");
    }
    // Legacy stereo-only, aux, per-string and combined layouts remain valid
    // with all three new sends disabled; no existing bus needs to move.
    for (int mask = 0; mask < 4; ++mask)
    {
        auto layout = original;
        for (int index = 1; index < legacyCount; ++index)
        {
            const bool perString = index >= 8 && index < 20;
            const bool enabled = (mask & (perString ? 2 : 1)) != 0;
            layout.outputBuses.getReference (index) = ! enabled ? juce::AudioChannelSet::disabled()
                : perString ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        }
        CHECK (processor.checkBusesLayoutSupported (layout));
        CHECK (processor.setBusesLayout (layout));
        for (int send = 0; send < 3; ++send)
            CHECK (! processor.getBus (false, legacyCount + send)->isEnabled());
    }
}
