/*  The CHARACTER tab's STRING AGING, ENVIRONMENT and BODY COUPLING groups
    (string-aging.md 7, environment.md 7, body-coupling.md 5): the gestures
    reach the parameters and the engine, and the panel carries the groups.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/RealismGroups.h"
#include "../UI/CharacterPanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    float plain (LuthierAudioProcessor& p, const char* id)
    {
        auto* r = dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id));
        return r != nullptr ? r->convertFrom0to1 (r->getValue()) : -1.0f;
    }

    void process (LuthierAudioProcessor& p, int blocks)
    {
        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;

        for (int b = 0; b < blocks; ++b)
        {
            buffer.clear();
            p.processBlock (buffer, midi);
        }
    }
}

LUTHIER_TEST (RealismUi, restringAllZeroesTheHoursAndTheState)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (ParamIDs::stringAgeHours)))
        p->setValueNotifyingHost (p->convertTo0to1 (80.0f));

    process (processor, 2);

    StringAgingGroup group (processor);
    group.setSize (300, group.preferredHeight());

    group.restring (2);
    process (processor, 1);
    CHECK_NEAR (processor.getEngine().getStringAging().getBaseHours (2), 80.0, 1.0e-3);

    group.restringAll();
    process (processor, 1);
    CHECK_NEAR (plain (processor, ParamIDs::stringAgeHours), 0.0, 1.0e-6);

    for (int s = 0; s < 6; ++s)
        CHECK (processor.getEngine().getStringAging().getBaseHours (s) == 0.0);
}

LUTHIER_TEST (RealismUi, retuneWritesTunedAtAndZeroesTheOffsets)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (ParamIDs::envTemperatureC)))
        p->setValueNotifyingHost (p->convertTo0to1 (30.0f));

    // Two minutes of warming.
    process (processor, (int) (120.0 * kSr / kBlock));
    CHECK (std::abs (processor.getEngine().getEnvironment().getState().openCents[5]) > 1.0);

    EnvironmentGroup group (processor);
    group.retune();
    process (processor, 1);

    CHECK (plain (processor, ParamIDs::envTunedAtC) > 22.5f);

    for (int s = 0; s < 6; ++s)
        CHECK_NEAR (processor.getEngine().getEnvironment().getState().openCents[(size_t) s], 0.0, 0.05);
}

LUTHIER_TEST (RealismUi, theCharacterPanelCarriesTheGroups)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    CharacterPanel panel (processor);
    panel.setSize (340, panel.preferredHeight());

    CHECK (panel.stringAgingGroup != nullptr && panel.stringAgingGroup->getHeight() > 0);
    CHECK (panel.environmentGroup != nullptr && panel.environmentGroup->getHeight() > 0);
    CHECK (panel.bodyCouplingGroup != nullptr && panel.bodyCouplingGroup->getHeight() > 0);

    // STRING AGING after STRING NOISE, BODY COUPLING after SETUP.
    CHECK (panel.bodyCouplingGroup->getY() > panel.stringAgingGroup->getY());

    // The wolf map is filled from the design.
    panel.bodyCouplingGroup->refreshNow();
    CHECK (panel.bodyCouplingGroup->getWolfMap().getNumStrings() == processor.getEngine().getNumStrings());
}
