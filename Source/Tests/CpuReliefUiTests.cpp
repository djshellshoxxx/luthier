/*  performance-budget.md 8, the user-facing half of the CPU relief ladder:
    the display and stream steps (1, 2), the frozen audition (6), relief 7's
    string drop, its "CPU limit" banner and its Options -> Diagnostics opt-out. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/CpuReliefUi.h"
#include "../UI/Widgets.h"
#include "../UI/NoiseGroups.h"
#include "../UI/OptionsPages.h"
#include "../UI/UiPreferences.h"
#include "../Workshop/WorkshopBench.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    /** Runs blocks until the ladder is at `step` or better (at most ~3 s). */
    bool driveTo (LuthierAudioProcessor& processor, int step, juce::MidiBuffer first = {})
    {
        juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), kBlock);

        for (int b = 0; b < (int) (3.0 * kSr / kBlock); ++b)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (b == 0)
                midi = first;

            processor.processBlock (buffer, midi);

            if (processor.getEngine().getCpuRelief().getStep() >= step)
                return true;
        }

        return false;
    }

    juce::MidiBuffer chord()
    {
        juce::MidiBuffer midi;
        for (int note : { 40, 45, 50, 55, 59, 64 })
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), 0);
        return midi;
    }
}

LUTHIER_TEST (CpuReliefUi, theBannerComesOncePerEpisodeAndGoes)
{
    using A = CpuReliefUi::BannerAction;
    CHECK (CpuReliefUi::bannerAction (true, false) == A::post);
    CHECK (CpuReliefUi::bannerAction (true, true) == A::none);
    CHECK (CpuReliefUi::bannerAction (false, true) == A::withdraw);
    CHECK (CpuReliefUi::bannerAction (false, false) == A::none);
    CHECK (CpuReliefUi::bannerMessage().startsWith ("CPU limit"));
}

LUTHIER_TEST (CpuReliefUi, theOptOutIsSavedAndReachesTheLadder)
{
    LuthierAudioProcessor processor;
    const bool before = CpuReliefUi::isStringDropAllowed();

    CpuReliefUi::setStringDropAllowed (processor, false);
    CHECK (! CpuReliefUi::isStringDropAllowed());
    CHECK (! processor.getEngine().getCpuRelief().isStringDropAllowed());

    // Opted out, a saturated CPU stops at step 6: nothing is dropped, no banner.
    processor.prepareToPlay (kSr, kBlock);
    processor.getEngine().getCpuRelief().setLoadOverrideForTest (2.0);
    CHECK (driveTo (processor, CpuRelief::freezeAudition, chord()));
    CHECK (! driveTo (processor, CpuRelief::dropStrings));
    CHECK (processor.getEngine().getReliefDroppedStrings() == 0);
    CHECK (! processor.getEngine().getCpuRelief().isCpuLimitBannerDue());

    CpuReliefUi::setStringDropAllowed (processor, true);
    CHECK (processor.getEngine().getCpuRelief().isStringDropAllowed());

    // The Options -> Diagnostics toggle shows the saved choice and writes it.
    DiagnosticsPage page (processor);
    page.refresh();
    juce::ToggleButton* toggle = nullptr;

    for (auto* child : page.getChildren())
        if (auto* t = dynamic_cast<juce::ToggleButton*> (child); t != nullptr && t->getTitle() == "Drop strings under CPU overload")
            toggle = t;

    CHECK (toggle != nullptr);

    if (toggle != nullptr)
    {
        CHECK (toggle->getToggleState());
        toggle->setToggleState (false, juce::sendNotificationSync);
        CHECK (! CpuReliefUi::isStringDropAllowed());
    }

    CpuReliefUi::setStringDropAllowed (processor, before);
    processor.getEngine().getCpuRelief().setLoadOverrideForTest (-1.0);
}

LUTHIER_TEST (CpuReliefUi, reliefSevenDropsTheQuietestStringsAndStopsWhenTheLoadFalls)
{
    LuthierAudioProcessor processor;
    const bool before = CpuReliefUi::isStringDropAllowed();
    CpuReliefUi::setStringDropAllowed (processor, true);
    processor.prepareToPlay (kSr, kBlock);

    auto& relief = processor.getEngine().getCpuRelief();
    relief.setLoadOverrideForTest (2.0);
    CHECK (driveTo (processor, CpuRelief::dropStrings, chord()));
    CHECK (relief.isCpuLimitBannerDue());

    // Held there for 0.6 s: a string every 200 ms.
    juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), kBlock);
    for (int b = 0; b < (int) (0.6 * kSr / kBlock); ++b)
    {
        buffer.clear();
        juce::MidiBuffer none;
        processor.processBlock (buffer, none);
    }

    const int dropped = processor.getEngine().getReliefDroppedStrings();
    CHECK_MSG (dropped >= 2 && dropped <= 4, "dropped " + juce::String (dropped));

    // The load falls: the ladder steps down and the episode ends.
    relief.setLoadOverrideForTest (0.1);
    for (int b = 0; b < (int) (1.5 * kSr / kBlock); ++b)
    {
        buffer.clear();
        juce::MidiBuffer none;
        processor.processBlock (buffer, none);
    }

    CHECK (! relief.isCpuLimitBannerDue());
    CHECK (processor.getEngine().getReliefDroppedStrings() == 0);

    relief.setLoadOverrideForTest (-1.0);
    CpuReliefUi::setStringDropAllowed (processor, before);
}

LUTHIER_TEST (CpuReliefUi, streamSuspendsAndAuditionFreezesUnderLoad)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    auto& relief = processor.getEngine().getCpuRelief();

    relief.setLoadOverrideForTest (2.0);
    CHECK (driveTo (processor, CpuRelief::freezeAudition));

    // Step 2: the stream does no work, even with records arriving.
    DataStreamDisplay stream;
    stream.setSource (&processor);
    juce::Component parent;
    parent.addAndMakeVisible (stream);
    stream.setBounds (0, 0, 300, 100);
    const bool wasEnabled = DataStreamDisplay::isEnabledByUser();
    DataStreamDisplay::setEnabledByUser (true);
    processor.getDiagnostics().setCrashLogEnabled (true);
    processor.getDiagnostics().log (LogCategory::Engine, "relief test record");
    stream.update (1000.0);
    CHECK (! stream.isScrolling());
    CHECK (stream.getNumLinesKept() == 0);

    // Step 6: no new audition starts.
    auto& bench = processor.getBench();
    const auto candidates = processor.getPartLibrary().getParts (PartType::bridge);
    CHECK (! candidates.isEmpty());

    if (! candidates.isEmpty())
    {
        bench.beginAudition (GuitarSlot::bridge, candidates[0]);
        CHECK_MSG (! bench.isAuditioning(), "an audition started at relief step 6");
    }

    // Back under the threshold, both work again.
    relief.setLoadOverrideForTest (0.1);
    juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), kBlock);
    for (int b = 0; b < (int) (8.0 * kSr / kBlock) && relief.getStep() > 0; ++b)
    {
        buffer.clear();
        juce::MidiBuffer none;
        processor.processBlock (buffer, none);
    }

    CHECK (relief.getStep() == 0);
    processor.getDiagnostics().log (LogCategory::Engine, "relief test record 2");
    stream.update (2000.0);
    CHECK (stream.getNumLinesKept() > 0);

    if (! candidates.isEmpty())
    {
        bench.beginAudition (GuitarSlot::bridge, candidates[0]);
        CHECK (bench.isAuditioning());
        bench.endAudition();
    }

    processor.getDiagnostics().setCrashLogEnabled (false);
    DataStreamDisplay::setEnabledByUser (wasEnabled);
    relief.setLoadOverrideForTest (-1.0);
}
