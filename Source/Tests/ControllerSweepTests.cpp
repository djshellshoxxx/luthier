/*  SPEC-SWEEP controllers checks (controllers.md): the chosen profile through
    the processor (CT-2, CT-4, CT-7), the MPE rules the interpreter enforces
    (CT-10, CT-16, CT-17, CT-26) and the UI command queue (ui-wiring UW-5).
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Controllers/ControllerProfile.h"
#include "../Controllers/ControllerStage.h"
#include "../Model/Playing/MidiInterpreter.h"
#include "../Model/Playing/TuningEngine.h"
#include "../Model/Playing/TechniqueEngine.h"
#include "../Model/Playing/RubricVoicer.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    struct MpeFixture
    {
        MpeFixture()
        {
            tuning.prepare (kSr);
            tuning.setNumStrings (6);
            tuning.setTuningPreset (TuningPreset::Standard);

            voicer.prepare (&tuning, 6);
            technique.prepare (kSr, 6);

            interpreter.prepare (kSr, 6);
            interpreter.setEngines (&tuning, &technique, &voicer);
            interpreter.setNumStrings (6);
            interpreter.setChordWindowMs (0.0);

            MidiInterpreter::Humanisation flat;
            flat.amount = 0.0;
            interpreter.setHumanisation (flat);
        }

        bool applyProfile (const char* id)
        {
            ControllerProfileLibrary library;
            const int index = library.indexOf (id);

            if (index >= 0)
                ControllerProfileLibrary::apply (library.getProfile (index), interpreter);

            return index >= 0;
        }

        PlayEventQueue send (const juce::MidiMessage& m)
        {
            juce::MidiBuffer midi;
            midi.addEvent (m, 0);

            PlayEventQueue out;
            interpreter.processBlock (midi, 256, position, out);
            position += 256;
            return out;
        }

        int noteOn (int channel, int note)
        {
            const auto out = send (juce::MidiMessage::noteOn (channel, note, 0.8f));
            return out.getNumNoteOns() > 0 ? out.getNoteOn (0).stringIndex : -1;
        }

        TuningEngine tuning;
        TechniqueEngine technique;
        RubricVoicer voicer;
        MidiInterpreter interpreter;
        int64_t position = 0;
    };

    void renderBlocks (LuthierAudioProcessor& processor, int count, juce::MidiBuffer* midi = nullptr)
    {
        juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumInputChannels(),
                                                     processor.getTotalNumOutputChannels()), kBlock);
        juce::MidiBuffer empty;

        for (int i = 0; i < count; ++i)
        {
            buffer.clear();
            processor.processBlock (buffer, (i == 0 && midi != nullptr) ? *midi : empty);
        }
    }

    ControllerProfile profileById (const char* id)
    {
        ControllerProfileLibrary library;
        const int index = library.indexOf (id);
        return index >= 0 ? library.getProfile (index) : ControllerProfile {};
    }
}

//==============================================================================
/*  CT-7: the parameter bridge re-applies mpe_enabled and bend_range every block.
    It used to write the parameters' defaults (MPE off, 2 semitones) straight over
    the Seaboard profile the player had just chosen. */
LUTHIER_TEST (Controllers, anMpeProfileSurvivesTheParameterBridge)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    renderBlocks (processor, 2);

    const auto seaboard = profileById ("roli-seaboard");
    CHECK (seaboard.isValid());
    CHECK (seaboard.mode == ControllerMode::mpe);

    processor.applyControllerProfile (seaboard);
    renderBlocks (processor, 4);
    processor.getParameterBridge().applyToEngine();

    auto& interp = processor.getEngine().getMidiInterpreter();
    CHECK_MSG (interp.isMpeEnabled(), "the bridge turned MPE back off");
    CHECK_NEAR (interp.getPitchBendRange(), seaboard.memberPitchBendSemis, 0.01);
    CHECK (interp.getPlayingMode() == PlayingMode::GuitarController);
    CHECK (interp.getMpeMasterChannel() == seaboard.mpeMasterChannel);

    // Back to a standard profile: MPE goes off and the bend returns.
    const auto generic = profileById ("generic-midi");
    processor.applyControllerProfile (generic);
    renderBlocks (processor, 2);

    CHECK (! interp.isMpeEnabled());
    CHECK_NEAR (interp.getPitchBendRange(), generic.pitchBendSemis, 0.01);
}

/*  CT-2: the chosen profile is part of the session. */
LUTHIER_TEST (Controllers, theChosenProfileSurvivesTheSessionRoundTrip)
{
    juce::MemoryBlock state;

    {
        LuthierAudioProcessor processor;
        processor.prepareToPlay (kSr, kBlock);
        processor.applyControllerProfile (profileById ("roland-gk"));
        renderBlocks (processor, 2);
        processor.getStateInformation (state);
    }

    LuthierAudioProcessor restored;
    restored.prepareToPlay (kSr, kBlock);
    restored.setStateInformation (state.getData(), (int) state.getSize());
    renderBlocks (restored, 2);

    CHECK_MSG (restored.getControllerProfileId() == "roland-gk",
               "restored profile was '" + restored.getControllerProfileId() + "'");

    auto& interp = restored.getEngine().getMidiInterpreter();
    CHECK (interp.getPlayingMode() == PlayingMode::GuitarController);
    CHECK (interp.getChannelForString (0) == 11);
}

/*  CT-4: a controller's latency budget moves its notes earlier in the block
    (never before the block starts). */
LUTHIER_TEST (Controllers, latencyCompensationMovesNotesEarlierByTheBudget)
{
    ControllerProfile laggy;
    laggy.id = "test-laggy";
    laggy.latencyMsDefault = 5.0;   // 240 samples at 48 kHz

    ControllerStage stage;
    stage.setProfile (laggy);
    CHECK_NEAR (stage.getLatencyMs(), 5.0, 1.0e-9);

    juce::MidiBuffer midi, scratch;
    scratch.ensureSize (1024);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 300);
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 400);
    midi.addEvent (juce::MidiMessage::noteOn (1, 64, 0.8f), 100);

    stage.compensateLatency (midi, kBlock, kSr, scratch);

    juce::Array<int> notePositions;
    int pedalAt = -1;

    for (const auto m : midi)
    {
        if (m.getMessage().isNoteOn())
            notePositions.add (m.samplePosition);
        else if (m.getMessage().isController())
            pedalAt = m.samplePosition;
    }

    CHECK (notePositions.size() == 2);
    CHECK (notePositions.contains (160));   // 400 - 240
    CHECK (notePositions.contains (0));     // 100 - 240, clamped to the block
    CHECK (pedalAt == 60);                  // the pedal moves with the notes

    // Measured latency wins over the default; switching compensation off leaves
    // the stream alone.
    laggy.latencyMsMeasured = 1.0;
    stage.setProfile (laggy);
    CHECK_NEAR (stage.getLatencyMs(), 1.0, 1.0e-9);

    stage.setLatencyCompensationEnabled (false);
    juce::MidiBuffer untouched;
    untouched.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 400);
    stage.compensateLatency (untouched, kBlock, kSr, scratch);

    for (const auto m : untouched)
        CHECK (m.samplePosition == 400);
}

//==============================================================================
/*  CT-17: in an MPE zone the master channel carries zone messages, not notes. */
LUTHIER_TEST (Controllers, mpeMasterChannelNotesAreIgnored)
{
    MpeFixture f;
    CHECK (f.applyProfile ("generic-mpe"));

    CHECK (f.interpreter.isMpeEnabled());
    CHECK_MSG (f.noteOn (1, 64) < 0, "a note on the MPE master channel sounded");
    CHECK_MSG (f.noteOn (2, 64) >= 0, "a note on a member channel did not sound");
}

/*  CT-18 / CT-26 (controllers.md 7): "MPE member channels stick to their string".
    A member channel's next note stays on the string it last played while that
    string can reach it, and its bend and pressure go to that string alone. */
LUTHIER_TEST (Controllers, mpeMemberChannelsStickToTheirString)
{
    MpeFixture f;
    CHECK (f.applyProfile ("generic-mpe"));

    const int first = f.noteOn (5, 57);   // A3
    CHECK (first >= 0);

    f.send (juce::MidiMessage::noteOff (5, 57));

    // Two semitones up: on most strings; the channel keeps the one it had.
    const int second = f.noteOn (5, 59);
    CHECK_MSG (second == first, "channel 5 moved from string " + juce::String (first)
                                  + " to " + juce::String (second));
    CHECK (f.interpreter.getLastStringForChannel (5) == first);

    // A second channel playing at the same time takes a different string.
    const int other = f.noteOn (6, 59);
    CHECK (other >= 0 && other != second);

    // The member bend reaches only channel 5's string, at the member range.
    const auto out = f.send (juce::MidiMessage::pitchWheel (5, 8192 + 4096));
    CHECK (out.getNumBends() == 1);

    if (out.getNumBends() == 1)
    {
        CHECK (out.getBend (0).stringIndex == second);
        CHECK_NEAR (out.getBend (0).cents, 0.5 * f.interpreter.getPitchBendRange() * 100.0, 1.0);
    }
}

/*  CT-10: the Osmose's pitch curve shapes member bends in the interpreter. */
LUTHIER_TEST (Controllers, osmoseBendFollowsTheCurveThroughTheInterpreter)
{
    MpeFixture f;
    CHECK (f.applyProfile ("osmose"));

    const auto osmose = profileById ("osmose");
    CHECK (osmose.pitchCurve.size() >= 2);

    const int s = f.noteOn (3, 60);
    CHECK (s >= 0);

    const auto out = f.send (juce::MidiMessage::pitchWheel (3, 8192 + 2048));   // +0.25
    CHECK (out.getNumBends() == 1);

    if (out.getNumBends() == 1)
    {
        const double expected = osmose.applyPitchCurve (0.25) * f.interpreter.getPitchBendRange() * 100.0;
        CHECK_NEAR (out.getBend (0).cents, expected, 2.0);

        // And it is not the linear answer, or the curve did nothing.
        CHECK (std::abs (out.getBend (0).cents - 0.25 * f.interpreter.getPitchBendRange() * 100.0) > 5.0);
    }
}

/*  CT-16: on a hex pickup, pressure on a string's channel is that string's. */
LUTHIER_TEST (Controllers, pressureOnAChannelVibratesOnlyItsString)
{
    MpeFixture f;
    CHECK (f.applyProfile ("roland-gk"));

    // GK: string 0 on channel 11, so channel 13 is string 2.
    f.noteOn (13, 55);
    const auto out = f.send (juce::MidiMessage::channelPressureChange (13, 100));

    CHECK (out.getNumPressures() == 1);

    if (out.getNumPressures() == 1)
        CHECK_MSG (out.getPressure (0).stringIndex == 2,
                   "pressure went to string " + juce::String (out.getPressure (0).stringIndex));
}

//==============================================================================
/*  ui-wiring UW-5: panic, string mute and string detune are posted by the UI
    and applied by the audio thread at the top of its next block. */
LUTHIER_TEST (StateModel, uiCommandsReachTheEngineOnTheAudioThread)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    renderBlocks (processor, 2);

    auto& tuning = processor.getEngine().getTuningEngine();
    const double before = tuning.getStringTuning (1).detuneCents;

    processor.setStringDetuneCents (1, 17.0);
    processor.setStringMuted (3, true);
    processor.panic();

    // Nothing touched the engine yet: the commands wait for the audio thread.
    CHECK_NEAR (tuning.getStringTuning (1).detuneCents, before, 1.0e-9);
    CHECK (processor.getNumPendingEngineCommands() == 3);
    CHECK (processor.isStringMuted (3));

    renderBlocks (processor, 1);

    CHECK (processor.getNumPendingEngineCommands() == 0);
    CHECK_NEAR (tuning.getStringTuning (1).detuneCents, 17.0, 1.0e-4);

    processor.setStringMuted (3, false);
    renderBlocks (processor, 1);
    CHECK (! processor.isStringMuted (3));
}

//==============================================================================
/*  PT-21 (docs/PLAYING_TECHNIQUES.md, CC table): "11 | Master level". CC 11 was
    mapped to MidiTarget::MasterLevel and then dropped on the floor. */
LUTHIER_TEST (Controllers, cc11MovesTheMasterLevel)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    renderBlocks (processor, 2);

    auto& bus = processor.getEngine().getMasterBus();
    const double unity = bus.getGainDb();

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 11, 64), 0);
    renderBlocks (processor, 1, &midi);
    processor.getParameterBridge().applyToEngine();

    CHECK_NEAR (processor.getEngine().getMidiInterpreter().getMasterLevel(), 0.5, 0.02);   // CC value to 0..1 as the interpreter maps it
    CHECK_MSG (bus.getGainDb() < unity - 5.0, "CC 11 at half left the master at "
                                                 + juce::String (bus.getGainDb(), 1) + " dB");

    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 11, 127), 0);
    renderBlocks (processor, 1, &midi);
    processor.getParameterBridge().applyToEngine();
    CHECK_NEAR (bus.getGainDb(), unity, 0.01);

    // A CC mapped to a macro target is handed to the processor for the macro.
    auto& interp = processor.getEngine().getMidiInterpreter();
    interp.setCcTarget (20, MidiTarget::Drive);
    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 0);
    renderBlocks (processor, 1, &midi);

    CHECK_NEAR (interp.takeMacroTarget (MidiTarget::Drive), 1.0f, 1.0e-3);
    CHECK (interp.takeMacroTarget (MidiTarget::Drive) < 0.0f);   // taken once
}

/*  PT-23 (docs/PLAYING_TECHNIQUES.md: aftertouch "can be switched to bend"). */
LUTHIER_TEST (Controllers, aftertouchCanDriveBend)
{
    MpeFixture f;
    CHECK (f.applyProfile ("roland-gk"));
    f.interpreter.setAftertouchTarget (MidiTarget::PitchBend);

    f.noteOn (13, 55);   // string 2
    const auto out = f.send (juce::MidiMessage::channelPressureChange (13, 127));

    CHECK (out.getNumBends() == 1);

    if (out.getNumBends() == 1)
    {
        CHECK (out.getBend (0).stringIndex == 2);
        CHECK (out.getBend (0).cents > 100.0);
    }

    CHECK_NEAR (f.interpreter.getVibratoDepth(), 0.0, 1.0e-9);   // not vibrato any more

    // Through the processor: the setting reaches the interpreter on the audio
    // thread and travels with the session.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    processor.setAftertouchBends (true);
    renderBlocks (processor, 1);
    CHECK (processor.getEngine().getMidiInterpreter().getAftertouchTarget() == MidiTarget::PitchBend);

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    LuthierAudioProcessor restored;
    restored.prepareToPlay (kSr, kBlock);
    restored.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (restored.doesAftertouchBend());
}

/*  CT-11 (controllers 3): the latency wizard is fed by notes arriving at the
    processor, measured against the metronome click on the audio thread. It
    used never to receive a measurement. */
LUTHIER_TEST (Controllers, theWizardCompletesFromNotesThroughTheProcessor)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& metronome = processor.getMetronome();
    metronome.setTempo (100.0);
    metronome.setEnabled (true);
    processor.setLatencyWizardListening (true);

    LatencyWizard wizard;
    wizard.begin (10);

    const double samplesPerBeat = 60.0 / 100.0 * kSr;
    const int lateBy = (int) (0.010 * kSr);   // the player is 10 ms behind the click

    juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumInputChannels(),
                                                 processor.getTotalNumOutputChannels()), kBlock);
    int pendingNoteAt = -1;   // samples from now
    int note = 40;

    for (int block = 0; block < 2000 && wizard.isRunning(); ++block)
    {
        juce::MidiBuffer midi;

        if (pendingNoteAt < 0)
        {
            const double phase = metronome.getBeatPhase();
            pendingNoteAt = (int) std::lround ((1.0 - phase) * samplesPerBeat) + lateBy;
        }

        if (pendingNoteAt < kBlock)
        {
            midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), pendingNoteAt);
            note = note == 40 ? 45 : 40;
            pendingNoteAt = -1;
        }
        else
        {
            pendingNoteAt -= kBlock;
        }

        buffer.clear();
        processor.processBlock (buffer, midi);
        processor.drainLatencyMeasurements (wizard);
    }

    CHECK_MSG (! wizard.isRunning(), "the wizard only heard " + juce::String (wizard.getNumMeasurements()) + " notes");
    CHECK_NEAR (wizard.getMeasuredLatencyMs(), 10.0, 0.5);
    CHECK (wizard.isReliable());

    processor.setLatencyWizardListening (false);
}

/*  CT-9 (controllers 1): the LinnStrument profile with rows-as-strings on puts
    one row per channel onto the strings, high to low, bending over 48. */
LUTHIER_TEST (Controllers, linnstrumentGuitarModeMapsRowsToStrings)
{
    auto linn = profileById ("linnstrument");
    CHECK (linn.isValid());
    linn.rowsAsStrings = true;

    MpeFixture f;
    ControllerProfileLibrary::apply (linn, f.interpreter);

    CHECK (! f.interpreter.isMpeEnabled());
    CHECK (f.interpreter.getPlayingMode() == PlayingMode::GuitarController);

    for (int s = 0; s < 6; ++s)
        CHECK (f.interpreter.getChannelForString (s) == s + 1);

    CHECK_NEAR (f.interpreter.getPitchBendRange(), linn.memberPitchBendSemis, 1.0e-9);

    // Row 3 (channel 3) plays string 2 whatever the pitch.
    CHECK (f.noteOn (3, 55) == 2);
    const auto out = f.send (juce::MidiMessage::pitchWheel (3, 8192 + 4096));
    CHECK (out.getNumBends() == 1);

    if (out.getNumBends() == 1)
        CHECK_NEAR (out.getBend (0).cents, 0.5 * linn.memberPitchBendSemis * 100.0, 1.0);
}
