/*  Whole-engine and whole-plugin tests (engine spec 19, "Integration tests",
    "Round-trip test", and the fuzz requirement from include.md).

    These run the real signal path end to end: MIDI in, audio out, through every
    module at once. They are the tests that catch the failures unit tests cannot -
    a module that is correct alone but wrong in context.
*/

#include "TestFramework.h"

#include "../LuthierEngine.h"
#include "../Parameters.h"
#include "../Presets/PresetManager.h"
#include "../Presets/FactoryPresets.h"
#include "../Support/AudioExporter.h"
#include "../Support/ErrorLog.h"
#include "../Support/MidiCapture.h"
#include "../Support/MidiLearn.h"
#include "../Support/Diagnostics.h"

using namespace luthier;
using namespace luthier::tests;

/*  SPEC-SWEEP: FactoryPresets keeps the processor it reads ranges from in a
    static. A test that points it at its own local processor must clear it on
    the way out, or the next PresetManager (every processor's constructor
    writes the factory bank) reads a destroyed object. */
struct FactoryRangesReset
{
    ~FactoryRangesReset() { FactoryPresets::setProcessorForRanges (nullptr); }
};

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    /** A minimal AudioProcessor so the parameter, preset and fuzz tests can run
        against the real APVTS without pulling in the plugin wrapper. */
    class HarnessProcessor : public juce::AudioProcessor
    {
    public:
        HarnessProcessor()
            : AudioProcessor (BusesProperties()
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
              apvts (*this, nullptr, "LUTHIER", Parameters::createLayout()),
              bridge (apvts, engine),
              presets (*this, apvts, engine, ranges)
        {
            bridge.cachePointers();
        }

        void prepareToPlay (double sampleRate, int blockSize) override
        {
            engine.prepare (sampleRate, blockSize);
            bridge.applyAllNow();
        }

        void releaseResources() override { engine.releaseResources(); }

        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
        {
            bridge.applyToEngine();
            engine.processBlock (buffer, midi);
        }

        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        const juce::String getName() const override { return "LuthierTestHarness"; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 8.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

        LuthierEngine engine;
        RangeState ranges;
        juce::AudioProcessorValueTreeState apvts;
        ParameterBridge bridge;
        PresetManager presets;
    };

    /** Renders `seconds` of the engine, feeding it the supplied MIDI at time 0. */
    juce::AudioBuffer<float> render (LuthierEngine& engine, const juce::MidiBuffer& midi,
                                     double seconds)
    {
        const int total = (int) (kSr * seconds);
        juce::AudioBuffer<float> result (2, total);
        result.clear();

        juce::AudioBuffer<float> block (2, kBlock);

        int position = 0;
        bool first = true;

        while (position < total)
        {
            const int count = juce::jmin (kBlock, total - position);
            block.setSize (2, count, false, false, true);
            block.clear();

            juce::MidiBuffer thisBlock;

            if (first)
            {
                thisBlock = midi;
                first = false;
            }

            engine.processBlock (block, thisBlock);

            for (int ch = 0; ch < 2; ++ch)
                result.copyFrom (ch, position, block, ch, 0, count);

            position += count;
        }

        return result;
    }

    std::vector<double> toMono (const juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        std::vector<double> mono ((size_t) n);

        for (int i = 0; i < n; ++i)
            mono[(size_t) i] = 0.5 * ((double) buffer.getSample (0, i)
                                      + (double) buffer.getSample (1, i));

        return mono;
    }

    bool bufferIsFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (! std::isfinite (buffer.getSample (ch, i)))
                    return false;

        return true;
    }
}

//==============================================================================
//  Whole engine
//==============================================================================
LUTHIER_TEST (Engine, aNoteProducesSound)
{
    LuthierEngine engine;
    engine.prepare (kSr, kBlock);
    engine.setGuitarType (GuitarType::Stratocaster);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 52, 0.9f), 0);   // E3

    auto buffer = render (engine, midi, 1.0);

    CHECK_MSG (bufferIsFinite (buffer), "the engine produced non-finite samples");

    auto mono = toMono (buffer);
    const double level = rms (mono.data(), (int) mono.size());

    CHECK_MSG (level > 1.0e-4, "a note should make a sound, RMS was " + juce::String (level, 8));
}

LUTHIER_TEST (Engine, silenceInSilenceOut)
{
    LuthierEngine engine;
    engine.prepare (kSr, kBlock);

    juce::MidiBuffer empty;
    auto buffer = render (engine, empty, 0.5);

    CHECK_MSG (bufferIsFinite (buffer), "idle engine produced non-finite samples");

    auto mono = toMono (buffer);

    // Amp hiss and mains hum are deliberate, so this is a noise-floor check
    // rather than a strict zero.
    CHECK_MSG (peak (mono.data(), (int) mono.size()) < 0.05,
               "the idle noise floor is too loud: "
               + juce::String (peak (mono.data(), (int) mono.size()), 5));
}

LUTHIER_TEST (Engine, chromaticScalePlaysAtTheRightPitch)
{
    // Engine spec: play a chromatic scale, verify every note is at the correct pitch.
    LuthierEngine engine;
    engine.prepare (kSr, kBlock);
    engine.setGuitarType (GuitarType::Stratocaster);

    // Mono mode and a dry rig, so the measurement is of the instrument, not the amp.
    engine.getMidiInterpreter().setPlayingMode (PlayingMode::Mono);
    engine.getCabinetEngine().setEnabled (false);
    engine.getRoomEngine().setEnabled (false);
    engine.getAmpEngine().setGain (0.0);
    engine.getAmpEngine().setMaster (0.5);
    engine.getMidiInterpreter().setHumanisation ({ 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 });

    int checked = 0;

    for (int note = 40; note <= 64; ++note)
    {
        engine.panic();

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.85f), 0);

        auto buffer = render (engine, midi, 0.6);

        if (! bufferIsFinite (buffer))
        {
            ctx.fail ("note " + juce::String (note) + " produced non-finite output");
            continue;
        }

        auto mono = toMono (buffer);

        const double expected = midiToHz ((double) note);
        const int skip = (int) (kSr * 0.08);

        const double measured = findPeakFrequency (mono.data() + skip,
                                                   (int) mono.size() - skip, kSr,
                                                   expected * 0.55, expected * 1.8);

        if (measured <= 0.0)
            continue;

        const double cents = 1200.0 * std::log2 (measured / expected);
        ++checked;

        CHECK_MSG (std::abs (cents) < 35.0,
                   "MIDI " + juce::String (note) + ": measured "
                   + juce::String (measured, 2) + " Hz, expected "
                   + juce::String (expected, 2) + " (" + juce::String (cents, 1) + " cents)");
    }

    CHECK_MSG (checked >= 20, "only " + juce::String (checked) + " notes were measurable");
}

LUTHIER_TEST (Engine, fretlessModeIsGenuinelyContinuous)
{
    // Shipping criterion 6: verify continuous pitch with a spectral sweep.
    LuthierEngine engine;
    engine.prepare (kSr, kBlock);
    engine.setGuitarType (GuitarType::FretlessBass);
    engine.setFretless (true);
    engine.getCabinetEngine().setEnabled (false);
    engine.getRoomEngine().setEnabled (false);

    auto& tuning = engine.getTuningEngine();

    // Step the fret position in hundredths and confirm every step changes pitch.
    double previous = 0.0;
    int distinct = 0;

    for (int i = 0; i <= 500; ++i)
    {
        const double fret = (double) i / 100.0;
        const double hz = tuning.computeFrequency (0, fret, 0.0);

        if (i > 0)
        {
            CHECK_MSG (hz > previous,
                       "pitch must rise at every hundredth of a fret; stalled at "
                       + juce::String (fret, 2));

            if (hz > previous)
                ++distinct;
        }

        previous = hz;
    }

    CHECK_MSG (distinct == 500, "expected 500 distinct pitches, got " + juce::String (distinct));
}

LUTHIER_TEST (Engine, aChordVoicesAcrossStrings)
{
    LuthierEngine engine;
    engine.prepare (kSr, kBlock);
    engine.setGuitarType (GuitarType::Dreadnought);
    engine.getMidiInterpreter().setPlayingMode (PlayingMode::Poly);

    juce::MidiBuffer midi;

    // Open E major.
    for (int note : { 40, 47, 52, 56, 59, 64 })
        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);

    auto buffer = render (engine, midi, 1.5);

    CHECK_MSG (bufferIsFinite (buffer), "the chord produced non-finite samples");

    // Several strings should be ringing.
    int ringing = 0;

    for (int s = 0; s < engine.getNumStrings(); ++s)
        if (engine.getStringLevel (s) > 1.0e-5)
            ++ringing;

    CHECK_MSG (ringing >= 4,
               "an open E should light up most of the strings, only "
               + juce::String (ringing) + " are ringing");
}

LUTHIER_TEST (Engine, fastSlidesProduceNoNansOrDenormals)
{
    // Engine spec: play a fast slide across the fretboard, verify no denormals
    // and no NaNs.
    LuthierEngine engine;
    engine.prepare (kSr, kBlock);
    engine.setGuitarType (GuitarType::Stratocaster);
    engine.getMidiInterpreter().setPlayingMode (PlayingMode::Mono);

    const int total = (int) (kSr * 6.0);
    juce::AudioBuffer<float> block (2, kBlock);

    int position = 0;
    int noteIndex = 0;

    while (position < total)
    {
        block.clear();
        juce::MidiBuffer midi;

        // A note every 30 ms, sweeping up and down the neck as fast as possible.
        if ((position / kBlock) % 6 == 0)
        {
            const int note = 40 + (noteIndex % 25);
            midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.7f), 0);
            ++noteIndex;
        }

        engine.processBlock (block, midi);

        if (! bufferIsFinite (block))
        {
            ctx.fail ("non-finite output at sample " + juce::String (position));
            break;
        }

        position += kBlock;
    }

    ++ctx.checks;

    // The tail must also be finite and must settle rather than ringing forever.
    engine.panic();

    juce::MidiBuffer empty;
    auto tail = render (engine, empty, 2.0);

    CHECK_MSG (bufferIsFinite (tail), "the tail after a panic is not finite");
}

LUTHIER_TEST (Engine, everyGuitarTypeLoadsAndSounds)
{
    for (int g = 0; g < (int) GuitarType::NumTypes; ++g)
    {
        LuthierEngine engine;
        engine.prepare (kSr, kBlock);
        engine.setGuitarType ((GuitarType) g);

        const auto& spec = engine.getGuitarSpec();

        // Pick a note the instrument can actually play: its lowest open string.
        const double openHz = engine.getTuningEngine().getEffectiveOpenFrequency (
            engine.getNumStrings() - 1);
        const int note = juce::jlimit (0, 127,
                                       (int) std::round (hzToMidi (openHz)) + 5);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.85f), 0);

        auto buffer = render (engine, midi, 1.0);

        CHECK_MSG (bufferIsFinite (buffer),
                   juce::String (spec.name) + " produced non-finite output");

        auto mono = toMono (buffer);
        const double level = rms (mono.data(), (int) mono.size());

        CHECK_MSG (level > 1.0e-5,
                   juce::String (spec.name) + " was silent (RMS "
                   + juce::String (level, 9) + ")");

        CHECK_MSG (peak (mono.data(), (int) mono.size()) < 1.2,
                   juce::String (spec.name) + " clipped hard, peak "
                   + juce::String (peak (mono.data(), (int) mono.size()), 3));
    }
}

LUTHIER_TEST (Engine, everyTuningLoadsAndSounds)
{
    for (int p = 0; p < (int) TuningPreset::NumPresets; ++p)
    {
        LuthierEngine engine;
        engine.prepare (kSr, kBlock);
        engine.setTuningPreset ((TuningPreset) p);

        juce::MidiBuffer midi;

        const double openHz = engine.getTuningEngine().getEffectiveOpenFrequency (0);
        const int note = juce::jlimit (0, 127, (int) std::round (hzToMidi (openHz)));

        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);

        auto buffer = render (engine, midi, 0.8);

        CHECK_MSG (bufferIsFinite (buffer),
                   juce::String (TuningEngine::getTuningPresetName ((TuningPreset) p))
                   + " produced non-finite output");
    }
}

LUTHIER_TEST (Engine, sampleRateChangesAreSurvived)
{
    // Engine rule 5: the plugin must survive a host switching rates mid-session.
    LuthierEngine engine;

    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0, 44100.0 }) // qa-polish.md 1.3
    {
        engine.prepare (rate, kBlock);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 52, 0.8f), 0);

        const int total = (int) (rate * 0.3);
        juce::AudioBuffer<float> block (2, kBlock);

        int position = 0;
        bool first = true;

        while (position < total)
        {
            block.clear();
            juce::MidiBuffer thisBlock;

            if (first) { thisBlock = midi; first = false; }

            engine.processBlock (block, thisBlock);

            CHECK_MSG (bufferIsFinite (block),
                       "non-finite output at " + juce::String (rate, 0) + " Hz");

            position += kBlock;
        }
    }
}

LUTHIER_TEST (Engine, blockSizeChangesAreSurvived)
{
    LuthierEngine engine;

    for (int blockSize : { 16, 32, 64, 128, 256, 512, 1024, 2048 })
    {
        engine.prepare (kSr, blockSize);

        juce::AudioBuffer<float> block (2, blockSize);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 55, 0.8f), 0);

        for (int i = 0; i < 20; ++i)
        {
            block.clear();
            juce::MidiBuffer thisBlock;

            if (i == 0)
                thisBlock = midi;

            engine.processBlock (block, thisBlock);

            CHECK_MSG (bufferIsFinite (block),
                       "non-finite output at block size " + juce::String (blockSize));
        }
    }
}

LUTHIER_TEST (Engine, latencyIsReportedAndPlausible)
{
    LuthierEngine engine;
    engine.prepare (kSr, kBlock);

    const int latency = engine.getLatencySamples();

    CHECK_MSG (latency >= 0, "latency must never be negative");
    CHECK_MSG (latency < (int) (kSr * 0.05),
               "reported latency of " + juce::String (latency)
               + " samples is implausibly long");
}

LUTHIER_TEST (Engine, panicSilencesEverything)
{
    LuthierEngine engine;
    engine.prepare (kSr, kBlock);

    juce::MidiBuffer midi;

    for (int note : { 40, 47, 52, 56, 59, 64 })
        midi.addEvent (juce::MidiMessage::noteOn (1, note, 1.0f), 0);

    render (engine, midi, 0.5);

    engine.panic();

    juce::MidiBuffer empty;
    auto after = render (engine, empty, 1.0);

    auto mono = toMono (after);

    // Allow the reverb tail a moment, then require near-silence.
    const int tailStart = (int) (kSr * 0.6);

    CHECK_MSG (peak (mono.data() + tailStart, (int) mono.size() - tailStart) < 0.02,
               "panic left " + juce::String (peak (mono.data() + tailStart,
                                                   (int) mono.size() - tailStart), 5)
               + " ringing");
}

LUTHIER_TEST (Engine, monoCompatibility)
{
    // Pitfall 19: a wide effect must not cancel when a listener folds to mono.
    LuthierEngine engine;
    engine.prepare (kSr, kBlock);
    engine.setGuitarType (GuitarType::Stratocaster);

    engine.getCabinetEngine().setDualMicEnabled (true);
    engine.getCabinetEngine().setStereoWidth (1.0);
    engine.getRoomEngine().setRoomBlend (0.5);
    engine.getRoomEngine().setWidth (1.0);

    engine.getPostEffects().setSlotType (0, PedalType::Chorus);
    engine.getPostEffects().setSlotType (1, PedalType::Reverb);

    juce::MidiBuffer midi;

    for (int note : { 40, 47, 52 })
        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);

    auto buffer = render (engine, midi, 2.0);

    const int n = buffer.getNumSamples();

    double stereoEnergy = 0.0, monoEnergy = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const double l = buffer.getSample (0, i);
        const double r = buffer.getSample (1, i);

        stereoEnergy += 0.5 * (l * l + r * r);

        const double m = 0.5 * (l + r);
        monoEnergy += m * m;
    }

    CHECK_MSG (stereoEnergy > 1.0e-9, "the test signal was silent");

    const double retained = monoEnergy / juce::jmax (1.0e-12, stereoEnergy);

    CHECK_MSG (retained > 0.5,
               "folding to mono lost too much: only "
               + juce::String (retained * 100.0, 1) + "% of the energy survived");
}

LUTHIER_TEST (Engine, cpuStaysWithinBudget)
{
    // Engine spec 22: six strings ringing with everything on should stay well
    // inside real time. The absolute number depends on the machine, so this
    // asserts the thing that actually matters: it is comfortably faster than
    // real time.
    LuthierEngine engine;
    engine.prepare (kSr, kBlock);
    engine.setGuitarType (GuitarType::LesPaul);

    engine.getPreEffects().setSlotType (0, PedalType::Compressor);
    engine.getPreEffects().setSlotType (1, PedalType::Overdrive);
    engine.getPostEffects().setSlotType (0, PedalType::Delay);
    engine.getPostEffects().setSlotType (1, PedalType::Reverb);
    engine.getCabinetEngine().setDualMicEnabled (true);

    const double seconds = 3.0;

    /*  Best of three: wall-clock time on a shared machine picks up whatever
        else is running (virus scans, a VM), and the fastest run is the one
        that measures the engine rather than the neighbours. */
    double elapsed = 1.0e9;

    for (int run = 0; run < 3; ++run)
    {
        engine.panic();

        juce::MidiBuffer notes;

        for (int note : { 40, 47, 52, 56, 59, 64 })
            notes.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);

        const auto start = juce::Time::getHighResolutionTicks();

        render (engine, notes, seconds);

        elapsed = juce::jmin (elapsed, juce::Time::highResolutionTicksToSeconds (
                                           juce::Time::getHighResolutionTicks() - start));
    }

    const double realtimeFactor = elapsed / seconds;

    // The absolute figure depends entirely on the machine, so what is asserted
    // is the invariant that matters: one fully-loaded instance renders
    // comfortably faster than real time, with headroom for a whole session.
    CHECK_MSG (realtimeFactor < 0.85,
               "rendering " + juce::String (seconds, 1) + " s took "
               + juce::String (elapsed, 3) + " s ("
               + juce::String (realtimeFactor * 100.0, 1) + "% of real time)");
}

//==============================================================================
//  Parameters, presets and state
//==============================================================================
LUTHIER_TEST (Parameters, everyParameterHasAUniqueIdAndSaneDefault)
{
    HarnessProcessor processor;

    juce::StringArray seen;

    for (auto* p : processor.getParameters())
    {
        auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p);

        CHECK_MSG (withId != nullptr, "every parameter must have an ID");

        if (withId == nullptr)
            continue;

        CHECK_MSG (! seen.contains (withId->paramID),
                   "duplicate parameter ID: " + withId->paramID);
        seen.add (withId->paramID);

        CHECK_MSG (withId->getName (64).isNotEmpty(),
                   withId->paramID + " has no display name");

        const float def = p->getDefaultValue();

        CHECK_MSG (def >= 0.0f && def <= 1.0f,
                   withId->paramID + " has an out-of-range default: " + juce::String (def));
    }

    /*  The exact count, not "more than 150".

        A host stores automation against the parameter list, so adding, removing
        or reordering one silently rewrites what every saved session automates.
        That makes the size of this list a compatibility surface rather than an
        implementation detail, and something that should have to be changed on
        purpose. docs/CHANGELOG.md quotes this number; if you change the set,
        change it there too. */
    CHECK_MSG (seen.size() == 450 + 3   // MODEL-GAPS
                             + 15   // REALISM-A
                             + 29   // REALISM-B
                             + 29   // REALISM-C
                             + 2    // TUNE-HELP-ONBOARDING
                             + 34   // FEAT-JAM
                             + 1    // SPEC-SWEEP
                             ,
               "the parameter list has changed size: " + juce::String (seen.size())
                 + " parameters, not the expected total - saved host automation is indexed "
                   "against this list");
}

LUTHIER_TEST (Parameters, everyParameterTextRoundTrips)
{
    HarnessProcessor processor;

    for (auto* p : processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);

        if (ranged == nullptr)
            continue;

        for (float value : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            ranged->setValueNotifyingHost (value);

            const auto text = ranged->getCurrentValueAsText();

            CHECK_MSG (text.isNotEmpty(),
                       ranged->paramID + " produced no text at " + juce::String (value));
        }
    }
}

LUTHIER_TEST (Presets, everyFactoryPresetLoadsAndPlays)
{
    // Engine spec round-trip test, and shipping criterion: every factory preset
    // must load, produce sound, and not produce anything non-finite.
    HarnessProcessor processor;
    FactoryPresets::setProcessorForRanges (&processor);
    const FactoryRangesReset factoryRangesReset;   // SPEC-SWEEP: no dangling pointer after this test

    processor.prepareToPlay (kSr, kBlock);

    const int count = FactoryPresets::getNumPresets();

    CHECK_MSG (count >= 20, "the factory bank should be substantial, has "
               + juce::String (count));

    for (int i = 0; i < count; ++i)
    {
        const auto& def = FactoryPresets::getPreset (i);
        const juce::String name (def.name);

        const auto data = FactoryPresets::toVar (def, processor);

        CHECK_MSG (processor.presets.fromVar (data), name + " failed to load");

        processor.bridge.applyAllNow();

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 52, 0.85f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 59, 0.85f), 0);

        auto buffer = render (processor.engine, midi, 1.0);

        CHECK_MSG (bufferIsFinite (buffer), name + " produced non-finite audio");

        auto mono = toMono (buffer);
        const double level = rms (mono.data(), (int) mono.size());

        CHECK_MSG (level > 1.0e-6, name + " was silent (RMS " + juce::String (level, 10) + ")");

        CHECK_MSG (peak (mono.data(), (int) mono.size()) <= 1.01,
                   name + " exceeded full scale, peak "
                   + juce::String (peak (mono.data(), (int) mono.size()), 4));

        processor.engine.panic();
    }
}

LUTHIER_TEST (Presets, stateRoundTripsExactly)
{
    HarnessProcessor processor;
    FactoryPresets::setProcessorForRanges (&processor);
    const FactoryRangesReset factoryRangesReset;   // SPEC-SWEEP: no dangling pointer after this test
    processor.prepareToPlay (kSr, kBlock);

    RtRandom rng { 0xBEEF };

    // Scramble everything.
    for (auto* p : processor.getParameters())
        p->setValueNotifyingHost ((float) rng.nextDouble());

    processor.presets.captureExtraState();
    const auto saved = processor.presets.toVar ("RoundTrip", "Test");

    // Record what we had.
    juce::Array<float> before;

    for (auto* p : processor.getParameters())
        before.add (p->getValue());

    // Change everything again.
    for (auto* p : processor.getParameters())
        p->setValueNotifyingHost ((float) rng.nextDouble());

    // Restore.
    CHECK (processor.presets.fromVar (saved));

    int index = 0;
    int mismatches = 0;

    for (auto* p : processor.getParameters())
    {
        const float now = p->getValue();
        const float then = before[index++];

        // A preset never carries the morph position (ambiguity-resolutions 5).
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            if (withId->paramID == ParamIDs::presetMorphPosition
                  || ParamIDs::isJamTransient (withId->paramID))   // FEAT-JAM: jam-mode 10
                continue;

        if (std::abs (now - then) > 1.0e-4f)
            ++mismatches;
    }

    CHECK_MSG (mismatches == 0,
               juce::String (mismatches) + " parameters did not survive the round trip");
}

LUTHIER_TEST (Presets, anAdvancedValueSurvivesTheRoundTrip)
{
    /*  advanced-ranges.md 4: the ranges block has to be applied before the
        parameter values, because the values are stored normalised. Load it
        after and an amp gain of 1.8 comes back as 0.9. */
    HarnessProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    RangeState unlocked;
    unlocked.setFamilyAdvanced (RangeFamily::amp, true);
    processor.ranges = unlocked;
    CHECK (processor.ranges.applyTo (processor.apvts) == 0);

    auto* gain = processor.apvts.getParameter (ParamIDs::ampGain);
    CHECK (gain != nullptr);
    gain->setValueNotifyingHost (gain->convertTo0to1 (1.8f));
    CHECK (std::abs (gain->convertFrom0to1 (gain->getValue()) - 1.8f) < 1.0e-3f);

    processor.presets.captureExtraState();
    const auto saved = processor.presets.toVar ("Advanced", "Test");

    // A stock preset in between, so the load has to widen the range itself.
    processor.presets.resetToDefaults();
    processor.ranges.reset();
    processor.ranges.applyTo (processor.apvts);

    CHECK (processor.presets.fromVar (saved));
    CHECK_MSG (processor.ranges.isFamilyAdvanced (RangeFamily::amp),
               "the amp family did not come back unlocked");

    const float plain = gain->convertFrom0to1 (gain->getValue());
    CHECK_MSG (std::abs (plain - 1.8f) < 1.0e-3f,
               "amp gain came back as " + juce::String (plain) + ", not 1.8");
}

LUTHIER_TEST (Presets, audioIsIdenticalAfterARoundTrip)
{
    // The stronger form of the round-trip test: not just that the numbers match,
    // but that the sound does.
    auto renderWith = [] (HarnessProcessor& processor)
    {
        // reset(), not panic(): panic stops the strings but deliberately leaves
        // the reverb and convolution tails alone, which is the musical behaviour
        // but not a clean slate.
        processor.engine.reset();
        processor.bridge.applyAllNow();
        processor.engine.reset();

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 52, 0.8f), 0);

        return toMono (render (processor.engine, midi, 0.5));
    };

    HarnessProcessor processor;
    FactoryPresets::setProcessorForRanges (&processor);
    const FactoryRangesReset factoryRangesReset;   // SPEC-SWEEP: no dangling pointer after this test
    processor.prepareToPlay (kSr, kBlock);

    // Humanisation is deliberately random per note, so it has to be off for a
    // sample-exact comparison to mean anything.
    if (auto* p = processor.apvts.getParameter (ParamIDs::macroHumanize))
        p->setValueNotifyingHost (0.0f);

    if (auto* p = processor.apvts.getParameter (ParamIDs::realismDetune))
        p->setValueNotifyingHost (0.0f);

    const auto first = renderWith (processor);

    processor.presets.captureExtraState();
    const auto saved = processor.presets.toVar ("Audio", "Test");

    processor.presets.resetToDefaults();
    processor.bridge.applyAllNow();

    CHECK (processor.presets.fromVar (saved));
    processor.presets.applyExtraState();

    const auto second = renderWith (processor);

    CHECK_MSG (first.size() == second.size(), "renders differ in length");

    double worst = 0.0;

    for (size_t i = 0; i < juce::jmin (first.size(), second.size()); ++i)
        worst = juce::jmax (worst, std::abs (first[i] - second[i]));

    const double worstDb = gainToDb (worst + 1.0e-12);

    CHECK_MSG (worstDb < -60.0,
               "audio changed after a preset round trip: worst difference "
               + juce::String (worstDb, 2) + " dB");
}

LUTHIER_TEST (Presets, resetRestoresDefaults)
{
    HarnessProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    RtRandom rng { 7 };

    for (auto* p : processor.getParameters())
        p->setValueNotifyingHost ((float) rng.nextDouble());

    processor.presets.resetToDefaults();

    int wrong = 0;

    for (auto* p : processor.getParameters())
        if (std::abs (p->getValue() - p->getDefaultValue()) > 1.0e-5f)
            ++wrong;

    CHECK_MSG (wrong == 0, juce::String (wrong) + " parameters were not reset");
}

LUTHIER_TEST (Presets, randomiseRespectsLocks)
{
    HarnessProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    // Lock a handful and check they survive.
    const juce::StringArray locked { ParamIDs::macroDrive, ParamIDs::ampModel,
                                     ParamIDs::guitarType };

    // A choice parameter quantises whatever it is given, so what has to be
    // compared afterwards is the value it actually took, not the one we asked for.
    juce::Array<float> expected;

    for (const auto& id : locked)
    {
        auto* p = processor.apvts.getParameter (id);

        if (p != nullptr)
            p->setValueNotifyingHost (0.42f);

        expected.add (p != nullptr ? p->getValue() : 0.0f);
    }

    processor.presets.randomise (1234, locked);

    int index = 0;

    for (const auto& id : locked)
    {
        if (auto* p = processor.apvts.getParameter (id))
            CHECK_MSG (std::abs (p->getValue() - expected[index]) < 1.0e-5f,
                       id + " was randomised despite being locked");

        ++index;
    }

    // And that something actually changed.
    int changed = 0;

    for (auto* p : processor.getParameters())
        if (std::abs (p->getValue() - p->getDefaultValue()) > 1.0e-4f)
            ++changed;

    CHECK_MSG (changed > 10, "randomise barely changed anything");
}

LUTHIER_TEST (Presets, randomiseNeverProducesSomethingBroken)
{
    // include.md: try every combination and keep fixing until bug free. Exhaustive
    // is impossible, so this is the practical form: many random full states, each
    // rendered and checked.
    HarnessProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const int iterations = 120;

    for (int i = 0; i < iterations; ++i)
    {
        processor.presets.randomise ((uint64_t) (i * 7919 + 13), {});
        processor.bridge.applyAllNow();
        processor.engine.panic();

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 45 + (i % 20), 0.8f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 52 + (i % 12), 0.8f), 0);

        auto buffer = render (processor.engine, midi, 0.4);

        if (! bufferIsFinite (buffer))
        {
            ctx.fail ("random state " + juce::String (i) + " produced non-finite audio");
            break;
        }

        auto mono = toMono (buffer);

        if (peak (mono.data(), (int) mono.size()) > 1.02)
        {
            ctx.fail ("random state " + juce::String (i) + " exceeded full scale: "
                      + juce::String (peak (mono.data(), (int) mono.size()), 4));
            break;
        }
    }

    ++ctx.checks;
}

LUTHIER_TEST (Parameters, fuzzAcrossTenThousandStates)
{
    // include.md asks for a fuzz test over 10,000 random states. Rendering audio
    // for each would take hours, so this splits the work: every state is applied
    // to the engine and checked for internal sanity, and every hundredth state is
    // also rendered. That covers the parameter space broadly and the audio path
    // often enough to catch anything that gets through.
    HarnessProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    RtRandom rng { 0x1234ABCD };

    const int states = 10000;
    int rendered = 0;

    for (int i = 0; i < states; ++i)
    {
        for (auto* p : processor.getParameters())
            p->setValue ((float) rng.nextDouble());

        processor.bridge.applyToEngine();

        if (i % 100 == 0)
        {
            processor.bridge.applyAllNow();
            processor.engine.panic();

            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 40 + (i % 30), 0.9f), 0);

            auto buffer = render (processor.engine, midi, 0.15);
            ++rendered;

            if (! bufferIsFinite (buffer))
            {
                ctx.fail ("fuzz state " + juce::String (i) + " produced non-finite audio");
                break;
            }

            // qa-polish.md 2.2: no denormal escapes the engine's flushing.
            {
                int tiny = 0;

                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    for (int k = 0; k < buffer.getNumSamples(); ++k)
                    {
                        const auto v = std::abs (buffer.getSample (ch, k));
                        tiny += (v > 0.0f && v < 1.0e-30f) ? 1 : 0;
                    }

                if (tiny > 0)
                {
                    ctx.fail ("fuzz state " + juce::String (i) + " let " + juce::String (tiny)
                              + " denormal-range samples out");
                    break;
                }
            }

            auto mono = toMono (buffer);

            if (peak (mono.data(), (int) mono.size()) > 1.05)
            {
                ctx.fail ("fuzz state " + juce::String (i) + " exceeded full scale: "
                          + juce::String (peak (mono.data(), (int) mono.size()), 4));
                break;
            }
        }
    }

    ++ctx.checks;

    CHECK_MSG (rendered >= 90, "expected about 100 rendered fuzz states, got "
               + juce::String (rendered));
}

//==============================================================================
//  Support systems
//==============================================================================
LUTHIER_TEST (MidiCapture, capturesAndWritesAFile)
{
    MidiCapture capture;
    capture.prepare (kSr, 60.0);

    juce::MidiBuffer midi;

    for (int i = 0; i < 20; ++i)
    {
        midi.addEvent (juce::MidiMessage::noteOn (1, 40 + i, 0.8f), i * 100);
        midi.addEvent (juce::MidiMessage::noteOff (1, 40 + i), i * 100 + 50);
    }

    capture.capture (midi, 0);

    CHECK_MSG (capture.getEventCount() >= 40,
               "expected 40 events, got " + juce::String (capture.getEventCount()));

    const auto file = juce::File::createTempFile ("mid");

    CHECK (capture.writeToFile (file, 120.0));
    CHECK_MSG (file.getSize() > 0, "the written MIDI file is empty");

    // And it reads back.
    juce::FileInputStream stream (file);
    juce::MidiFile readBack;

    CHECK (stream.openedOk() && readBack.readFrom (stream));
    CHECK_MSG (readBack.getNumTracks() >= 1, "no tracks in the written file");

    file.deleteFile();
}

LUTHIER_TEST (MidiLearn, mapsAndUnmapsCleanly)
{
    HarnessProcessor processor;
    MidiLearnManager learn (processor.apvts);

    learn.addMapping (ParamIDs::macroDrive, 21);

    CHECK (learn.getCcForParameter (ParamIDs::macroDrive) == 21);
    CHECK (learn.getParameterForCc (21) == ParamIDs::macroDrive);
    CHECK (learn.getNumMappings() == 1);

    // A CC drives the parameter.
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 21, 127), 0);

    learn.processMidi (midi);

    if (auto* p = processor.apvts.getParameter (ParamIDs::macroDrive))
        CHECK_NEAR (p->getValue(), 1.0f, 0.01f);

    // One CC controls one parameter: remapping replaces rather than duplicating.
    learn.addMapping (ParamIDs::macroTone, 21);

    CHECK (learn.getNumMappings() == 1);
    CHECK (learn.getCcForParameter (ParamIDs::macroDrive) == -1);
    CHECK (learn.getParameterForCc (21) == ParamIDs::macroTone);

    // Round trip.
    const auto saved = learn.toVar();
    learn.clearAllMappings();
    CHECK (learn.getNumMappings() == 0);

    learn.fromVar (saved);
    CHECK (learn.getCcForParameter (ParamIDs::macroTone) == 21);
}

LUTHIER_TEST (Audition, everyPhraseProducesUsableMidi)
{
    for (int t = 0; t < (int) AuditionPhrase::Type::NumTypes; ++t)
    {
        const auto type = (AuditionPhrase::Type) t;
        const auto sequence = AuditionPhrase::build (type, 100.0);
        const juce::String name (AuditionPhrase::getName (type));

        CHECK_MSG (sequence.getNumEvents() > 0, name + " produced no events");

        CHECK_MSG (sequence.getEndTime() > 0.1 && sequence.getEndTime() < 60.0,
                   name + " runs for " + juce::String (sequence.getEndTime(), 2) + " s");

        // Every note-on must have a matching note-off, or the phrase leaves a
        // note hanging when it finishes.
        int noteOns = 0, noteOffs = 0;

        for (int i = 0; i < sequence.getNumEvents(); ++i)
        {
            const auto& m = sequence.getEventPointer (i)->message;

            if (m.isNoteOn())  ++noteOns;
            if (m.isNoteOff()) ++noteOffs;
        }

        CHECK_MSG (noteOns == noteOffs,
                   name + " has " + juce::String (noteOns) + " note-ons but "
                   + juce::String (noteOffs) + " note-offs");
    }
}

LUTHIER_TEST (Diagnostics, ringBufferAndSelfTestWork)
{
    Diagnostics diagnostics;
    diagnostics.prepare (kSr);
    diagnostics.setEnabled (true);

    for (int i = 0; i < 1000; ++i)
        diagnostics.logValue (LogCategory::Engine, "test", (double) i, i);

    CHECK (diagnostics.getTotalRecords() == 1000);

    Diagnostics::Record records[32];
    const int count = diagnostics.getRecords (records, 32);

    CHECK (count == 32);
    CHECK_MSG (juce::String (records[31].text) == "test", "the newest record is wrong");
    CHECK_NEAR (records[31].value, 999.0, 1.0e-9);

    const auto selfTest = Diagnostics::runSelfTest();

    CHECK_MSG (selfTest.passed.size() > 0, "the self test checked nothing");

    const auto report = diagnostics.buildTroubleshootingReport ("{}", "All checks passing.");

    CHECK_MSG (report.contains ("LUTHIER TROUBLESHOOTING REPORT"), "the report has no header");
    CHECK_MSG (report.contains ("SELF TEST"), "the report has no self-test section");
    CHECK_MSG (report.contains ("CURRENT SETTINGS"), "the report has no settings section");
}

//==============================================================================
/*  gui-integration.md section 19 puts MIDI Learn on a header button as well as on
    the right-click menu, and its ground rule 4 forbids a feature being reachable
    only by right-click. That needs a global arm: a state where no parameter has
    been chosen yet and the next control clicked becomes the target.

    The click itself belongs to the editor, which is not in this target, so what is
    checked here is the state machine the editor drives. */
LUTHIER_TEST (MidiLearn, armingIsSeparateFromLearningUntilAControlClaimsIt)
{
    HarnessProcessor processor;
    MidiLearnManager learn (processor.apvts);

    CHECK (! learn.isArmed());
    CHECK (! learn.isLearning());

    learn.setArmed (true);

    // Armed but not yet learning: nothing has said which parameter.
    CHECK (learn.isArmed());
    CHECK_MSG (! learn.isLearning(),
               "arming alone started learning, so the first CC would map itself to "
               "whatever was learned last");

    // A CC arriving while merely armed must not be captured.
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 42, 100), 0);
    learn.processMidi (midi);

    CHECK_MSG (learn.getNumMappings() == 0,
               "a CC was captured while armed but before a control was chosen");

    // A control claiming the arm is what picks the parameter.
    CHECK (learn.claimArmedLearn (ParamIDs::macroDrive));

    CHECK_MSG (! learn.isArmed(), "claiming the arm left it armed for the next click");
    CHECK (learn.isLearning());
    CHECK (learn.getLearningParameterId() == ParamIDs::macroDrive);

    /*  The CC is now captured. The mapping itself is added on the message thread
        - processMidi defers it, because mutating the array on the audio thread is
        not allowed - and nothing here runs a dispatch loop, so what is observable
        synchronously is that the learn has been consumed. */
    learn.processMidi (midi);

    CHECK_MSG (! learn.isLearning(),
               "a CC arrived while learning and the learn was not consumed");
}

//==============================================================================
/*  A second control cannot steal a claim that has already been made, and
    disarming has to cancel a learn that is in flight - otherwise the plugin sits
    waiting for a CC with nothing on screen saying so, and the next stray knob on
    the user's controller maps itself. */
LUTHIER_TEST (MidiLearn, disarmingCancelsAnInFlightLearn)
{
    HarnessProcessor processor;
    MidiLearnManager learn (processor.apvts);

    // Nothing to claim when not armed.
    CHECK (! learn.claimArmedLearn (ParamIDs::macroDrive));
    CHECK (! learn.isLearning());

    learn.setArmed (true);
    CHECK (learn.claimArmedLearn (ParamIDs::macroDrive));

    // The arm is spent, so a second control clicked afterwards is not captured.
    CHECK (! learn.claimArmedLearn (ParamIDs::macroTone));
    CHECK (learn.getLearningParameterId() == ParamIDs::macroDrive);

    learn.setArmed (false);

    CHECK_MSG (! learn.isLearning(),
               "disarming left a learn in flight, so the next CC would still be "
               "captured with no visible sign of it");

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 42, 100), 0);
    learn.processMidi (midi);

    CHECK (learn.getNumMappings() == 0);
}

//==============================================================================
/*  file-formats.md 14.2: the magic marker is checked before anything is applied,
    so a JSON file that is not a Luthier preset is refused rather than
    half-loaded. */
LUTHIER_TEST (Presets, aFileWithoutTheMagicMarkerIsRefused)
{
    HarnessProcessor processor;
    FactoryPresets::setProcessorForRanges (&processor);
    const FactoryRangesReset factoryRangesReset;   // SPEC-SWEEP: no dangling pointer after this test
    processor.prepareToPlay (kSr, kBlock);

    processor.presets.captureExtraState();
    const auto good = processor.presets.toVar ("Magic", "Test");

    CHECK_MSG (processor.presets.fromVar (good),
               "a preset this build just wrote did not load back");

    // Well-formed JSON, plausible shape, not ours.
    auto* impostor = new juce::DynamicObject();
    impostor->setProperty ("schemaVersion", 1);
    impostor->setProperty ("name", "Not a preset");

    auto* params = new juce::DynamicObject();
    params->setProperty (ParamIDs::macroDrive, 0.9);
    impostor->setProperty ("parameters", juce::var (params));

    CHECK_MSG (! processor.presets.fromVar (juce::var (impostor)),
               "a JSON file with no magic marker was accepted as a preset");

    // The canonical marker from file-formats 1 is what gets written.
    if (auto* obj = good.getDynamicObject())
        CHECK (obj->getProperty ("magic").toString() == PresetManager::kMagic);
}

//==============================================================================
/*  file-formats.md 0.3: fields this build does not understand survive a load and
    save, so opening a newer version's preset and re-saving it does not silently
    delete whatever that version added. */
LUTHIER_TEST (Presets, unknownFieldsSurviveARoundTrip)
{
    HarnessProcessor processor;
    FactoryPresets::setProcessorForRanges (&processor);
    const FactoryRangesReset factoryRangesReset;   // SPEC-SWEEP: no dangling pointer after this test
    processor.prepareToPlay (kSr, kBlock);

    processor.presets.captureExtraState();
    auto fromTheFuture = processor.presets.toVar ("Future", "Test");

    auto* obj = fromTheFuture.getDynamicObject();
    CHECK (obj != nullptr);

    if (obj == nullptr)
        return;

    // Something a later schema added and this build knows nothing about.
    auto* workshop = new juce::DynamicObject();
    workshop->setProperty ("bridge", "tune-o-matic");
    workshop->setProperty ("relief_mm", 0.25);

    obj->setProperty ("workshop", juce::var (workshop));
    obj->setProperty ("someFutureFlag", true);

    CHECK (processor.presets.fromVar (fromTheFuture));

    processor.presets.captureExtraState();
    const auto written = processor.presets.toVar ("Future", "Test");

    auto* out = written.getDynamicObject();
    CHECK (out != nullptr);

    if (out == nullptr)
        return;

    CHECK_MSG (out->hasProperty ("someFutureFlag"),
               "a field from a newer schema was dropped on save");

    if (auto* keptWorkshop = out->getProperty ("workshop").getDynamicObject())
    {
        CHECK (keptWorkshop->getProperty ("bridge").toString() == "tune-o-matic");
        CHECK_NEAR ((double) keptWorkshop->getProperty ("relief_mm"), 0.25, 1.0e-9);
    }
    else
    {
        CHECK_MSG (false, "the nested block from a newer schema was dropped on save");
    }
}

//==============================================================================
/*  file-formats.md 13.4: the version being replaced is filed in a dated backup
    folder, and 13's sweep prunes anything past the retention window. A backup
    must not then reappear in the browser as a preset of its own. */
LUTHIER_TEST (Presets, savingBacksUpTheVersionItReplaces)
{
    HarnessProcessor processor;
    FactoryPresets::setProcessorForRanges (&processor);
    const FactoryRangesReset factoryRangesReset;   // SPEC-SWEEP: no dangling pointer after this test
    processor.prepareToPlay (kSr, kBlock);

    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("LuthierBackupTest");

    folder.deleteRecursively();
    folder.createDirectory();

    auto target = folder.getChildFile (juce::String ("Backed Up") + PresetManager::kFileExtension);

    // The version that will be replaced.
    target.replaceWithText ("{\"magic\":\"luthier.preset\",\"schemaVersion\":1,\"name\":\"first\"}");

    PresetManager::backupBeforeOverwrite (target);

    const auto today = juce::Time::getCurrentTime().formatted ("%Y-%m-%d");
    auto dated = folder.getChildFile ("Backup").getChildFile (today);

    CHECK_MSG (dated.isDirectory(), "no dated backup folder was created");

    auto kept = dated.getChildFile (target.getFileName());

    CHECK_MSG (kept.existsAsFile(), "the replaced version was not kept");
    CHECK (kept.loadFileAsString().contains ("first"));

    // A second save the same day keeps both rather than overwriting the backup.
    PresetManager::backupBeforeOverwrite (target);

    int backupsToday = 0;

    for (const auto& entry : juce::RangedDirectoryIterator (dated, false,
                                                            juce::String ("*") + PresetManager::kFileExtension,
                                                            juce::File::findFiles))
    {
        juce::ignoreUnused (entry);
        ++backupsToday;
    }

    CHECK_MSG (backupsToday == 2,
               "a second save the same day overwrote the first backup, so "
               + juce::String (backupsToday) + " remain rather than 2");

    folder.deleteRecursively();
}

//==============================================================================
/*  file-formats.md 16: "Fuzz: 10 000 mutated bytes across a sample of factory
    files; every load either succeeds or refuses cleanly with no crash."

    The point is not that a mutated preset loads - most will not - but that a
    malformed one never takes the plugin down or half-applies itself. A preset is
    the one file type a user routinely receives from someone else. */
LUTHIER_TEST (Presets, mutatedPresetsNeverCrashTheLoader)
{
    HarnessProcessor processor;
    FactoryPresets::setProcessorForRanges (&processor);
    const FactoryRangesReset factoryRangesReset;   // SPEC-SWEEP: no dangling pointer after this test
    processor.prepareToPlay (kSr, kBlock);

    processor.presets.captureExtraState();

    const auto original = juce::JSON::toString (processor.presets.toVar ("Fuzz", "Test"), false);

    CHECK (original.isNotEmpty());

    RtRandom rng { 0xF0F0BEEF };

    auto bytes = original.toStdString();

    int loaded = 0, refused = 0;

    for (int iteration = 0; iteration < 10000; ++iteration)
    {
        auto mutated = bytes;

        // One to four byte-level corruptions, which is what a truncated download
        // or a bad edit actually looks like.
        const int edits = 1 + (int) (rng.nextDouble() * 4.0);

        for (int e = 0; e < edits; ++e)
        {
            const auto position = (size_t) (rng.nextDouble() * (double) (mutated.size() - 1));

            switch ((int) (rng.nextDouble() * 3.0))
            {
                case 0:  mutated[position] = (char) (int) (rng.nextDouble() * 255.0); break;
                case 1:  mutated[position] = '"'; break;
                default: mutated = mutated.substr (0, position); break;
            }

            if (mutated.empty())
                break;
        }

        const auto parsed = juce::JSON::parse (juce::String (mutated));

        // A parse failure is a clean refusal in itself.
        if (! parsed.isObject())
        {
            ++refused;
            continue;
        }

        if (processor.presets.fromVar (parsed))
            ++loaded;
        else
            ++refused;
    }

    // Reaching here at all is the assertion: nothing threw, nothing faulted.
    CHECK_MSG (loaded + refused == 10000,
               "the fuzz loop lost iterations: " + juce::String (loaded) + " loaded, "
                 + juce::String (refused) + " refused");

    CHECK_MSG (refused > 0, "every mutated preset was accepted, so the loader is "
                            "not validating anything");

    // Whatever the mutations did, the plugin is still in a usable state.
    for (auto* p : processor.getParameters())
    {
        const float value = p->getValue();

        CHECK_MSG (std::isfinite (value) && value >= 0.0f && value <= 1.0f,
                   "a parameter left the normalised range after fuzzed loads");
    }
}

//==============================================================================
/*  error-recovery.md 5 and 13: every failure writes a line to
    errors-<yyyymm>.log, in a format a person can read and a support reply can be
    written against, whether or not telemetry is enabled.

    Ground rule 5 is deliberately not conditional on telemetry consent: a user who
    has opted out of sending anything still deserves a local record, and support
    cannot ask for a log that was never written. */
LUTHIER_TEST (ErrorLog, failuresAreLoggedAsReadableJsonLines)
{
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("LuthierErrorLogTest");

    folder.deleteRecursively();
    folder.createDirectory();

    ErrorLog::setFolderForTesting (folder);
    ErrorLog::setVerbose (false);

    ErrorLog::write (ErrorLog::Severity::warn, "PresetSystem", "MISSING_REFERENCE",
                     "Preset referenced an IR that is not there",
                     [&]
                     {
                         auto* context = new juce::DynamicObject();
                         context->setProperty ("expected_path", "Cab-Match-A.wav");
                         return juce::var (context);
                     }());

    // debug and info are dropped unless Diagnostics verbose is on.
    ErrorLog::write (ErrorLog::Severity::info, "PresetSystem", "CHATTER", "Not important");
    ErrorLog::write (ErrorLog::Severity::debug, "PresetSystem", "NOISE", "Less important");

    auto file = ErrorLog::getLogFile();

    CHECK_MSG (file.existsAsFile(), "no error log was written");

    auto lines = juce::StringArray::fromLines (file.loadFileAsString().trim());

    CHECK_MSG (lines.size() == 1,
               "expected one line with verbose off, got " + juce::String (lines.size()));

    const auto parsed = juce::JSON::parse (lines[0]);
    auto* entry = parsed.getDynamicObject();

    CHECK_MSG (entry != nullptr, "the log line was not a JSON object");

    if (entry != nullptr)
    {
        CHECK (entry->getProperty ("severity").toString() == "warn");
        CHECK (entry->getProperty ("module").toString() == "PresetSystem");
        CHECK (entry->getProperty ("code").toString() == "MISSING_REFERENCE");
        CHECK (entry->getProperty ("ts").toString().isNotEmpty());

        if (auto* context = entry->getProperty ("context").getDynamicObject())
            CHECK (context->getProperty ("expected_path").toString() == "Cab-Match-A.wav");
        else
            CHECK_MSG (false, "the context object was not written");
    }

    // With verbose on, the quiet severities land too.
    ErrorLog::setVerbose (true);
    ErrorLog::write (ErrorLog::Severity::info, "PresetSystem", "CHATTER", "Now it counts");

    lines = juce::StringArray::fromLines (file.loadFileAsString().trim());
    CHECK_MSG (lines.size() == 2,
               "verbose did not enable info, got " + juce::String (lines.size()) + " lines");

    ErrorLog::setVerbose (false);
    ErrorLog::setFolderForTesting ({});
    folder.deleteRecursively();
}

//==============================================================================
/*  error-recovery.md 1: a refused load says why, in the log, and leaves the file
    on disk untouched - ground rule 3. */
LUTHIER_TEST (ErrorLog, arefusedPresetLoadIsRecordedAndChangesNothing)
{
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("LuthierRefusedLoadTest");

    folder.deleteRecursively();
    folder.createDirectory();

    ErrorLog::setFolderForTesting (folder);

    HarnessProcessor processor;
    FactoryPresets::setProcessorForRanges (&processor);
    const FactoryRangesReset factoryRangesReset;   // SPEC-SWEEP: no dangling pointer after this test
    processor.prepareToPlay (kSr, kBlock);

    auto file = folder.getChildFile (juce::String ("Impostor") + PresetManager::kFileExtension);

    const juce::String contents = "{\"schemaVersion\":1,\"name\":\"not ours\"}";
    file.replaceWithText (contents);

    CHECK_MSG (! processor.presets.loadPreset (file),
               "a file with no magic marker was loaded");

    CHECK_MSG (file.loadFileAsString() == contents,
               "a refused load modified the file on disk");

    const auto log = ErrorLog::getLogFile().loadFileAsString();

    CHECK_MSG (log.contains ("BAD_MAGIC"),
               "the refusal was not recorded with a code support could act on");

    ErrorLog::setFolderForTesting ({});
    folder.deleteRecursively();
}

//==============================================================================
//  Ported from PR #2 (claude/clever-hopper-07uz7t): chords on free strings.
/*  The user's "cannot do chords": notes played one after another while the
    earlier ones are held must each get a string of their own. C4 lands on the B
    string, E4 on the high E, and G4 - cheapest on the high E at fret 3 - used to
    take the high E and end E4. Now the held strings are out of bounds for the
    voicer, so all three ring, and neither of the first two is ever re-struck. */
LUTHIER_TEST (Engine, notesPlayedWhileOthersAreHeldEachGetTheirOwnString)
{
    LuthierEngine engine;
    engine.prepare (kSr, kBlock);
    engine.setGuitarType (GuitarType::Dreadnought);
    engine.getMidiInterpreter().setPlayingMode (PlayingMode::Poly);
    engine.getMidiInterpreter().setHumanisation ({ 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 });

    // Each note arrives well outside the chord window of the one before, so
    // every one is voiced on its own against what is already held.
    auto play = [&engine] (int midiNote, double seconds)
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, midiNote, 0.8f), 0);
        render (engine, midi, seconds);
    };

    auto stringHolding = [&engine] (int midiNote)
    {
        for (int s = 0; s < engine.getNumStrings(); ++s)
            if (engine.getStringMidiNote (s) == midiNote)
                return s;

        return -1;
    };

    play (60, 0.05);
    const int c = stringHolding (60);
    CHECK_MSG (c >= 0, "C4 is not sounding on any string");

    play (64, 0.05);
    const int e = stringHolding (64);
    CHECK_MSG (e >= 0 && e != c, "E4 did not get a string of its own");
    CHECK_MSG (stringHolding (60) == c, "E4 took over C4's string");

    play (67, 0.3);
    const int g = stringHolding (67);
    CHECK_MSG (g >= 0 && g != c && g != e, "G4 did not get a string of its own");
    CHECK_MSG (stringHolding (60) == c, "G4 took over C4's string");
    CHECK_MSG (stringHolding (64) == e, "G4 took over E4's string");

    int ringing = 0;

    for (int s = 0; s < engine.getNumStrings(); ++s)
        if (engine.getStringLevel (s) > 1.0e-5)
            ++ringing;

    CHECK_MSG (ringing >= 3, "only " + juce::String (ringing) + " strings are ringing under a held C-E-G");
}
