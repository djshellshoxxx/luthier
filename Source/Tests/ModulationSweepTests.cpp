/*  SPEC-SWEEP: the modulation family's setter clamps (advanced-ranges.md 2.1,
    3.4; PR-44 / AR-15) and the MOD tab source-card controls that had engine
    support but no UI (modulation-matrix 1.1-1.4: MM-14, 18, 19, 20, 23, 25).
*/

#include "TestFramework.h"

#include "../Modulation/ModMatrix.h"
#include "../PhysicalRange.h"
#include "../PluginProcessor.h"
#include "../UI/ModMatrixPanel.h"

using namespace luthier;
using namespace luthier::tests;

//==============================================================================
LUTHIER_TEST (Ranges, modulationSettersClampUnlessAdvanced)
{
    // A source on its own is locked: stock.
    {
        ModLfo lfo;
        lfo.setRateHz (100.0);
        CHECK_NEAR (lfo.getRateHz(), 20.0, 1.0e-12);

        CHECK (lfo.setAdvancedRange (true) == 0);
        lfo.setRateHz (100.0);
        CHECK_NEAR (lfo.getRateHz(), 100.0, 1.0e-12);

        // Locking clamps what is there, and says so.
        CHECK (lfo.setAdvancedRange (false) == 1);
        CHECK_NEAR (lfo.getRateHz(), 20.0, 1.0e-12);
    }

    {
        ModEnvelope env;
        env.setAttackSeconds (20.0);
        CHECK_NEAR (env.getAttackSeconds(), 5.0, 1.0e-12);
        env.setHoldSeconds (20.0);   // not in the family: keeps 0-30 s
        CHECK_NEAR (env.getHoldSeconds(), 20.0, 1.0e-12);

        ModStepSequencer seq;
        seq.setInternalRateHz (300.0);
        CHECK_NEAR (seq.getInternalRateHz(), 40.0, 1.0e-12);

        ModEnvelopeFollower f;
        f.prepare (375.0);
        f.setReleaseMs (5000.0);
        CHECK_NEAR (f.getReleaseMs(), 1000.0, 1.0e-12);
    }

    // Through the processor: the family's state reaches the matrix, and an
    // unlocked 100 Hz LFO survives a state round trip.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto& lfo = processor.getModMatrix().getLfo (0);
    lfo.setRateHz (100.0);
    CHECK_NEAR (lfo.getRateHz(), 20.0, 1.0e-12);

    RangeState unlocked;
    unlocked.setFamilyAdvanced (RangeFamily::modulation, true);
    processor.setRanges (unlocked);
    CHECK (processor.getModMatrix().isModulationRangeAdvanced());

    lfo.setRateHz (100.0);
    CHECK_NEAR (lfo.getRateHz(), 100.0, 1.0e-12);

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    {
        LuthierAudioProcessor restored;
        restored.prepareToPlay (48000.0, 512);
        restored.setStateInformation (state.getData(), (int) state.getSize());

        CHECK (restored.getRanges().isFamilyAdvanced (RangeFamily::modulation));
        CHECK_MSG (std::abs (restored.getModMatrix().getLfo (0).getRateHz() - 100.0) < 1.0e-9,
                   "an unlocked 100 Hz LFO should come back at 100 Hz, got "
                     + juce::String (restored.getModMatrix().getLfo (0).getRateHz()));
    }

    // Locking clamps it back to stock and counts it.
    const int clamped = processor.setRanges (RangeState());
    CHECK (clamped >= 1);
    CHECK_NEAR (lfo.getRateHz(), 20.0, 1.0e-12);
    CHECK (! processor.getModMatrix().isModulationRangeAdvanced());
}

//==============================================================================
LUTHIER_TEST (ModMatrixUi, theSourceCardsWriteTheirNewControls)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    auto& matrix = processor.getModMatrix();

    ModSourceCard card (processor);
    card.setSize (300, ModSourceCard::preferredHeight);
    auto c = card.getSweepControls();

    // LFO: phase (MM-14).
    card.setSlot (ModSourceSlots::lfoBase);
    CHECK (c.phase->isVisible());
    c.phase->setValue (90.0, juce::sendNotificationSync);
    CHECK_NEAR (matrix.getLfo (0).getPhaseOffsetDegrees(), 90.0, 1.0e-6);

    // The rate slider offers the locked pair (PR-44).
    CHECK_NEAR (c.rate->getMaximum(), 20.0, 1.0e-9);

    // Envelope: retrigger, loop and stage curves (MM-18 / 19 / 20).
    card.setSlot (ModSourceSlots::envBase);
    CHECK (c.envRetrigger->isVisible() && c.loopMode->isVisible() && c.attackCurve->isVisible());
    CHECK (! c.phase->isVisible());

    c.envRetrigger->setSelectedId (3, juce::sendNotificationSync);
    c.loopMode->setSelectedId (2, juce::sendNotificationSync);
    c.attackCurve->setSelectedId ((int) ModCurve::exponential + 1, juce::sendNotificationSync);
    c.releaseCurve->setSelectedId ((int) ModCurve::sCurve + 1, juce::sendNotificationSync);

    const auto& env = matrix.getEnvelope (0);
    CHECK (env.getRetrigger() == ModEnvelope::Retrigger::oneShot);
    CHECK (env.getLoopMode() == ModEnvelope::LoopMode::decayToSustain);
    CHECK (env.getStageCurve (ModEnvelope::Stage::attack) == ModCurve::exponential);
    CHECK (env.getStageCurve (ModEnvelope::Stage::release) == ModCurve::sCurve);

    // Sequencer: the free-running rate (MM-23), saved with the matrix.
    card.setSlot (ModSourceSlots::seqBase);
    CHECK (c.seqRate->isVisible());
    c.seqRate->setValue (7.5, juce::sendNotificationSync);
    CHECK_NEAR (matrix.getSequencer (0).getInternalRateHz(), 7.5, 1.0e-6);

    {
        ModMatrix copy;
        copy.fromVar (matrix.toVar());
        CHECK_NEAR (copy.getSequencer (0).getInternalRateHz(), 7.5, 1.0e-6);
    }

    // Follower: string and log (MM-25).
    card.setSlot (ModSourceSlots::followerBase);
    CHECK (c.followerString->isVisible() && c.followerLog->isVisible());
    c.followerString->setSelectedId (4, juce::sendNotificationSync);
    c.followerLog->setToggleState (true, juce::sendNotificationSync);
    CHECK (matrix.getFollower (0).getStringIndex() == 3);
    CHECK (matrix.getFollower (0).isLogarithmic());

    // Unlocking the family widens the sliders at the next refresh.
    RangeState unlocked;
    unlocked.setFamilyAdvanced (RangeFamily::modulation, true);
    processor.setRanges (unlocked);
    card.setSlot (ModSourceSlots::followerBase);
    CHECK_NEAR (c.followerRelease->getMaximum(), 10000.0, 1.0e-9);
    card.setSlot (ModSourceSlots::lfoBase);
    CHECK_NEAR (c.rate->getMaximum(), 200.0, 1.0e-9);
}

//==============================================================================
/*  SPEC-SWEEP: MM-40 - the route table shows and edits each route's offset
    beside its depth (modulation-matrix 5). */
LUTHIER_TEST (ModMatrixUi, theRouteTableEditsDepthAndOffset)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    auto& matrix = processor.getModMatrix();

    ModRoute route;
    route.sourceId = "macro1";
    route.destinationId = "amp_gain";
    route.depth = 0.5f;
    CHECK (matrix.addRoute (route));

    ModRouteTable table (processor);
    table.setSize (400, 160);
    table.refresh();
    CHECK (table.getNumRows() == 1);

    table.applyTypedValue (0, true, 0.25f);
    CHECK_NEAR (matrix.getRoute (0).offset, 0.25f, 1.0e-6f);

    table.applyTypedValue (0, false, -0.75f);
    CHECK_NEAR (matrix.getRoute (0).depth, -0.75f, 1.0e-6f);

    // Paints every column, the new one included, without complaint.
    juce::Image image (juce::Image::ARGB, 400, 20, true);
    juce::Graphics g (image);
    for (int column = 1; column <= 7; ++column)
        table.paintCell (g, 0, column, 50, 20, false);
}
