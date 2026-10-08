/*  SPEC-SWEEP: volume-knob-interaction.md tests the audit found missing
    (VK-13 the parameter table, VK-16 50s wiring). */

#include "TestFramework.h"

#include "../DSP/Circuit/GuitarCircuit.h"
#include "../Parameters.h"
#include "../PluginProcessor.h"
#include "../UI/EasyPanel.h"
#include "../UI/CircuitPanel.h"

using namespace luthier;
using namespace luthier::tests;

//==============================================================================
/*  VK-13: volume-knob-interaction.md 3 - every circuit parameter exists with
    the table's default. */
LUTHIER_TEST (Circuit, parametersMatchTheSpecTable)
{
    LuthierAudioProcessor processor;
    auto& state = processor.getState();

    auto plain = [&state] (const char* id) -> double
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id));

        if (p == nullptr)
            return -1.0e99;

        return (double) p->convertFrom0to1 (p->getDefaultValue());
    };

    struct Row { const char* id; double expected; double tolerance; };

    const Row rows[] =
    {
        { ParamIDs::guitarVolume,       1.0,     1.0e-6 },
        { ParamIDs::guitarTone,         1.0,     1.0e-6 },
        { ParamIDs::circuitVolumePot,   500.0e3, 1.0 },
        { ParamIDs::circuitTonePot,     500.0e3, 1.0 },
        { ParamIDs::circuitToneCap,     22.0,    1.0e-3 },
        { ParamIDs::circuitPotTaper,    (double) PotTaper::audio,        1.0e-6 },
        { ParamIDs::circuitTrebleBleed, (double) TrebleBleed::none,      1.0e-6 },
        { ParamIDs::circuitBleedR,      130.0e3, 1.0 },
        { ParamIDs::circuitBleedC,      1.1,     1.0e-3 },
        { ParamIDs::circuitBleedMode,   0.0,     1.0e-6 },   // parallel
        { ParamIDs::circuitActive,      0.0,     1.0e-6 },
        { ParamIDs::ampInputImpedance,  1.0e6,   1.0 },
        { ParamIDs::cableOn,            1.0,     1.0e-6 },
        { ParamIDs::cableLength,        3.0,     1.0e-3 },
        { ParamIDs::cableQuality,       (double) CableQuality::standard, 1.0e-6 },
    };

    for (const auto& row : rows)
    {
        const double v = plain (row.id);
        CHECK_MSG (std::abs (v - row.expected) <= row.tolerance,
                   juce::String (row.id) + " default " + juce::String (v) + ", spec says " + juce::String (row.expected));
    }
}

//==============================================================================
/*  VK-16: 50s wiring takes the tone control off the wiper, so turning the
    volume down keeps more of the top than modern (audio-taper) wiring does. */
LUTHIER_TEST (Circuit, fiftiesWiringKeepsTheTop)
{
    CircuitComponents p;
    p.tone = 0.8;

    auto presence = [] (CircuitComponents c, double volume)
    {
        c.volume = volume;
        return GuitarCircuit::magnitudeDb (c, 4000.0) - GuitarCircuit::magnitudeDb (c, 300.0);
    };

    auto modern = p;
    modern.taper = PotTaper::audio;
    auto fifties = p;
    fifties.taper = PotTaper::fiftiesWiring;

    const double modernLoss  = presence (modern, 0.7) - presence (modern, 1.0);
    const double fiftiesLoss = presence (fifties, 0.7) - presence (fifties, 1.0);

    CHECK_MSG (std::abs (fiftiesLoss) < std::abs (modernLoss),
               "50s wiring should lose less top at volume 0.7: "
               + juce::String (fiftiesLoss, 2) + " dB vs modern " + juce::String (modernLoss, 2) + " dB");
}

//==============================================================================
/*  VK-25: gui-integration 4.1 - the Easy rig strip carries the guitar's own
    volume and tone, attached to their parameters, beside a visible circuit
    view. */
LUTHIER_TEST (Circuit, theEasyRigStripCarriesTheGuitarKnobsAndTheView)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    EasyPanel panel (processor);
    panel.setSize (1200, 800);

    juce::Array<LuthierKnob*> knobs;
    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (auto* k = dynamic_cast<LuthierKnob*> (child)) knobs.add (k);
            walk (*child);
        }
    };
    walk (panel);

    auto find = [&] (const char* id) -> LuthierKnob*
    {
        for (auto* k : knobs)
            if (k->getParameterId() == id && k->isVisible() && k->getWidth() > 0)
                return k;
        return nullptr;
    };

    CHECK_MSG (find (ParamIDs::guitarVolume) != nullptr, "no visible knob on guitar_volume");
    CHECK_MSG (find (ParamIDs::guitarTone) != nullptr, "no visible knob on guitar_tone");

    CircuitResponseView* view = nullptr;
    std::function<void (juce::Component&)> findView = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (auto* v = dynamic_cast<CircuitResponseView*> (child)) view = v;
            findView (*child);
        }
    };
    findView (panel);

    CHECK_MSG (view != nullptr && view->isVisible() && view->getWidth() > 0 && view->getHeight() > 0,
               "the circuit view is missing or has no size in the Easy rig strip");
}
