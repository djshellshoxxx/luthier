/*  SPEC-SWEEP: the modulation family's setter clamps (advanced-ranges.md 2.1,
    3.4; PR-44 / AR-15) and the MOD tab source-card controls that had engine
    support but no UI (modulation-matrix 1.1-1.4: MM-14, 18, 19, 20, 23, 25).
*/

#include "TestFramework.h"

#include "../Modulation/ModMatrix.h"
#include "../PhysicalRange.h"
#include "../PluginProcessor.h"
#include "../UI/ModMatrixPanel.h"
#include "../UI/ModSourceEditors.h"
#include "../Modulation/ModSourceEdit.h"

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

//==============================================================================
/*  SPEC-SWEEP: MM-7 - recalling a snapshot restarts the sources, like a
    preset load (modulation-matrix 0.5). */
LUTHIER_TEST (Modulation, recallingASnapshotResetsTheSources)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    processor.getSnapshots().setCrossfadeMs (0.0);   // recall inside the call
    CHECK (processor.captureSnapshot (0, "A", 1));

    auto& m = processor.getModMatrix();
    m.getEnvelope (0).setAttackSeconds (0.01);
    m.noteOn (60, 1.0);

    ModBlockContext context;
    for (int i = 0; i < 20; ++i)
        m.processBlock (512, context);

    CHECK (m.getEnvelope (0).isActive());

    CHECK (processor.recallSnapshot (0));

    // Snapshot recall may be applied on the message thread straight away or
    // queued; either way the matrix restarts at its next block.
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    for (int i = 0; i < 4; ++i)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
    }

    CHECK (! m.getEnvelope (0).isActive());
}

//==============================================================================
/*  SPEC-SWEEP: MM-T3 - modulation-matrix 8: a matrix saved in a preset file
    and loaded back is the same matrix, byte for byte. */
LUTHIER_TEST (Modulation, aPresetFileCarriesTheMatrixExactly)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    auto& matrix = processor.getModMatrix();
    auto& presets = processor.getPresetManager();

    for (const char* destination : { "amp_gain", "amp_bass", "amp_treble" })
    {
        ModRoute route;
        route.sourceId = "lfo1";
        route.destinationId = destination;
        route.depth = 0.3f;
        route.offset = -0.1f;
        route.curve = ModCurve::sCurve;
        CHECK (matrix.addRoute (route));
    }

    matrix.getLfo (0).setRateHz (3.25);
    matrix.getEnvelope (1).setAttackSeconds (0.75);
    matrix.getSequencer (0).setInternalRateHz (6.5);

    const auto before = juce::JSON::toString (matrix.toVar(), true);

    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                  .getNonexistentChildFile ("luthier-mm-t3", PresetManager::kFileExtension, false);
    file.replaceWithText (juce::JSON::toString (presets.toVar ("MM-T3"), false));

    matrix.clearRoutes();
    matrix.getLfo (0).setRateHz (1.0);

    CHECK (presets.loadPreset (file));
    const auto after = juce::JSON::toString (matrix.toVar(), true);
    file.deleteFile();

    CHECK_MSG (before == after, "the matrix changed through a preset file:\n" + before + "\nvs\n" + after);
}

//==============================================================================
/*  SPEC-SWEEP: MM-12 / MM-22 - the breakpoint editor and the step grid write
    their sources through the matrix's edit queue. */
namespace
{
    juce::MouseEvent mouseAt (juce::Component& c, float x, float y, juce::ModifierKeys mods = {})
    {
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { x, y }, mods,
                                 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, juce::Time::getCurrentTime(),
                                 { x, y }, juce::Time::getCurrentTime(), 1, false);
    }
}

LUTHIER_TEST (ModMatrixUi, draggingABreakpointWritesTheLfo)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    auto& matrix = processor.getModMatrix();

    LfoBreakpointEditor editor;
    editor.setSize (140, LfoBreakpointEditor::preferredHeight);
    editor.getPoint = [&matrix] (int i) { return matrix.getLfo (2).getBreakpoint (i); };
    editor.setPoint = [&matrix] (int i, double v)
    {
        ModSourceEdit e;
        e.kind = ModSourceEdit::Kind::lfoBreakpoint;
        e.index = 2;
        e.subIndex = i;
        e.pointValue = v;
        matrix.postSourceEdit (e);
    };

    // Point 3 sits at x = 60 (140 / 7 per point); the top is +1.
    editor.mouseDown (mouseAt (editor, 60.0f, 0.0f));
    CHECK_NEAR (matrix.getLfo (2).getBreakpoint (3), 1.0, 1.0e-6);

    editor.mouseDrag (mouseAt (editor, 61.0f, (float) editor.getHeight() * 0.75f));
    CHECK_NEAR (matrix.getLfo (2).getBreakpoint (3), -0.5, 1.0e-6);
    CHECK_NEAR (matrix.getLfo (2).getBreakpoint (4), matrix.getLfo (0).getBreakpoint (4), 1.0e-12);   // neighbours untouched

    juce::Image image (juce::Image::ARGB, 140, LfoBreakpointEditor::preferredHeight, true);
    juce::Graphics g (image);
    editor.paintEntireComponent (g, false);
}

LUTHIER_TEST (ModMatrixUi, theStepGridWritesSteps)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    auto& matrix = processor.getModMatrix();
    auto& seq = matrix.getSequencer (1);

    StepGridEditor grid;
    grid.setSize (160, StepGridEditor::preferredHeight);   // 16 steps of 10 px
    grid.getLength = [&seq] { return seq.getLength(); };
    grid.getStep = [&seq] (int i) { return seq.getStep (i); };
    grid.setStep = [&matrix] (int i, const ModStepSequencer::Step& s)
    {
        ModSourceEdit e;
        e.kind = ModSourceEdit::Kind::seqStep;
        e.index = 1;
        e.subIndex = i;
        e.pointValue = s.value;
        e.stepGate = s.gate;
        e.stepSlide = s.slide;
        e.stepProbability = s.probability;
        matrix.postSourceEdit (e);
    };

    CHECK (seq.getLength() == 16);

    const float barHeight = (float) (StepGridEditor::preferredHeight - StepGridEditor::probabilityRowHeight);

    // Step 4: drag to the top.
    grid.mouseDown (mouseAt (grid, 45.0f, 0.0f));
    CHECK_NEAR (seq.getStep (4).value, 1.0, 1.0e-6);

    // Drag it down to a quarter below centre.
    grid.mouseDrag (mouseAt (grid, 45.0f, barHeight * 0.625f));
    CHECK_NEAR (seq.getStep (4).value, -0.25, 1.0e-6);

    // Cmd/Ctrl-click toggles the gate, Alt-click the slide.
    const bool gateBefore = seq.getStep (7).gate;
    grid.mouseDown (mouseAt (grid, 75.0f, 10.0f, juce::ModifierKeys (juce::ModifierKeys::commandModifier)));
    CHECK (seq.getStep (7).gate != gateBefore);

    const bool slideBefore = seq.getStep (7).slide;
    grid.mouseDown (mouseAt (grid, 75.0f, 10.0f, juce::ModifierKeys (juce::ModifierKeys::altModifier)));
    CHECK (seq.getStep (7).slide != slideBefore);

    // The bottom strip is probability: 30 % of the way across step 9.
    const double valueBefore = seq.getStep (9).value;
    grid.mouseDown (mouseAt (grid, 93.0f, (float) StepGridEditor::preferredHeight - 3.0f));
    CHECK_NEAR (seq.getStep (9).probability, 0.3, 1.0e-3);
    CHECK_NEAR (seq.getStep (9).value, valueBefore, 1.0e-12);   // value untouched

    // And the matrix saves what the grid wrote.
    ModMatrix copy;
    copy.fromVar (matrix.toVar());
    CHECK_NEAR (copy.getSequencer (1).getStep (4).value, -0.25, 1.0e-6);

    juce::Image image (juce::Image::ARGB, 160, StepGridEditor::preferredHeight, true);
    juce::Graphics g (image);
    grid.paintEntireComponent (g, false);
}
