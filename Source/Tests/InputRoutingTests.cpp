/*  input-routing.md: which consumer sees an incoming event, in which order, and
    which consumers take an event away from the ones after them (IR-29). Each
    test drives a real processor, because the chain lives in processSlice.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Controllers/ControllerProfile.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    void render (LuthierAudioProcessor& processor, juce::MidiBuffer& midi)
    {
        juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumInputChannels(),
                                                     processor.getTotalNumOutputChannels()), kBlock);
        buffer.clear();
        processor.processBlock (buffer, midi);
    }

    void renderEmpty (LuthierAudioProcessor& processor, int blocks)
    {
        for (int i = 0; i < blocks; ++i)
        {
            juce::MidiBuffer none;
            render (processor, none);
        }
    }
}

//==============================================================================
/*  IR-3 (input-routing 5, "MIDI Learn consumes the event it learns"): moving the
    mod wheel to assign it must not also put vibrato on the note. Once learned,
    the wheel both drives the parameter and keeps its instrument meaning. */
LUTHIER_TEST (InputRouting, midiLearnConsumesTheEventItLearns)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    renderEmpty (processor, 2);

    auto& learn = processor.getMidiLearn();
    auto& interp = processor.getEngine().getMidiInterpreter();

    learn.startLearning (ParamIDs::ampGain);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 100), 10);
    midi.addEvent (juce::MidiMessage::noteOn (1, 52, 0.8f), 20);
    render (processor, midi);

    CHECK_MSG (interp.getVibratoDepth() == 0.0,
               "the learned mod wheel also reached the interpreter: vibrato "
                 + juce::String (interp.getVibratoDepth(), 3));

    // The note in the same block was not collateral damage.
    bool noteSurvived = false;

    for (int s = 0; s < processor.getEngine().getNumStrings(); ++s)
        noteSurvived = noteSurvived || interp.getStringMidiNote (s) == 52;

    CHECK_MSG (noteSurvived, "the note sharing the learned CC's block was dropped");

    learn.dispatchPendingLearn();
    CHECK (learn.getCcForParameter (ParamIDs::ampGain) == 1);

    // Learned: the next wheel move drives the parameter and still plays.
    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 0);
    render (processor, midi);

    CHECK (interp.getVibratoDepth() > 0.9);

    if (auto* gain = processor.getState().getParameter (ParamIDs::ampGain))
        CHECK_NEAR (gain->getValue(), 1.0f, 1.0e-3);
}

//==============================================================================
/*  IR-11 / live-performance 8: a calibrated pedal reaches every consumer through
    its calibration. CC 11 (master level) is the consumer here: a pedal that only
    travels 20..100 must still reach silence and unity. */
LUTHIER_TEST (InputRouting, calibratedCcReachesConsumersRemapped)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    renderEmpty (processor, 1);

    auto& expression = processor.getExpression();
    const bool hadEleven = expression.has (11);
    const auto previous = expression.get (11);

    ExpressionCalibration pedal;
    pedal.ccNumber = 11;
    pedal.rawMinimum = 20;
    pedal.rawMaximum = 100;
    expression.set (pedal);
    processor.serviceExpressionCalibration();

    auto& interp = processor.getEngine().getMidiInterpreter();

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 11, 100), 0);
    render (processor, midi);
    CHECK_NEAR (interp.getMasterLevel(), 1.0, 1.0e-6);

    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 11, 20), 0);
    render (processor, midi);
    CHECK_NEAR (interp.getMasterLevel(), 0.0, 1.0e-6);

    // Uncalibrated CCs pass untouched.
    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 64), 0);
    render (processor, midi);
    CHECK_NEAR (interp.getVibratoDepth(), 64.0 / 127.0, 1.0e-6);

    if (hadEleven) expression.set (previous);
    else           expression.remove (11);
}

/*  LP-34 / IR-11: the calibration wizard hears the pedal through the audio
    thread. It used to be fed nothing, so heel and toe were the defaults. */
LUTHIER_TEST (InputRouting, theCalibrationWizardHearsThePedal)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& expression = processor.getExpression();
    const bool hadFour = expression.has (4);
    const auto previous = expression.get (4);

    expression.beginCalibration (4);
    processor.serviceExpressionCalibration();

    auto sweep = [&processor] (std::initializer_list<int> values)
    {
        juce::MidiBuffer midi;
        int t = 0;

        for (int v : values)
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 4, v), t++);

        render (processor, midi);
        processor.serviceExpressionCalibration();
    };

    sweep ({ 30, 18, 12, 15 });      // rocked back
    expression.confirmStage();
    sweep ({ 90, 109, 104 });        // rocked forward
    expression.confirmStage();

    CHECK (expression.getWizardStage() == ExpressionCalibrationSet::WizardStage::done);
    CHECK (expression.get (4).rawMinimum == 12);
    CHECK (expression.get (4).rawMaximum == 109);

    if (hadFour) expression.set (previous);
    else         expression.remove (4);
}

/*  IR-14 (input-routing 1.4): Bank Select chooses a preset only in "Bank + PC"
    mode; in "PC only" it passes through like any CC. Program Change is taken
    either way. */
LUTHIER_TEST (LiveSnapshots, bankSelectOnlySelectsAPresetInBankPlusPcMode)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    CHECK (processor.doesBankSelectChoosePreset());   // the default keeps live-performance 2

    // The MIDI capture sits just after the live stage, so its event count says
    // what got through: PC is always taken; CC 0 only in "PC only" mode.
    auto passedThrough = [&processor]
    {
        auto& capture = processor.getMidiCapture();
        const int before = capture.getEventCount();

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 0, 3), 0);
        midi.addEvent (juce::MidiMessage::programChange (1, 2), 1);
        render (processor, midi);

        return capture.getEventCount() - before;
    };

    CHECK_MSG (passedThrough() == 0, "Bank + PC let bank select or PC through");

    processor.setBankSelectChoosesPreset (false);
    CHECK_MSG (passedThrough() == 1, "PC only did not pass bank select through (or let PC through)");

    processor.setBankSelectChoosesPreset (false);
    juce::MemoryBlock state;
    processor.getStateInformation (state);

    LuthierAudioProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (! restored.doesBankSelectChoosePreset());
}

/*  IR-12 (input-routing 1.3): controllers reach the engines only through the
    interpreter's mapping. With the rhythm engine running, the mod wheel and
    CC 74 change the mapped targets and nothing about the pattern or the
    strings' tuning. */
LUTHIER_TEST (InputRouting, ccsOnlyReachEnginesThroughTheInterpreter)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& rhythm = processor.getEngine().getRhythmEngine();
    rhythm.setEnabled (true);
    rhythm.setFreeRun (true);

    const auto patternBefore = juce::JSON::toString (rhythm.getPattern().toVar());
    const double densityBefore = rhythm.getVoicingDensity();
    const int styleBefore = (int) rhythm.getVoicingStyle();

    auto& tuning = processor.getEngine().getTuningEngine();
    const double hzBefore = tuning.getEffectiveOpenFrequency (0);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 0);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 100), 1);
    render (processor, midi);
    renderEmpty (processor, 2);

    auto& interp = processor.getEngine().getMidiInterpreter();
    CHECK (interp.getVibratoDepth() > 0.9);   // CC 1's mapped target

    CHECK (juce::JSON::toString (rhythm.getPattern().toVar()) == patternBefore);
    CHECK_NEAR (rhythm.getVoicingDensity(), densityBefore, 1.0e-9);
    CHECK ((int) rhythm.getVoicingStyle() == styleBefore);
    CHECK_NEAR (tuning.getEffectiveOpenFrequency (0), hzBefore, 0.01);   // tuning-stability drift is allowed; a CC-driven retune is not
}

/*  UW-T5 (ui-wiring 23): every CC except the pedals and channel-mode messages
    the learner skips (64, 66, 120-127) is learned within one block, and the
    next block drives the parameter. */
LUTHIER_TEST (MidiLearn, everyCcLearnsWithinOneBlock)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& learn = processor.getMidiLearn();
    auto* param = processor.getState().getParameter (ParamIDs::ampGain);
    CHECK (param != nullptr);

    if (param == nullptr)
        return;

    int failures = 0;

    for (int cc = 0; cc < 120; ++cc)
    {
        if (cc == 64 || cc == 66)
            continue;

        learn.clearAllMappings();
        learn.startLearning (ParamIDs::ampGain);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, cc, 10), 0);
        render (processor, midi);
        learn.dispatchPendingLearn();

        if (learn.getCcForParameter (ParamIDs::ampGain) != cc)
        {
            ++failures;
            continue;
        }

        midi.clear();
        midi.addEvent (juce::MidiMessage::controllerEvent (1, cc, 127), 0);
        render (processor, midi);

        if (param->getValue() < 0.99f)
            ++failures;
    }

    CHECK_MSG (failures == 0, juce::String (failures) + " CCs did not learn and drive within a block");
}

/*  IR-5 (input-routing 1 step 3): the controller profile stage sits before the
    interpreter - a hex pickup's channels land on their strings, and the note
    moves earlier by the profile's latency budget. */
LUTHIER_TEST (InputRouting, profileStageRemapsChannelsAndTime)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    ControllerProfileLibrary library;
    const int gk = library.indexOf ("roland-gk");
    CHECK (gk >= 0);

    if (gk < 0)
        return;

    auto profile = library.getProfile (gk);
    profile.latencyMsMeasured = 4.0;   // 192 samples
    processor.applyControllerProfile (profile);
    renderEmpty (processor, 1);

    const int arrivesAt = 300;
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (13, 55, 0.8f), arrivesAt);   // GK channel 13 = string 2
    render (processor, midi);

    CHECK (processor.getEngine().getMidiInterpreter().getStringMidiNote (2) == 55);

    const auto& activity = processor.getEngine().getStringActivity();
    int onsetAt = -1;

    for (int i = 0; i < activity.size(); ++i)
        if (activity[i].isNoteOn && activity[i].stringIndex == 2)
            onsetAt = activity[i].sampleOffset;

    CHECK_MSG (onsetAt >= 0 && onsetAt <= arrivesAt - 150,
               "the note sounded at " + juce::String (onsetAt) + ", arrived at " + juce::String (arrivesAt));
}
