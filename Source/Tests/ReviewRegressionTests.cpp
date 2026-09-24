/*  Regression tests for bugs found in code review (docs/review/FINDINGS.md).
    Each test names the finding it pins down. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../DSP/Amp/ToneStack.h"
#include "../DSP/Effects/PedalsDrive.h"
#include "../DSP/Effects/PedalsMod.h"
#include "../DSP/Amp/RoomEngine.h"
#include "../Model/Playing/MidiInterpreter.h"
#include "../Practice/Looper.h"
#include "../Notation/NotationExport.h"
#include "../DSP/Whammy/WhammyEngine.h"
#include "../DSP/Common/Oversampler.h"
#include <complex>
#include "../Model/Playing/TechniqueEngine.h"
#include "../Model/Playing/RubricVoicer.h"
#include "../Model/Playing/TuningEngine.h"

using namespace luthier;
using namespace luthier::tests;

//==============================================================================
/*  R-001: a host may hand processBlock more samples than it promised in
    prepareToPlay. The engine split such a block, but the processor's own
    scratch buffers (the click, the tune click, the backing track, the monitor)
    were sized from the promise and written for the whole block. */
LUTHIER_TEST (ReviewRegression, aBlockBiggerThanPreparedIsRenderedWhole)
{
    constexpr int kPrepared = 128;
    constexpr int kHostBlock = 1000;

    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, kPrepared);
    processor.setPracticePanelOpen (true);
    processor.getMetronome().setEnabled (true);
    processor.setClickToMain (true);

    juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(),
                                                 processor.getTotalNumInputChannels()), kHostBlock);
    bool heardLate = false;

    for (int block = 0; block < 8; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        // A note after the prepared size must still be played, at its own time.
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 110), 700);

        processor.processBlock (buffer, midi);

        CHECK_FINITE (buffer.getReadPointer (0), kHostBlock);

        if (block == 0)
        {
            // Nothing from the note before its own sample: it was not moved earlier.
            heardLate = buffer.getMagnitude (0, kPrepared * 5, kHostBlock - kPrepared * 5) > 0.0f;
        }
    }

    CHECK (heardLate);
}

//==============================================================================
/*  R-006: the tone stack's b3 term had t*C1C2C3R1R2R4 where Yeh's derivation has
    t*l*C1C2C3R1R2R4. A passive network cannot have gain above unity; with the bass
    at zero the typo gave up to +28 dB of treble. */
namespace
{
    double toneStackGainAt (double bass, double mid, double treble, double hz)
    {
        constexpr double sr = 48000.0;
        luthier::ToneStack stack;
        stack.prepare (sr);
        stack.setControls (bass, mid, treble);

        double peak = 0.0;
        const int n = (int) sr;

        for (int i = 0; i < n; ++i)
        {
            const double y = stack.process (std::sin (juce::MathConstants<double>::twoPi * hz * i / sr));

            if (i > n / 2)
                peak = std::max (peak, std::abs (y));
        }

        return peak;
    }
}

LUTHIER_TEST (ReviewRegression, theToneStackIsPassiveAtEverySetting)
{
    // ToneStack::process applies a fixed 6.5x insertion-loss makeup; the
    // network itself must stay at or below unity.
    constexpr double makeup = 6.5;

    for (double bass : { 0.0, 0.5, 1.0 })
        for (double treble : { 0.0, 0.5, 1.0 })
            for (double hz : { 100.0, 1000.0, 5000.0, 10000.0 })
                CHECK_MSG (toneStackGainAt (bass, 0.5, treble, hz) / makeup <= 1.05,
                           "bass " + juce::String (bass) + " treble " + juce::String (treble)
                               + " at " + juce::String (hz) + " Hz");
}

//==============================================================================
/*  R-007: the pitch shifter advanced its read delay by (ratio - 1) per sample,
    which plays at 2 - ratio: a fifth up came out a fourth down, and +12 froze. */
namespace
{
    double shiftedFrequency (double semitones)
    {
        constexpr double sr = 48000.0, inHz = 220.0;
        luthier::PitchShifterPedal pedal;
        pedal.prepare (sr, 512);
        pedal.setParameterValue (0, semitones);
        pedal.setParameterValue (2, 1.0);   // all wet
        pedal.reset();

        const int n = (int) sr;
        std::vector<double> l ((size_t) n), r ((size_t) n);

        for (int i = 0; i < n; ++i)
            l[(size_t) i] = r[(size_t) i] = std::sin (juce::MathConstants<double>::twoPi * inHz * i / sr);

        for (int i = 0; i < n; i += 512)
            pedal.process (l.data() + i, r.data() + i, juce::jmin (512, n - i));

        // Upward zero crossings over the second half.
        int crossings = 0;

        for (int i = n / 2 + 1; i < n; ++i)
            if (l[(size_t) i - 1] < 0.0 && l[(size_t) i] >= 0.0)
                ++crossings;

        return crossings / 0.5;
    }
}

LUTHIER_TEST (ReviewRegression, thePitchShifterShiftsTheWayItSays)
{
    const double up = shiftedFrequency (7.0);
    const double down = shiftedFrequency (-7.0);

    CHECK_MSG (std::abs (up / (220.0 * std::pow (2.0, 7.0 / 12.0)) - 1.0) < 0.08, "up: " + juce::String (up));
    CHECK_MSG (std::abs (down / (220.0 * std::pow (2.0, -7.0 / 12.0)) - 1.0) < 0.08, "down: " + juce::String (down));
}

//==============================================================================
/*  R-008 / R-009: the parameter bridge re-sends every parameter every block. The
    Reverb pedal rebuilt (and zeroed) its delay lines on every Size or Character
    send, and the room engine on every decay-scale send, so with blocks shorter
    than the shortest line no late reverb was ever heard. */
LUTHIER_TEST (ReviewRegression, theReverbPedalTailSurvivesRepeatedParameterSends)
{
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    luthier::ReverbPedal pedal;
    pedal.prepare (sr, block);
    pedal.setParameterValue (5, 1.0);   // mix

    std::vector<double> l ((size_t) block), r ((size_t) block);
    double lateEnergy = 0.0;

    for (int b = 0; b < 40; ++b)
    {
        // What ParameterBridge::applyToEngine does every block.
        for (int p = 0; p < pedal.getNumParameters(); ++p)
            pedal.setParameterNormalised (p, pedal.getParameterNormalised (p));

        std::fill (l.begin(), l.end(), 0.0);
        std::fill (r.begin(), r.end(), 0.0);

        if (b == 0)
            l[0] = r[0] = 1.0;

        pedal.process (l.data(), r.data(), block);

        if (b >= 20)   // well past the input and the pre-delay
            for (auto v : l)
                lateEnergy += v * v;
    }

    CHECK_MSG (lateEnergy > 1.0e-8, "late energy " + juce::String (lateEnergy));
}

LUTHIER_TEST (ReviewRegression, theRoomTailSurvivesRepeatedDecaySends)
{
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    luthier::RoomEngine room;
    room.prepare (sr, block);
    room.setEnabled (true);
    room.setRoomSize (luthier::RoomSize::LiveRoom);
    room.setRoomBlend (1.0);
    room.reset();

    juce::AudioBuffer<float> buffer (2, block);
    double lateEnergy = 0.0;

    for (int b = 0; b < 220; ++b)
    {
        room.setDecayScale (1.0);   // every block, as the bridge does
        buffer.clear();

        if (b == 0)
            buffer.setSample (0, 0, 1.0f), buffer.setSample (1, 0, 1.0f);

        room.processBlock (buffer);

        if (b >= 180)   // past every early reflection (0.68 s at most)
            lateEnergy += buffer.getRMSLevel (0, 0, block);
    }

    CHECK_MSG (lateEnergy > 1.0e-7, "late energy " + juce::String (lateEnergy));
}

//==============================================================================
/*  R-012: in Poly mode a note waits in the chord window before it is voiced. A
    note-off arriving inside that window looked only at the voiced strings, found
    nothing and was dropped; the note then sounded and was never released. */
LUTHIER_TEST (ReviewRegression, aNoteReleasedInsideTheChordWindowIsReleased)
{
    constexpr double sr = 48000.0;
    constexpr int block = 256;

    TuningEngine tuning;
    tuning.prepare (sr);
    tuning.setNumStrings (6);
    tuning.setTuningPreset (TuningPreset::Standard);

    RubricVoicer voicer;
    voicer.prepare (&tuning, 6);
    TechniqueEngine technique;
    technique.prepare (sr, 6);

    MidiInterpreter interpreter;
    interpreter.prepare (sr, 6);
    interpreter.setEngines (&tuning, &technique, &voicer);
    interpreter.setNumStrings (6);
    interpreter.setPlayingMode (PlayingMode::Poly);
    interpreter.setChordWindowMs (20.0);

    MidiInterpreter::Humanisation flat;
    flat.amount = 0.0;
    interpreter.setHumanisation (flat);

    int ons = 0, offs = 0;
    int64_t start = 0;

    for (int b = 0; b < 40; ++b, start += block)
    {
        juce::MidiBuffer midi;

        // A 2 ms stab near the end of the first block: both inside the window.
        if (b == 0)
        {
            midi.addEvent (juce::MidiMessage::noteOn (1, 64, 0.8f), 200);
            midi.addEvent (juce::MidiMessage::noteOff (1, 64), 250);
        }

        PlayEventQueue out;
        out.clear();
        interpreter.processBlock (midi, block, start, out);
        ons += out.getNumNoteOns();
        offs += out.getNumNoteOffs();
    }

    CHECK_MSG (ons == 1, "note-ons: " + juce::String (ons));
    CHECK_MSG (offs == 1, "note-offs: " + juce::String (offs));
    CHECK (interpreter.getActiveNoteCount() == 0);
}

/*  R-013: in guitar-controller mode a note-off released the first string holding
    that note number, whatever channel it came on. */
LUTHIER_TEST (ReviewRegression, aControllerNoteOffReleasesItsOwnString)
{
    constexpr double sr = 48000.0;
    constexpr int block = 256;

    TuningEngine tuning;
    tuning.prepare (sr);
    tuning.setNumStrings (6);
    tuning.setTuningPreset (TuningPreset::Standard);
    RubricVoicer voicer;
    voicer.prepare (&tuning, 6);
    TechniqueEngine technique;
    technique.prepare (sr, 6);

    MidiInterpreter interpreter;
    interpreter.prepare (sr, 6);
    interpreter.setEngines (&tuning, &technique, &voicer);
    interpreter.setNumStrings (6);
    interpreter.setPlayingMode (PlayingMode::GuitarController);
    MidiInterpreter::Humanisation flat;
    flat.amount = 0.0;
    interpreter.setHumanisation (flat);

    // E4 open on the high E (channel 1) and at the fifth fret of the B (channel 2).
    juce::MidiBuffer on;
    on.addEvent (juce::MidiMessage::noteOn (1, 64, 0.8f), 0);
    on.addEvent (juce::MidiMessage::noteOn (2, 64, 0.8f), 0);
    PlayEventQueue out;
    out.clear();
    interpreter.processBlock (on, block, 0, out);
    CHECK (out.getNumNoteOns() == 2);

    juce::MidiBuffer off;
    off.addEvent (juce::MidiMessage::noteOff (2, 64), 0);
    out.clear();
    interpreter.processBlock (off, block, block * 40, out);

    CHECK (out.getNumNoteOffs() == 1);
    CHECK_MSG (out.getNumNoteOffs() == 1 && out.getNoteOff (0).stringIndex == 1,
               "released string " + juce::String (out.getNoteOff (0).stringIndex));
}

//==============================================================================
/*  R-016 / R-017: overdubbing recorded the block into the active layer and then
    played every layer back, the active one included, so the player heard their
    live signal twice; and the overdub was written at unwrapped positions, so
    whatever was played after the loop end went past the loop and was never
    heard. */
LUTHIER_TEST (ReviewRegression, anOverdubIsHeardOnceAndWrapsWithTheLoop)
{
    constexpr int block = 512;
    Looper looper;
    looper.prepare (48000.0, 10.0);

    juce::AudioBuffer<float> buffer (2, block);
    int64_t t = 0;

    // 1 kHz at 48 kHz: a 48-sample period, which divides the 21-block loop, so
    // every pass of the overdub lands in phase with the last.
    auto render = [&] (int blocks, float amplitude, int n = block)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < n; ++i, ++t)
                buffer.setSample (0, i, amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * (double) (t % 48) / 48.0)),
                buffer.setSample (1, i, buffer.getSample (0, i));

            looper.processBlock (buffer, n);
        }
    };

    // A silent first layer sets the loop length.
    looper.press();
    render (20, 0.0f);
    looper.press();
    render (1, 0.0f);
    CHECK (looper.getState() == Looper::State::playing);

    const int length = looper.getLoopLengthSamples();
    CHECK_MSG (length % 48 == 0, "loop length " + juce::String (length));

    // Into the loop a little, so the overdub starts mid-loop.
    render (3, 0.0f);

    // While overdubbing, what is heard is the live signal once.
    looper.press();
    CHECK (looper.getState() == Looper::State::overdubbing);
    render (4, 0.2f, 500);
    CHECK_NEAR (buffer.getRMSLevel (0, 0, 500), 0.2f / std::sqrt (2.0f), 0.01f);

    // One and a half loops in all, in blocks that do not tile the loop, so one
    // of them straddles its end; then play back with no input.
    render ((length * 3 / 2) / 500 - 4, 0.2f, 500);
    looper.press();
    CHECK (looper.getState() == Looper::State::playing);

    float quietest = 1.0e9f;

    for (int b = 0; b < length / block; ++b)
    {
        buffer.clear();
        looper.processBlock (buffer, block);

        for (int period = 0; period + 48 <= block; period += 48)   // one sine period each
            quietest = juce::jmin (quietest, buffer.getRMSLevel (0, period, 48));
    }

    CHECK_MSG (quietest > 0.1f, "a hole in the overdub: period RMS " + juce::String (quietest));
}

//==============================================================================
/*  R-207: Capture::processBlock had no caller, so a capture never progressed and
    every tone-match wizard waited at "Recording..." for ever. */
LUTHIER_TEST (ReviewRegression, aCaptureRecordsTheMainOutput)
{
    constexpr int block = 256;
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, block);

    auto& capture = processor.getCapture();
    capture.setSource (Capture::Source::mainOut);
    capture.start (0.2);

    juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(),
                                                 processor.getTotalNumInputChannels()), block);

    for (int b = 0; b < 60 && ! capture.isComplete(); ++b)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (b == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 110), 0);

        processor.processBlock (buffer, midi);
    }

    CHECK (capture.isComplete());
    CHECK (capture.getRecordedSamples() > 0);
    CHECK (capture.getBuffer().getMagnitude (0, 0, capture.getRecordedSamples()) > 0.0f);
}

//==============================================================================
/*  R-208 / R-209: MusicXML numbers <string> from 1 = the highest string. The
    exporter wrote numStrings - index (and the importer undid it), so every
    other program put each note on the mirrored string. And an imported
    <chord/> note started after the note before it instead of with it. */
LUTHIER_TEST (ReviewRegression, musicXmlStringsCountFromTheHighestAndChordsStayTogether)
{
    PerformanceScore score;
    score.beginCapture (120.0, 4, 4);

    // An open high E (string index 0) and, a beat later, a two-note chord.
    score.noteStarted (0, 0, 64, 329.63, 0.8, 0.0);
    score.noteEnded (0, 1.0);
    score.noteStarted (5, 3, 43, 98.0, 0.8, 1.0);
    score.noteStarted (4, 2, 47, 123.47, 0.8, 1.0);
    score.noteEnded (5, 2.0);
    score.noteEnded (4, 2.0);
    score.endCapture (4.0);

    NotationExporter exporter;
    const auto xml = exporter.renderMusicXml (score);

    // The high E's note carries <string>1</string>, the low E's <string>6</string>.
    const int highE = xml.indexOf ("<pitch>");
    CHECK (highE >= 0);
    CHECK_MSG (xml.fromFirstOccurrenceOf ("<string>", false, false).upToFirstOccurrenceOf ("</string>", false, false) == "1",
               "the high E was exported as string "
                 + xml.fromFirstOccurrenceOf ("<string>", false, false).upToFirstOccurrenceOf ("</string>", false, false));

    NotationImporter importer;
    PerformanceScore back;
    CHECK (importer.readMusicXml (xml, back));

    const auto notes = back.getTrack (0).measures[0].collectNotes();
    CHECK (notes.size() == 3);

    int chordNotesAtBeatOne = 0;

    for (const auto* n : notes)
        if (std::abs (n->startBeat - 1.0) < 1.0e-6)
            ++chordNotesAtBeatOne;

    CHECK_MSG (chordNotesAtBeatOne == 2, juce::String (chordNotesAtBeatOne) + " chord notes at beat 1");
}

//==============================================================================
/*  R-204: a preset without a "strings" block (every factory preset) left the
    previous preset's per-string detune, gauges and temperament in place, and
    one without a "midiMap" block kept the previous CC map. */
LUTHIER_TEST (ReviewRegression, aPresetWithoutAStringsBlockClearsThePreviousDetune)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    auto& presets = processor.getPresetManager();
    presets.getExtraState().detuneCents[0] = 35.0;
    presets.applyExtraState();
    CHECK_NEAR (processor.getEngine().getTuningEngine().getStringTuning (0).detuneCents, 35.0, 1.0e-9);

    auto* root = new juce::DynamicObject();
    root->setProperty ("magic", PresetManager::kMagic);
    root->setProperty ("schemaVersion", PresetManager::kSchemaVersion);
    root->setProperty ("name", "No strings block");
    root->setProperty ("parameters", juce::var (new juce::DynamicObject()));

    CHECK (presets.fromVar (juce::var (root)));
    presets.applyExtraState();

    CHECK_NEAR (processor.getEngine().getTuningEngine().getStringTuning (0).detuneCents, 0.0, 1.0e-9);
}

//==============================================================================
/*  R-205: ModMatrix::prepare reset every step sequencer's steps and every LFO's
    custom shape to the defaults. Hosts restore state before prepareToPlay, and
    prepare again on a rate change or an offline bounce, so a programmed
    sequence came back as the default ramp. */
LUTHIER_TEST (ReviewRegression, prepareKeepsTheSequencerSteps)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    auto& sequencer = processor.getModMatrix().getSequencer (0);
    auto step = sequencer.getStep (3);
    step.value = 0.123;
    sequencer.setStep (3, step);

    processor.prepareToPlay (96000.0, 512);

    CHECK_NEAR (processor.getModMatrix().getSequencer (0).getStep (3).value, 0.123, 1.0e-9);
}

//==============================================================================
/*  R-216: a click fires when the position crosses an integer, and the position
    started at exactly 0, so beat one never sounded: the first click was beat
    two, a beat late, unaccented. */
LUTHIER_TEST (ReviewRegression, theMetronomeStartsOnBeatOne)
{
    Metronome metronome;
    metronome.prepare (48000.0, 512);
    metronome.setTempo (120.0);
    metronome.setEnabled (true);

    std::vector<float> out (512);
    metronome.processBlock (out.data(), 512);

    // A click in the first 10 ms, not half a second later.
    float early = 0.0f;

    for (int i = 0; i < 480; ++i)
        early = juce::jmax (early, std::abs (out[(size_t) i]));

    CHECK_MSG (early > 1.0e-3f, "no click at the start: " + juce::String (early));
}

//==============================================================================
/*  R-222: a Fixed bridge zeroed the whammy range, but the parameter bridge sets
    the user's ranges every block, so a hardtail still bent with the arm. */
LUTHIER_TEST (ReviewRegression, aHardtailIgnoresTheWhammyRanges)
{
    WhammyEngine whammy;
    whammy.prepare (48000.0, 6);
    whammy.setBridgeType (WhammyEngine::BridgeType::Fixed);
    whammy.setRange (2.0, 1.0);   // what the bridge sends every block

    CHECK (whammy.getDownRange() == 0.0);
    CHECK (whammy.getUpRange() == 0.0);
}

//==============================================================================
/*  R-201: MIDI Learn read its mapping array on the audio thread without the
    lock the message thread held while clearing or reallocating it, looked the
    parameter up by String id there, and finished a learn with a callAsync that
    captured a raw this. The audio thread now reads a plain table under a
    try-lock; a learn is finished by the manager's own AsyncUpdater. */
LUTHIER_TEST (ReviewRegression, midiLearnLearnsAppliesAndSurvivesAClear)
{
    LuthierAudioProcessor processor;
    auto& learn = processor.getMidiLearn();
    auto* target = processor.getParameters()[0];
    auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (target);
    CHECK (withId != nullptr);

    if (withId == nullptr)
        return;

    learn.startLearning (withId->paramID);

    juce::MidiBuffer cc;
    cc.addEvent (juce::MidiMessage::controllerEvent (1, 21, 64), 0);
    learn.processMidi (cc);

    // The message thread finishes the learn.
    learn.dispatchPendingLearn();
    CHECK_MSG (learn.getCcForParameter (withId->paramID) == 21, "the CC was not learned");

    juce::MidiBuffer full;
    full.addEvent (juce::MidiMessage::controllerEvent (1, 21, 127), 0);
    learn.processMidi (full);
    CHECK_NEAR (target->getValue(), 1.0f, 1.0e-6f);

    // Cleared, the CC no longer reaches the parameter (and nothing indexes an
    // empty array).
    learn.clearAllMappings();
    juce::MidiBuffer zero;
    zero.addEvent (juce::MidiMessage::controllerEvent (1, 21, 0), 0);
    learn.processMidi (zero);
    CHECK_NEAR (target->getValue(), 1.0f, 1.0e-6f);
}

//==============================================================================
/*  R-210: two strings in unison exchange energy through the coupling matrix,
    wired the way LuthierEngine wires it. The matrix added each string's motion
    to the other without taking it from the sender, so at the default amount
    the pair grew to the +-4 guard and stayed there. A passive bridge cannot
    add energy: the pair must decay. */
#include "../DSP/String/StringEngine.h"
#include "../DSP/Coupling/CouplingMatrix.h"

namespace
{
    std::vector<double> coupledPairEnvelope (double amount, double hzA, double hzB)
    {
        constexpr double sr = 48000.0;
        StringEngine strings[2];
        CouplingMatrix coupling;
        coupling.prepare (sr, 2);
        coupling.setAmount (amount);
        coupling.buildDefault (0.020);

        const double hz[2] = { hzA, hzB };

        for (int s = 0; s < 2; ++s)
        {
            strings[s].prepare (sr, 512);
            strings[s].setIndex (s);
            strings[s].snapToFrequency (hz[s]);
            coupling.setStringFrequency (s, hz[s]);
        }

        Excitation::Params pluck;
        pluck.delaySamples = sr / hzA;
        strings[0].excite (pluck);

        double bridge[kMaxStrings] {}, in[kMaxStrings] {};
        std::vector<double> perSecond;
        double peak = 0.0;

        for (int i = 0; i < (int) sr * 8; ++i)
        {
            coupling.process (bridge, in);

            for (int s = 0; s < 2; ++s)
            {
                const double out = strings[s].processSample (in[s]);
                bridge[s] = strings[s].getBridgeOutput();
                peak = std::max (peak, std::abs (out));
            }

            if ((i + 1) % (int) sr == 0)
            {
                perSecond.push_back (peak);
                peak = 0.0;
            }
        }

        return perSecond;
    }
}

LUTHIER_TEST (ReviewRegression, aUnisonPairDecays)
{
    const auto coupled = coupledPairEnvelope (0.85, 329.63, 329.63);
    const auto alone = coupledPairEnvelope (0.0, 329.63, 329.63);

    juce::String trace;

    for (size_t i = 0; i < coupled.size(); ++i)
        trace << juce::String (coupled[i], 4) << "/" << juce::String (alone[i], 4) << " ";

    CHECK_MSG (coupled.back() < coupled.front() * 0.1, "coupled/alone per second: " + trace);

    // Three cents apart ran away too; well apart, the sympathetic ring is kept.
    const auto nearly = coupledPairEnvelope (0.85, 329.63, 329.63 * std::pow (2.0, 3.0 / 1200.0));
    CHECK (nearly.back() < nearly.front() * 0.1);
}


//==============================================================================
/*  R-211: the half-band branches run at the base rate but used a two-sample
    all-pass memory, i.e. A(z^4) at the oversampled rate: weaker image
    rejection, passband droop, and a round trip that delayed about twice what
    getLatencySamples tells the host. The delay near DC now matches the report. */
LUTHIER_TEST (ReviewRegression, theOversamplerDelaysWhatItReports)
{
    constexpr double sr = 48000.0;

    for (int factor : { 2, 4, 8 })
    {
        Oversampler os;
        os.prepare (sr, factor);

        std::vector<double> h (4096);

        for (size_t n = 0; n < h.size(); ++n)
            h[n] = os.processSample (n == 0 ? 1.0 : 0.0, [] (double x) { return x; });

        auto phaseAt = [&h] (double hz)
        {
            std::complex<double> sum;

            for (size_t n = 0; n < h.size(); ++n)
                sum += h[n] * std::polar (1.0, -juce::MathConstants<double>::twoPi * hz / sr * (double) n);

            return std::arg (sum);
        };

        double dphi = phaseAt (150.0) - phaseAt (50.0);

        while (dphi > 0.0)
            dphi -= juce::MathConstants<double>::twoPi;

        const double delay = -dphi / (juce::MathConstants<double>::twoPi * 100.0 / sr);

        CHECK_MSG (std::abs (delay - os.getLatencySamples()) < 1.0,
                   juce::String (factor) + "x delays " + juce::String (delay, 2) + " samples, reports "
                     + juce::String (os.getLatencySamples()));
    }
}

/*  R-035: setFactor cleared the half-band filters even when the factor did not
    change, and the amp re-sends it on every structural change: a click. */
LUTHIER_TEST (ReviewRegression, resendingTheSameOversamplingFactorDoesNotReset)
{
    Oversampler a, b;
    a.prepare (48000.0, 4);
    b.prepare (48000.0, 4);

    double maxDiff = 0.0;

    for (int i = 0; i < 2000; ++i)
    {
        if (i == 1000)
            b.setFactor (4);

        const double x = std::sin (0.05 * i);
        const double ya = a.processSample (x, [] (double v) { return v; });
        const double yb = b.processSample (x, [] (double v) { return v; });
        maxDiff = std::max (maxDiff, std::abs (ya - yb));
    }

    CHECK_MSG (maxDiff == 0.0, "diverged by " + juce::String (maxDiff));
}

/*  R-036: snapshot recall and preset morph blended integer parameters (string
    bitmasks, CC numbers) like continuous ones, passing through unrelated masks
    and controllers mid-fade. */
LUTHIER_TEST (ReviewRegression, integerParametersSwitchRatherThanBlend)
{
    juce::AudioParameterInt mask (juce::ParameterID { "mask", 1 }, "Mask", 0, 4095, 0);
    CHECK (SnapshotBank::isDiscrete (mask));

    juce::AudioParameterFloat level (juce::ParameterID { "level", 1 }, "Level", 0.0f, 1.0f, 0.5f);
    CHECK (! SnapshotBank::isDiscrete (level));
}

/*  R-037: a tempo-synced LFO retriggered on the sync boundary free-runs when the
    host gives no position, but its phase was never wrapped, so it grew without
    bound and sample-and-hold never picked a new value. */
#include "../Modulation/ModSources.h"

LUTHIER_TEST (ReviewRegression, aSyncedLfoWithTheTransportStoppedStillCycles)
{
    ModLfo lfo;
    lfo.prepare (1000.0, 7);
    lfo.setShape (ModLfo::Shape::sampleAndHold);
    lfo.setSynced (true);
    lfo.setRetrigger (ModLfo::Retrigger::onSyncBoundary);

    juce::SortedSet<double> values;

    for (int i = 0; i < 20000; ++i)
        values.add (lfo.tick (0.01, -1.0));   // no host position

    CHECK_MSG (values.size() > 2, juce::String (values.size()) + " distinct values");
}

/*  R-038: Capture::autoTrim returned early whenever there was no leading
    silence, so a capture that started on the sound kept its silent tail. */
LUTHIER_TEST (ReviewRegression, autoTrimTrimsTheTailWithoutLeadingSilence)
{
    Capture capture;
    capture.prepare (48000.0, 1.0);
    capture.start (0.5);

    std::vector<float> block (24000, 0.0f);

    for (int i = 0; i < 4800; ++i)
        block[(size_t) i] = 0.5f;   // sound from the first sample, then silence

    const float* channels[] = { block.data() };
    capture.processBlock (channels, 1, (int) block.size());
    CHECK (capture.getRecordedSamples() == 24000);

    capture.autoTrim();
    CHECK_MSG (capture.getRecordedSamples() == 4800, juce::String (capture.getRecordedSamples()));
}
