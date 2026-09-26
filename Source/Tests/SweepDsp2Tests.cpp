/*  SPEC-SWEEP phase 2 (dsp2): behaviour gaps closed in pick-noise, string-squeak,
    fret-buzz, slide, bass, practice-tools, tone-match and host-integration.

    Kept in a file of its own so the sweep's tests do not collide with the
    owning suites' files; each test's suite name is the spec's own.
*/

#include "TestFramework.h"

#include <set>

#include "../DSP/Noise/FretBuzz.h"
#include "../DSP/Noise/PlayingNoise.h"
#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../Practice/Metronome.h"
#include "../DSP/Slide/SlideEngine.h"
#include "../DSP/Effects/PedalsMod.h"
#include "../Practice/BackingTrack.h"
#include "../Practice/TimePitchShifter.h"
#include "../ToneMatch/ToneMatch.h"
#include "../Export/MidiImportTargets.h"
#include "../UI/PracticePanel.h"
#include "../UI/ToneMatchPanel.h"

#if defined (LUTHIER_ALLOCATION_COUNTER)
namespace luthier::tests
{
    long allocationsOnThisThread() noexcept;   // CircuitTests.cpp
}
#endif

using namespace luthier;
using namespace luthier::tests;

namespace
{
    /** A host that reports a fixed tempo and transport state. */
    struct FakePlayHead final : public juce::AudioPlayHead
    {
        double bpm = 120.0;
        bool playing = false;
        juce::int64 samplePosition = 0;
        int numerator = 4, denominator = 4;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (bpm);
            info.setIsPlaying (playing);
            info.setTimeInSamples (samplePosition);
            info.setPpqPosition ((double) samplePosition / 48000.0 * bpm / 60.0);
            info.setTimeSignature (juce::AudioPlayHead::TimeSignature { numerator, denominator });
            return info;
        }
    };

    void runBlocks (LuthierAudioProcessor& processor, int blocks, int blockSize = 512)
    {
        juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), blockSize);
        juce::MidiBuffer midi;

        for (int i = 0; i < blocks; ++i)
        {
            buffer.clear();
            midi.clear();
            processor.processBlock (buffer, midi);
        }
    }

    //==========================================================================
    /** A guitar-controller engine (channel = string) with every shift squeaking. */
    struct SqueakRig
    {
        LuthierEngine engine;
        juce::AudioBuffer<float> buffer { 2, 256 };

        SqueakRig()
        {
            engine.prepare (48000.0, 256);
            engine.setGuitarType (GuitarType::Dreadnought);
            engine.getMidiInterpreter().setPlayingMode (PlayingMode::GuitarController);

            SqueakSettings always;
            always.probability = 1.0;
            always.moisture = 0.0;
            engine.setSqueak (always);
        }

        void play (const juce::MidiMessage& m)
        {
            juce::MidiBuffer midi;
            midi.addEvent (m, 0);
            buffer.clear();
            engine.processBlock (buffer, midi);
        }

        juce::int64 squeaks() { return engine.getPlayingNoise().getPool().getTriggerCount (NoiseClass::squeak); }
    };

    double activeLevel (NoiseEngine& pool, NoiseClass c)
    {
        for (int i = 0; i < pool.getPoolLimit (c); ++i)
            if (auto* g = pool.getGenerator (c, i); g != nullptr && g->isActive())
                return g->getEvent().level;

        return 0.0;
    }

    SetupGeometry needsATech()
    {
        SetupGeometry g;
        const auto& style = getSetupStyle (5);
        g.actionTreble = style.actionTreble;
        g.actionBass = style.actionBass;
        g.relief = style.relief;
        return g;
    }
}

//==============================================================================
//  string-squeak
//==============================================================================
LUTHIER_TEST (Squeak, aRevoiceOfAHeldNoteSqueaks)
{
    // SQ-8 (string-squeak.md 2): the low E (channel 6) held at fret 3, then
    // struck again at fret 8 without letting go - a chord shift. The note is
    // re-struck, and the finger travelled, so it squeaks.
    {
        SqueakRig rig;
        rig.play (juce::MidiMessage::noteOn (6, 43, (juce::uint8) 110));
        rig.play (juce::MidiMessage::noteOn (6, 48, (juce::uint8) 120));
        CHECK_MSG (rig.squeaks() == 1, "a held note moved five frets did not squeak");
    }

    // The same move with the note released in between: the finger lifted.
    {
        SqueakRig rig;
        rig.play (juce::MidiMessage::noteOn (6, 43, (juce::uint8) 110));
        rig.play (juce::MidiMessage::noteOff (6, 43));
        rig.play (juce::MidiMessage::noteOn (6, 48, (juce::uint8) 120));
        CHECK_MSG (rig.squeaks() == 0, "a new pluck after the finger lifted squeaked");
    }

    // A plain string never squeaks, however it moves (0.1).
    {
        SqueakRig rig;
        rig.play (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 110));
        rig.play (juce::MidiMessage::noteOn (1, 72, (juce::uint8) 120));
        CHECK (rig.squeaks() == 0);
    }
}

LUTHIER_TEST (Squeak, fretWearRaisesSqueak)
{
    // SQ-27 (string-squeak.md 10): a worn fret raises squeak slightly.
    auto levelWithWear = [] (double wear)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.setGuitarType (GuitarType::Dreadnought);

        auto& character = engine.getCharacterEngine();
        character.setEnabled (true);
        character.setAmount (1.0);
        character.refret();
        character.setFretWear (7, wear);

        SqueakSettings always;
        always.probability = 1.0;
        always.moisture = 0.3;   // clear of the roughness ceiling, still certain to roll
        engine.setSqueak (always);

        const int wound = engine.getNumStrings() - 1;

        NoteOnEvent pluck;
        pluck.stringIndex = wound;
        pluck.fretPosition = 2.0;
        engine.triggerNoteNow (pluck);

        NoteOnEvent slide = pluck;
        slide.technique = Technique::Slide;
        slide.slideFromFret = 2.0;
        slide.fretPosition = 7.0;
        slide.slideSeconds = 0.12;
        engine.triggerNoteNow (slide);

        return activeLevel (engine.getPlayingNoise().getPool(), NoiseClass::squeak);
    };

    const double fresh = levelWithWear (0.0);
    const double worn = levelWithWear (1.0);

    CHECK (fresh > 0.0);
    CHECK_MSG (worn > fresh * 1.1 && worn < fresh * 1.2,
               "a fully worn fret should raise squeak by about 15 %, got x" + juce::String (worn / juce::jmax (1.0e-12, fresh)));
}

//==============================================================================
//  fret-buzz
//==============================================================================
LUTHIER_TEST (Buzz, fretWearMovesTheBuzz)
{
    // FB-21 (fret-buzz.md 8): a worn fret is lower - more clearance over it -
    // but a note fretted on it starts lower, so the fret ahead comes closer.
    const auto fresh = needsATech();
    auto worn = fresh;
    worn.fretWearMm[3] = 0.3;

    const int low = fresh.numStrings - 1;

    CHECK_MSG (worn.clearanceMm (low, 2.0, 3) > fresh.clearanceMm (low, 2.0, 3) + 0.2,
               "a worn fret 3 should clear a note fretted at 2 more easily");
    CHECK_MSG (worn.clearanceMm (low, 3.0, 4) < fresh.clearanceMm (low, 3.0, 4) - 0.1,
               "a note fretted on a worn fret 3 should sit closer to fret 4");

    // The setup itself is measured against the board line: the 12th-fret
    // action does not move because some other fret wore.
    CHECK_NEAR (worn.clearanceMm (low, 0.0, 12), fresh.clearanceMm (low, 0.0, 12), 1.0e-9);

    // And the engine carries the instrument's own wear into the buzz model.
    LuthierEngine engine;
    engine.prepare (48000.0, 256);
    auto& character = engine.getCharacterEngine();
    character.setEnabled (true);
    character.setAmount (1.0);
    character.refret();
    character.setFretWear (3, 1.0);
    engine.setSetupGeometry (fresh);

    CHECK_MSG (engine.getFretBuzz().getGeometry().fretWearMm[3] > 0.3,
               "the engine did not pass fret 3's wear to the buzz model");
    CHECK (engine.getFretBuzz().getGeometry().fretWearMm[5] == 0.0);

    character.setEnabled (false);
    engine.setSetupGeometry (fresh);
    CHECK (engine.getFretBuzz().getGeometry().fretWearMm[3] == 0.0);
}

//==============================================================================
//  practice-tools
//==============================================================================
LUTHIER_TEST (PracticeMetronome, followsTheHostAndTheTap)
{
    // PT-6 (practice-tools 1, live-performance 5).
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    processor.setPracticePanelOpen (true);

    FakePlayHead host;
    processor.setPlayHead (&host);

    auto& metronome = processor.getMetronome();
    metronome.setEnabled (true);
    CHECK_MSG (metronome.getFollowsTempo(), "a new session's click should follow the tempo");

    host.bpm = 90.0;
    host.playing = true;
    host.numerator = 3;
    host.denominator = 4;
    runBlocks (processor, 2);
    CHECK_NEAR (metronome.getTempo(), 90.0, 1.0e-6);

    // HI-29 (host-integration 6): and the host's metre.
    CHECK (metronome.getTimeSignature().numerator == 3);
    CHECK (metronome.getTimeSignature().denominator == 4);

    // Host stopped: a tapped tempo wins.
    host.playing = false;
    processor.getTapTempo().setBpm (140.0);
    runBlocks (processor, 2);
    CHECK_NEAR (metronome.getTempo(), 140.0, 1.0e-6);

    // A typed tempo takes over.
    metronome.setFollowsTempo (false);
    metronome.setTempo (70.0);
    runBlocks (processor, 2);
    CHECK_NEAR (metronome.getTempo(), 70.0, 1.0e-6);

    // Round trip, and a session saved before following keeps its tempo.
    Metronome copy;
    copy.fromVar (metronome.toVar());
    CHECK (! copy.getFollowsTempo());

    metronome.setFollowsTempo (true);
    copy.fromVar (metronome.toVar());
    CHECK (copy.getFollowsTempo());

    auto old = metronome.toVar();
    old.getDynamicObject()->removeProperty ("followsTempo");
    copy.fromVar (old);
    CHECK (! copy.getFollowsTempo());

    processor.setPlayHead (nullptr);
    processor.releaseResources();
}

LUTHIER_TEST (PracticeTrainers, aPlayedNoteAnswersTheQuiz)
{
    // PT-34 (practice-tools 4): notes played while the drawer is open reach
    // the trainers; with it closed they cost nothing and go nowhere.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto playNote = [&processor] (int note)
    {
        juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 10);
        buffer.clear();
        processor.processBlock (buffer, midi);
    };

    auto& feed = processor.getPracticeNoteFeed();
    int popped = -1;

    playNote (60);
    CHECK_MSG (! feed.pop (popped), "a closed drawer should not collect notes");

    processor.setPracticePanelOpen (true);
    playNote (64);
    CHECK (feed.pop (popped));
    CHECK (popped == 64);
    CHECK (! feed.pop (popped));

    // The SCALE tab answers its quiz with the note, and asks the next.
    ScaleTab tab (processor);
    auto& trainer = processor.getScaleTrainer();
    trainer.setMode (ScaleTrainer::Mode::quiz);
    trainer.resetScore();

    juce::Random random (7);
    trainer.nextQuestion (random);
    const int expected = trainer.getExpectedPitchClass();
    CHECK (expected >= 0);

    tab.notePlayed (60 + ((expected + 1) % 12));
    CHECK_MSG (trainer.getScore() == 0, "a wrong note scored");

    const int askedBefore = trainer.getAsked();
    tab.notePlayed (60 + expected);
    CHECK_MSG (trainer.getScore() == 1, "the right note did not score");
    CHECK_MSG (trainer.getAsked() == askedBefore + 1, "a right answer should pose the next question");

    processor.releaseResources();
}

LUTHIER_TEST (PracticeTrainers, earQuestionsPlayThroughTheEngine)
{
    // PT-39 (practice-tools 5): the question's pitches are the ones heard.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto& tuning = processor.getEngine().getTuningEngine();
    const int numStrings = processor.getEngine().getNumStrings();

    const int question[] = { 45, 52, 57, 61 };   // A2, E3, A3, C#4
    int strings[4] {}, frets[4] {};
    EarTab::placeOnStrings (tuning, numStrings, question, 4, strings, frets);

    std::set<int> usedStrings;

    for (int i = 0; i < 4; ++i)
    {
        CHECK (strings[i] >= 0);
        usedStrings.insert (strings[i]);

        const double hz = tuning.computeFrequency (strings[i], (double) frets[i]);
        CHECK_MSG ((int) std::round (hzToMidi (hz, tuning.getConcertA())) == question[i],
                   "note " + juce::String (question[i]) + " was placed where it sounds something else");
    }

    CHECK_MSG ((int) usedStrings.size() == 4, "two notes of one question shared a string");

    // And through the processor's preview path, the engine rings those notes.
    for (int i = 0; i < 4; ++i)
        processor.triggerPreviewNote (strings[i], (double) frets[i], 0.75);

    runBlocks (processor, 2);

    for (int i = 0; i < 4; ++i)
        CHECK_MSG (processor.getEngine().getStringMidiNote (strings[i]) == question[i],
                   "string " + juce::String (strings[i]) + " is not ringing " + juce::String (question[i]));

    processor.releaseResources();
}

//==============================================================================
namespace
{
    /** A sine written to a temporary file in the given format. */
    juce::File writeSine (juce::AudioFormat& format, const juce::String& extension, double hz,
                          double seconds, double sampleRate = 48000.0)
    {
        auto file = juce::File::createTempFile (extension);
        file.deleteFile();

        const int n = (int) (seconds * sampleRate);
        juce::AudioBuffer<float> audio (2, n);

        for (int i = 0; i < n; ++i)
        {
            const float v = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / sampleRate);
            audio.setSample (0, i, v);
            audio.setSample (1, i, v);
        }

        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());

        if (stream == nullptr)
            return {};

        std::unique_ptr<juce::AudioFormatWriter> writer (
            format.createWriterFor (stream.get(), sampleRate, 2, 16, {}, 0));

        if (writer == nullptr)
            return {};

        stream.release();
        writer->writeFromAudioSampleBuffer (audio, 0, n);
        return file;
    }

    /** Frequency from rising zero crossings, over a stretch of mono audio. */
    double zeroCrossingHz (const float* x, int n, double sampleRate)
    {
        int first = -1, last = -1, count = 0;

        for (int i = 1; i < n; ++i)
        {
            if (x[i - 1] <= 0.0f && x[i] > 0.0f)
            {
                if (first < 0)
                    first = i;

                last = i;
                ++count;
            }
        }

        return (count > 1 && last > first) ? (count - 1) * sampleRate / (last - first) : 0.0;
    }

    /** Plays a loaded track to its end (or `maxSeconds`), returning the rendered left channel. */
    std::vector<float> playTrack (BackingTrackPlayer& player, double maxSeconds)
    {
        juce::AudioBuffer<float> block (2, 512);
        std::vector<float> rendered;

        // The file streams on its own thread; give it time to fill the ring.
        juce::Thread::sleep (400);
        player.play();

        while (player.isPlaying() && (double) rendered.size() < maxSeconds * 48000.0)
        {
            player.processBlock (block, 512);
            rendered.insert (rendered.end(), block.getReadPointer (0), block.getReadPointer (0) + 512);
        }

        return rendered;
    }
}

LUTHIER_TEST (PracticeTrack, loadsEveryAdvertisedFormat)
{
    // PT-24 (practice-tools 3): WAV, AIFF, FLAC and MP3 are offered, so all
    // four must have a reader.
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    for (auto* extension : { ".wav", ".aif", ".aiff", ".flac", ".mp3", ".ogg" })
        CHECK_MSG (formats.findFormatForFileExtension (extension) != nullptr,
                   juce::String ("no reader for ") + extension);

    juce::WavAudioFormat wav;
    juce::AiffAudioFormat aiff;
    juce::FlacAudioFormat flac;

    const std::pair<juce::AudioFormat*, const char*> written[] = { { &wav, ".wav" }, { &aiff, ".aiff" }, { &flac, ".flac" } };

    for (const auto& [format, extension] : written)
    {
        const auto file = writeSine (*format, extension, 440.0, 0.5);
        CHECK (file.existsAsFile());

        BackingTrackPlayer player;
        player.prepare (48000.0, 512);
        CHECK_MSG (player.load (file), juce::String ("could not load a generated ") + extension);
        CHECK_NEAR (player.getLengthSeconds(), 0.5, 0.01);

        player.unload();
        file.deleteFile();
    }
}

LUTHIER_TEST (PracticeTrack, pitchShiftMovesPitchNotTempo)
{
    // PT-28: +12 semitones is an octave up, and the track takes as long.
    juce::WavAudioFormat wav;
    const auto file = writeSine (wav, ".wav", 440.0, 2.0);

    BackingTrackPlayer player;
    player.prepare (48000.0, 512);
    CHECK (player.load (file));
    player.setLevelDb (0.0);
    player.setPitchShiftSemitones (12.0);

    const auto rendered = playTrack (player, 6.0);
    const double seconds = (double) rendered.size() / 48000.0;

    CHECK_MSG (seconds > 1.8 && seconds < 2.3, "a pitch shift changed the duration: " + juce::String (seconds) + " s");

    const double hz = zeroCrossingHz (rendered.data() + 24000, 24000, 48000.0);
    CHECK_MSG (std::abs (hz - 880.0) < 8.0, "+12 semitones of 440 Hz came out at " + juce::String (hz) + " Hz");

    // And a fifth down.
    player.stop();
    player.setPitchShiftSemitones (-7.0);
    const auto lower = playTrack (player, 6.0);
    const double lowerHz = zeroCrossingHz (lower.data() + 24000, 24000, 48000.0);
    CHECK_MSG (std::abs (lowerHz - 440.0 * std::pow (2.0, -7.0 / 12.0)) < 5.0,
               "-7 semitones came out at " + juce::String (lowerHz) + " Hz");

    player.unload();
    file.deleteFile();
}

LUTHIER_TEST (PracticeTrack, tempoShiftChangesDurationNotPitch)
{
    // PT-29: half speed takes twice as long at the same pitch; 150 % two thirds.
    juce::WavAudioFormat wav;
    const auto file = writeSine (wav, ".wav", 440.0, 2.0);

    for (const double ratio : { 0.5, 1.5 })
    {
        BackingTrackPlayer player;
        player.prepare (48000.0, 512);
        CHECK (player.load (file));
        player.setLevelDb (0.0);
        player.setTempoRatio (ratio);

        const auto rendered = playTrack (player, 8.0);
        const double seconds = (double) rendered.size() / 48000.0;
        const double expected = 2.0 / ratio;

        CHECK_MSG (std::abs (seconds - expected) < 0.25,
                   "at " + juce::String (ratio) + "x the track took " + juce::String (seconds) + " s, not "
                     + juce::String (expected));

        const double hz = zeroCrossingHz (rendered.data() + 12000, 24000, 48000.0);
        CHECK_MSG (std::abs (hz - 440.0) < 5.0,
                   "a tempo shift of " + juce::String (ratio) + " moved the pitch to " + juce::String (hz) + " Hz");

        player.unload();
    }

    // Neutral settings are a bypass: no latency, sample for sample the file.
    CHECK (TimePitchShifter::isNeutral (1.0, 0.0));
    CHECK (! TimePitchShifter::isNeutral (1.0, 0.5));

    file.deleteFile();
}

//==============================================================================
//  tone-match
//==============================================================================
namespace
{
    /** An IR file: decaying noise (or an impulse) with `channels` channels. */
    juce::File writeIr (juce::AudioFormat& format, const juce::String& extension, double seconds,
                        int channels = 1, double sampleRate = 48000.0, bool impulse = false)
    {
        auto file = juce::File::createTempFile (extension);
        file.deleteFile();

        const int n = juce::jmax (1, (int) (seconds * sampleRate));
        juce::AudioBuffer<float> audio (channels, n);
        audio.clear();
        juce::Random random (0x1f);

        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < n; ++i)
                audio.setSample (c, i, impulse ? (i == c * 10 ? 0.9f : 0.0f)
                                               : (random.nextFloat() * 2.0f - 1.0f) * 0.5f
                                                   * (float) std::exp (-(double) i / (sampleRate * 0.08)));

        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());

        if (stream == nullptr)
            return {};

        std::unique_ptr<juce::AudioFormatWriter> writer (
            format.createWriterFor (stream.get(), sampleRate, (unsigned int) channels, 24, {}, 0));

        if (writer == nullptr)
            return {};

        stream.release();
        writer->writeFromAudioSampleBuffer (audio, 0, n);
        return file;
    }

    /** Renders one dreadnought note, optionally through a body IR slot. */
    std::vector<float> renderBodyNote (IrSlot* slot)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 512);
        engine.setGuitarType (GuitarType::Dreadnought);
        engine.setBodyIrSlot (slot);

        NoteOnEvent note;
        note.stringIndex = 4;
        note.fretPosition = 2.0;
        note.velocity = 0.8;
        note.pitchHz = engine.getTuningEngine().computeFrequency (4, 2.0);
        engine.triggerNoteNow (note);

        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;

        for (int b = 0; b < 40; ++b)
        {
            buffer.clear();
            engine.processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 512);
        }

        return out;
    }
}

LUTHIER_TEST (ToneMatch, anEngagedBodyIrChangesTheSound)
{
    // TM-6 (tone-match 1): the body slot replaces the body's response.
    const auto reference = renderBodyNote (nullptr);

    juce::WavAudioFormat wav;
    const auto file = writeIr (wav, ".wav", 0.3);

    IrSlot slot;
    slot.prepare (48000.0, 512);
    CHECK (slot.load (file));

    // Loaded but not engaged: the body is untouched, sample for sample.
    const auto idle = renderBodyNote (&slot);
    double idleDiff = 0.0;

    for (size_t i = 0; i < reference.size(); ++i)
        idleDiff = juce::jmax (idleDiff, (double) std::abs (reference[i] - idle[i]));

    CHECK_MSG (idleDiff == 0.0, "a disengaged body IR changed the audio by " + juce::String (idleDiff));

    // Engaged: the convolution swaps its response in off the audio thread, so
    // give it a moment and a block to land.
    slot.setEngaged (true);
    slot.setMix (1.0);
    juce::Thread::sleep (300);

    {
        std::vector<float> warm (512, 0.0f);
        slot.processReplacing (warm.data(), warm.data(), 512);
    }

    const auto engaged = renderBodyNote (&slot);
    double diff = 0.0, energy = 0.0;

    for (size_t i = 0; i < reference.size(); ++i)
    {
        diff += (double) (reference[i] - engaged[i]) * (reference[i] - engaged[i]);
        energy += (double) reference[i] * reference[i];
        CHECK (std::isfinite (engaged[i]));
    }

    CHECK (energy > 0.0);
    CHECK_MSG (diff > energy * 0.01, "an engaged body IR barely changed the sound");

    slot.unload();
    file.deleteFile();
}

LUTHIER_TEST (ToneMatch, loadingSwapsTheResponseWithoutAGap)
{
    // TM-1: a second load mid-stream hands over without a crash or a NaN.
    juce::WavAudioFormat wav;
    const auto first = writeIr (wav, ".wav", 0.2);
    const auto second = writeIr (wav, ".wav", 0.4);

    IrSlot slot;
    slot.prepare (48000.0, 256);
    CHECK (slot.load (first));
    slot.setEngaged (true);

    const auto firstName = slot.getName();
    juce::AudioBuffer<float> buffer (2, 256);
    bool finite = true;

    for (int b = 0; b < 60; ++b)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 256; ++i)
                buffer.setSample (c, i, (float) std::sin (0.03 * (b * 256 + i)));

        if (b == 20)
            CHECK (slot.load (second));

        slot.process (buffer.getArrayOfWritePointers(), 2, 256);

        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 256; ++i)
                finite = finite && std::isfinite (buffer.getSample (c, i));

        juce::Thread::sleep (1);
    }

    CHECK (finite);
    CHECK (slot.getName() != firstName);
    CHECK (slot.getName() == second.getFileNameWithoutExtension());

    slot.unload();
    first.deleteFile();
    second.deleteFile();
}

LUTHIER_TEST (ToneMatch, longIrsAreTruncatedToMaxSeconds)
{
    // TM-3 (tone-match 0.3).
    juce::WavAudioFormat wav;
    const auto file = writeIr (wav, ".wav", 6.0);

    IrSlot slot;
    slot.prepare (48000.0, 512);
    CHECK (slot.load (file));
    CHECK_MSG (slot.getLengthMs() <= 4000.5, "a 6 s IR kept " + juce::String (slot.getLengthMs()) + " ms");

    slot.setMaxSeconds (2.0);
    CHECK (slot.getLengthMs() <= 2000.5);
    CHECK (slot.getLengthMs() > 1900.0);

    slot.unload();
    file.deleteFile();
}

LUTHIER_TEST (ToneMatch, loadsWavAiffFlacUpToSixChannels)
{
    // TM-8 / TM-9 (tone-match 1).
    juce::WavAudioFormat wav;
    juce::AiffAudioFormat aiff;
    juce::FlacAudioFormat flac;

    const std::pair<juce::AudioFormat*, const char*> formats[] = { { &wav, ".wav" }, { &aiff, ".aiff" }, { &flac, ".flac" } };

    for (const auto& [format, extension] : formats)
    {
        for (const int channels : { 1, 2, 6 })
        {
            const auto file = writeIr (*format, extension, 0.1, channels);

            if (! file.existsAsFile())
                continue;   // a format without that many channels is not the slot's fault

            IrSlot slot;
            slot.prepare (48000.0, 512);
            CHECK_MSG (slot.load (file), juce::String (channels) + "-channel " + extension + " did not load: "
                                           + slot.getLastError());
            slot.unload();
            file.deleteFile();
        }
    }

    // Garbage says why.
    auto garbage = juce::File::createTempFile (".wav");
    garbage.replaceWithText ("not audio at all");

    IrSlot slot;
    slot.prepare (48000.0, 512);
    CHECK (! slot.load (garbage));
    CHECK (slot.getLastError().isNotEmpty());
    garbage.deleteFile();

    // A stereo IR with different channels: choosing one reads that one. The
    // impulse of channel c sits at sample 10 c, so the choice moves the peak.
    const auto stereo = writeIr (wav, ".wav", 0.05, 2, 48000.0, true);
    IrSlot pick;
    pick.prepare (48000.0, 512);
    pick.setChannel (1);
    CHECK (pick.load (stereo));
    CHECK (pick.getChannel() == 1);

    const auto saved = pick.toVar();
    IrSlot restored;
    restored.prepare (48000.0, 512);
    restored.fromVar (saved);
    CHECK_MSG (restored.getChannel() == 1, "the channel choice did not survive a save");

    pick.unload();
    restored.unload();
    stereo.deleteFile();
}

//==============================================================================
namespace
{
    template <typename Type>
    Type* findByTitle (juce::Component& root, const juce::String& title)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* typed = dynamic_cast<Type*> (child); typed != nullptr && child->getTitle() == title)
                return typed;

            if (auto* found = findByTitle<Type> (*child, title))
                return found;
        }

        return nullptr;
    }

    juce::TextButton* findButton (juce::Component& root, const juce::String& text)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* button = dynamic_cast<juce::TextButton*> (child); button != nullptr && button->getButtonText() == text)
                return button;

            if (auto* found = findButton (*child, text))
                return found;
        }

        return nullptr;
    }
}

LUTHIER_TEST (PracticePanelUi, scaleOverlaysChangeTheLabels)
{
    // PT-33 (practice-tools 4).
    LuthierAudioProcessor processor;
    ScaleTab tab (processor);
    tab.setSize (700, 200);

    auto& trainer = processor.getScaleTrainer();
    trainer.setKey (0);
    trainer.setScale (ScaleType::ionian);

    CHECK (tab.labelFor (4, ScaleTab::Overlay::notes) == "E");
    CHECK (tab.labelFor (4, ScaleTab::Overlay::intervals) == "3");
    CHECK (tab.labelFor (5, ScaleTab::Overlay::degrees) == "4");
    CHECK (tab.labelFor (1, ScaleTab::Overlay::notes).isEmpty());

    trainer.setKey (9);   // A major
    CHECK (tab.labelFor (1, ScaleTab::Overlay::notes) == "C#");
    CHECK (tab.labelFor (1, ScaleTab::Overlay::intervals) == "3");

    // It draws.
    juce::Image image (juce::Image::ARGB, 700, 200, true);
    juce::Graphics g (image);
    tab.paintEntireComponent (g, false);
}

LUTHIER_TEST (PracticePanelUi, aCustomScaleIsHighlighted)
{
    // PT-37 (practice-tools 4): steps in, the scale's notes out.
    LuthierAudioProcessor processor;
    ScaleTab tab (processor);

    auto& trainer = processor.getScaleTrainer();
    trainer.setKey (0);

    CHECK (tab.setCustomSteps ("2 1 2 2 1 2 2"));   // natural minor
    CHECK (trainer.getScale() == ScaleType::custom);
    CHECK (trainer.containsPitchClass (3));
    CHECK (! trainer.containsPitchClass (4));
    CHECK (trainer.getNumDegrees() == 7);

    CHECK (tab.setCustomSteps ("3 2 2 3"));          // minor pentatonic, stopping short
    CHECK (trainer.containsPitchClass (10));
    CHECK (trainer.getNumDegrees() == 5);

    CHECK (! tab.setCustomSteps ("5 5 5"));          // past the octave
    CHECK (! tab.setCustomSteps ("two"));
}

LUTHIER_TEST (PracticeTrainers, fifteenProgressions)
{
    // PT-38 (practice-tools 5): five named and ten more.
    CHECK (EarTrainer::kNumProgressions == 15);

    EarTrainer trainer;
    trainer.setExercise (EarTrainer::Exercise::progression);
    trainer.setDifficulty (4);

    juce::Random random (3);
    int notes[EarTrainer::kMaxNotesInQuestion] {};
    double offsets[EarTrainer::kMaxNotesInQuestion] {};
    trainer.nextQuestion (random, notes, offsets, EarTrainer::kMaxNotesInQuestion);

    CHECK (trainer.getChoices().size() == 15);
    CHECK (trainer.getChoices().contains ("I-IV-vi-V"));
}

LUTHIER_TEST (PracticeLooper, layerFiltersCutWhatTheySay)
{
    // PT-20 (practice-tools 2): each layer strip has a low-cut and a high-cut.
    LuthierAudioProcessor processor;
    LooperTab tab (processor);
    tab.setSize (800, 400);

    auto* low = findByTitle<juce::Slider> (tab, "Layer 2 low-cut");
    auto* high = findByTitle<juce::Slider> (tab, "Layer 2 high-cut");
    CHECK (low != nullptr);
    CHECK (high != nullptr);

    if (low == nullptr || high == nullptr)
        return;

    low->setValue (300.0, juce::sendNotificationSync);
    high->setValue (5000.0, juce::sendNotificationSync);

    CHECK_NEAR (processor.getLooper().getLayer (1).getLowCutHz(), 300.0, 0.5);
    CHECK_NEAR (processor.getLooper().getLayer (1).getHighCutHz(), 5000.0, 0.5);
    CHECK_NEAR (processor.getLooper().getLayer (0).getLowCutHz(), 20.0, 0.5);
}

LUTHIER_TEST (PracticeTrack, panAndFiltersShapeTheTrack)
{
    // PT-26 and PT-30 (practice-tools 3).
    LuthierAudioProcessor processor;
    TrackTab tab (processor);
    tab.setSize (800, 300);

    auto* pan = findByTitle<juce::Slider> (tab, "Track pan");
    auto* low = findByTitle<juce::Slider> (tab, "Track low-cut");
    auto* high = findByTitle<juce::Slider> (tab, "Track high-cut");
    CHECK (pan != nullptr && low != nullptr && high != nullptr);

    if (pan == nullptr || low == nullptr || high == nullptr)
        return;

    pan->setValue (-0.5, juce::sendNotificationSync);
    low->setValue (150.0, juce::sendNotificationSync);
    high->setValue (8000.0, juce::sendNotificationSync);

    auto& track = processor.getBackingTrack();
    CHECK_NEAR (track.getPan(), -0.5, 1.0e-6);
    CHECK_NEAR (track.getLowCutHz(), 150.0, 0.5);
    CHECK_NEAR (track.getHighCutHz(), 8000.0, 0.5);

    // A panned track is louder on its side.
    juce::WavAudioFormat wav;
    const auto file = writeSine (wav, ".wav", 1000.0, 1.0);
    CHECK (track.load (file));
    track.prepare (48000.0, 512);
    track.setLowCutHz (20.0);
    track.setHighCutHz (20000.0);
    track.setPan (-1.0);

    juce::AudioBuffer<float> block (2, 512);
    juce::Thread::sleep (300);
    track.play();

    for (int b = 0; b < 8; ++b)
        track.processBlock (block, 512);
    CHECK_MSG (block.getRMSLevel (0, 0, 512) > 10.0f * block.getRMSLevel (1, 0, 512) + 1.0e-6f,
               "a hard-left track was not on the left");
    track.unload();
    file.deleteFile();

    // Markers keep the name typed for them.
    auto* name = findByTitle<juce::TextEditor> (tab, "Marker name");
    auto* mark = findButton (tab, "Mark");
    CHECK (name != nullptr && mark != nullptr);

    if (name != nullptr && mark != nullptr)
    {
        name->setText ("Chorus");
        mark->onClick();
        mark->onClick();

        CHECK (track.getNumMarkers() == 2);

        juce::StringArray names;

        for (int i = 0; i < track.getNumMarkers(); ++i)
            names.add (track.getMarker (i).name);

        CHECK (names.contains ("Chorus"));
        CHECK (names.contains ("Marker 2"));
    }
}

LUTHIER_TEST (ToneMatch, cabMatchPlaysItsTestSignalOutOfAuxOne)
{
    // TM-17 (tone-match 2): step 1 plays the test signal to the rig and starts
    // the reference capture on the same sample.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    std::vector<float> signal (2000);

    for (size_t i = 0; i < signal.size(); ++i)
        signal[i] = 0.25f * (float) std::sin (0.01 * (double) i * (double) i * 0.001);

    auto& capture = processor.getCapture();
    capture.reset();
    capture.setSource (Capture::Source::sidechain);
    processor.getCabMatchSignal().arm (signal, 1.0);

    CHECK (! capture.isRecording());

    const bool auxOut = processor.getBusCount (false) > 1 && processor.getBus (false, 1) != nullptr
                          && processor.getBus (false, 1)->isEnabled();
    const int outChannel = auxOut ? processor.getChannelIndexInProcessBlockBuffer (false, 1, 0) : 0;

    juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), 512);
    std::vector<float> heard;

    for (int b = 0; b < 5; ++b)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);
        heard.insert (heard.end(), buffer.getReadPointer (outChannel), buffer.getReadPointer (outChannel) + 512);

        if (b == 0)
            CHECK_MSG (capture.isRecording(), "the reference capture did not start with the signal");
    }

    double error = 0.0;

    for (size_t i = 0; i < signal.size(); ++i)
        error = juce::jmax (error, (double) std::abs (heard[i] - signal[i]));

    CHECK_MSG (error < 1.0e-6, "the test signal was not what came out, off by " + juce::String (error));
    CHECK (! processor.getCabMatchSignal().isPlaying());

    processor.releaseResources();
}

//==============================================================================
//  midi-export
//==============================================================================
LUTHIER_TEST (MidiImport, theNotificationListsDefaultedFields)
{
    // MX-28 (midi-export 10/11): a Luthier event missing documented fields is
    // read with defaults, and the import says which.
    MidiPerformance performance (48000.0);
    performance.setTempo (120.0);
    performance.addMessage (0, juce::MidiMessage::noteOn (1, 40, (juce::uint8) 100));
    performance.addMessage (20000, juce::MidiMessage::noteOff (1, 40));
    performance.addEvent (LuthierEvent::make (LuthierEventClass::strum, 0));

    auto file = juce::File::createTempFile (".mid");
    MidiExportOptions options;
    options.profile = MidiProfile::luthier;
    juce::String error;
    CHECK_MSG (MidiProfiles::exportToFile (performance, options, file, &error), error);

    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    const auto outcome = MidiImportTargets::importFile (processor, file, MidiImportTarget::session);
    CHECK_MSG (outcome.ok, outcome.message);
    CHECK (! outcome.read.defaultedFields.isEmpty());
    CHECK_MSG (outcome.message.contains ("Defaults used for: STRUM."), outcome.message);

    file.deleteFile();
}

LUTHIER_TEST (ToneMatch, thePanelReachesTrimBandLengthAndSearch)
{
    // TM-11, TM-25, TM-31, TM-33, TM-38 (tone-match 1, 3, 4, 6).
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    juce::WavAudioFormat wav;
    const auto file = writeIr (wav, ".wav", 0.2);
    CHECK (processor.getCabIrSlot (0).load (file));

    ToneMatchPanel panel (processor);
    panel.setSize (900, panel.preferredHeight());

    auto* start = findByTitle<juce::Slider> (panel, "Cabinet IR 1 start trim");
    auto* end = findByTitle<juce::Slider> (panel, "Cabinet IR 1 end trim");
    CHECK (start != nullptr && end != nullptr);

    if (start != nullptr && end != nullptr)
    {
        start->setValue (100.0, juce::sendNotificationSync);
        end->setValue (200.0, juce::sendNotificationSync);
        CHECK (processor.getCabIrSlot (0).getStartTrim() == 100);
        CHECK (processor.getCabIrSlot (0).getEndTrim() == 200);
    }

    CHECK (findByTitle<juce::Slider> (panel, "EQ match low band edge") != nullptr);
    CHECK (findByTitle<juce::Slider> (panel, "EQ match high band edge") != nullptr);
    CHECK (findByTitle<juce::Slider> (panel, "Capture length") != nullptr);
    CHECK (findButton (panel, "Auto-trim silence") != nullptr);

    // The library search.
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-ir-search");
    dir.deleteRecursively();
    dir.createDirectory();
    const auto greenback = dir.getChildFile ("Greenback 4x12.wav");
    const auto vintage = dir.getChildFile ("Vintage 1x12.wav");
    file.copyFileTo (greenback);
    file.copyFileTo (vintage);

    panel.setLibraryFilesForTesting ({ greenback, vintage });
    CHECK (panel.getVisibleLibraryFiles().size() == 2);

    auto* search = findByTitle<juce::TextEditor> (panel, "Search IRs");
    CHECK (search != nullptr);

    if (search != nullptr)
    {
        search->setText ("greenback", false);
        search->onTextChange();
        CHECK (panel.getVisibleLibraryFiles().size() == 1);
        CHECK (panel.getVisibleLibraryFiles()[0] == greenback);

        search->setText ("", false);
        search->onTextChange();
        CHECK (panel.getVisibleLibraryFiles().size() == 2);
    }

    processor.getCabIrSlot (0).unload();
    dir.deleteRecursively();
    file.deleteFile();
}

//==============================================================================
//  slide-guitar
//==============================================================================
LUTHIER_TEST (Slide, tooMuchPressureChokes)
{
    // SG-10 (slide-guitar.md 3): past firm, the string is pressed onto the
    // frets and loses sustain; up to firm, more pressure sustains more.
    auto scaleAt = [] (double pressure)
    {
        SlideEngine slide;
        slide.prepare (48000.0);

        SlideSettings settings;
        settings.enabled = true;
        settings.mode = SlideMode::lapSteel;
        settings.pressure = pressure;
        slide.setSettings (settings);
        slide.noteOn (2, 6);
        return slide.sustainScale (2);
    };

    CHECK (scaleAt (0.6) > scaleAt (0.3));
    CHECK (scaleAt (0.8) >= scaleAt (0.6));
    CHECK_MSG (scaleAt (1.0) < scaleAt (0.6) * 0.8,
               "full pressure should choke: " + juce::String (scaleAt (1.0)) + " vs " + juce::String (scaleAt (0.6)));
}

//==============================================================================
//  host-integration
//==============================================================================
LUTHIER_TEST (PluginBuses, monoMainOutputIsRefused)
{
    // HI-10 (host-integration 2): stereo main out only.
    LuthierAudioProcessor processor;
    auto layout = processor.getBusesLayout();

    CHECK (processor.checkBusesLayoutSupported (layout));

    layout.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
    CHECK_MSG (! processor.checkBusesLayoutSupported (layout), "a mono main output was accepted");
}

LUTHIER_TEST (PluginBuses, midiOutputIsAnnounced)
{
    // HI-2 / HI-31 (host-integration 0.3, 7): the processor says it makes MIDI,
    // and the plugin target announces it, so VST3 and AU get an event output.
    LuthierAudioProcessor processor;
    CHECK (processor.producesMidi());

    const auto cmake = juce::File (__FILE__).getParentDirectory().getParentDirectory().getParentDirectory()
                           .getChildFile ("CMakeLists.txt");

    if (cmake.existsAsFile())
    {
        const auto text = cmake.loadFileAsString();
        CHECK_MSG (text.contains ("NEEDS_MIDI_OUTPUT           TRUE"), "CMakeLists.txt does not announce MIDI output");
        CHECK (! text.contains ("NEEDS_MIDI_OUTPUT           FALSE"));
    }
}

//==============================================================================
//  string-squeak: the tests the audit found missing
//==============================================================================
namespace
{
    StringNoiseInfo woundE()
    {
        StringNoiseInfo info;
        info.wound = true;
        info.windingPitchPerMm = 6.5;
        info.windingDepth = 1.0;
        info.material = StringMaterial::PhosphorBronze;
        return info;
    }
}

LUTHIER_TEST (Squeak, moistureLowersOddsAndBrightness)
{
    // SQ-16 (string-squeak.md 6).
    SqueakSettings dry, damp;
    dry.moisture = 0.1;
    damp.moisture = 0.9;
    dry.probability = damp.probability = 0.8;

    const auto a = PlayingNoise::makeSqueak (dry, woundE(), 5, 120.0, 0.25, 5.0);
    const auto b = PlayingNoise::makeSqueak (damp, woundE(), 5, 120.0, 0.25, 5.0);
    CHECK (a.brightness > b.brightness);
    CHECK (a.level > b.level);

    auto hits = [] (const SqueakSettings& settings)
    {
        PlayingNoise noise;
        noise.prepare (48000.0);
        noise.setSqueak (settings);
        int count = 0;

        for (juce::uint32 i = 0; i < 256; ++i)
        {
            if (noise.onShift (5, woundE(), 648.0, 2.0, 7.0, 0.12, i))
                ++count;

            noise.getPool().reset();
        }

        return count;
    };

    CHECK_MSG (hits (dry) > hits (damp) + 20, "damp fingers should squeak less often");
}

LUTHIER_TEST (Squeak, pressureRaisesLevelAndCoarsensTexture)
{
    // SQ-17 (string-squeak.md 3): level follows pressure^1.3, and pressure
    // changes the texture's resonance.
    SqueakSettings light, firm;
    light.pressure = 0.4;
    firm.pressure = 0.8;

    const auto a = PlayingNoise::makeSqueak (light, woundE(), 5, 120.0, 0.25, 5.0);
    const auto b = PlayingNoise::makeSqueak (firm, woundE(), 5, 120.0, 0.25, 5.0);

    CHECK_NEAR (b.level / a.level, std::pow (2.0, 1.3), 1.0e-6);
    CHECK (a.q != b.q);
}

LUTHIER_TEST (Squeak, aBendAndVibratoDoNotSqueak)
{
    // SQ-9 / SQ-T7 (string-squeak.md 2): the finger does not travel along
    // the string in a bend or vibrato - pitch wheel at several depths, and a
    // 6 Hz wobble, on a held wound note.
    SqueakRig rig;
    rig.play (juce::MidiMessage::noteOn (6, 45, (juce::uint8) 100));
    CHECK (rig.squeaks() == 0);

    for (const int depth : { 1000, 3000, 8191 })
    {
        for (int b = 0; b < 40; ++b)
        {
            const double wobble = std::sin (juce::MathConstants<double>::twoPi * 6.0 * b * 256.0 / 48000.0);
            rig.play (juce::MidiMessage::pitchWheel (6, 8192 + (int) (wobble * depth * 0.99)));
        }

        rig.play (juce::MidiMessage::pitchWheel (6, juce::jmin (16383, 8192 + depth)));   // a full bend
    }

    CHECK_MSG (rig.squeaks() == 0, "a bend or vibrato squeaked " + juce::String (rig.squeaks()) + " times");
}

LUTHIER_TEST (Squeak, noFactoryGuitarSqueaksOnAPlainString)
{
    // SQ-T1 (string-squeak.md 0.1): every guitar, every plain string, a
    // five-fret legato slide with squeak forced on.
    for (int g = 0; g < (int) GuitarType::NumTypes; ++g)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.setGuitarType ((GuitarType) g);

        SqueakSettings always;
        always.probability = 1.0;
        always.moisture = 0.0;
        engine.setSqueak (always);

        for (int s = 0; s < engine.getNumStrings(); ++s)
        {
            if (engine.getStringSpec (s).wound)
                continue;

            NoteOnEvent pluck;
            pluck.stringIndex = s;
            pluck.fretPosition = 2.0;
            engine.triggerNoteNow (pluck);

            NoteOnEvent slide = pluck;
            slide.technique = Technique::Slide;
            slide.slideFromFret = 2.0;
            slide.fretPosition = 7.0;
            slide.slideSeconds = 0.12;
            engine.triggerNoteNow (slide);
        }

        CHECK_MSG (engine.getPlayingNoise().getPool().getTriggerCount (NoiseClass::squeak) == 0,
                   "guitar " + juce::String (g) + " squeaked on a plain string");
    }
}

//==============================================================================
//  ambiguity-resolutions
//==============================================================================
LUTHIER_TEST (Doubler, defaultsAreTheAdtOnes)
{
    // AR-14 (ambiguity-resolutions: the ADT defaults).
    DoublerPedal doubler;
    CHECK (doubler.getNumParameters() == 7);

    const double expected[] = { 22.0, -8.0, -0.7, 1.0, 40.0, 100.0, 8000.0 };

    for (int i = 0; i < 7; ++i)
        CHECK_NEAR (doubler.getParameterDescriptor (i).defaultValue, expected[i], 1.0e-9);

    CHECK (juce::String (doubler.getParameterDescriptor (3).choices[1]).containsIgnoreCase ("stereo"));
}

//==============================================================================
//  No allocation on the audio thread (engine.md 0)
//==============================================================================
namespace
{
    /** Renders `blocks` blocks of the engine, `each` run before every block,
        and returns the allocations it made after a warm-up. */
    template <typename Each>
    long allocationsWhile (LuthierEngine& engine, int blocks, Each&& each)
    {
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;
        midi.ensureSize (256);

        for (int b = 0; b < 8; ++b)   // warm-up: first-use paths
        {
            midi.clear();
            each (b, midi);
            buffer.clear();
            engine.processBlock (buffer, midi);
        }

       #if defined (LUTHIER_ALLOCATION_COUNTER)
        const auto before = allocationsOnThisThread();
       #endif

        for (int b = 0; b < blocks; ++b)
        {
            midi.clear();
            each (b, midi);
            buffer.clear();
            engine.processBlock (buffer, midi);
        }

       #if defined (LUTHIER_ALLOCATION_COUNTER)
        return allocationsOnThisThread() - before;
       #else
        return 0;
       #endif
    }
}

LUTHIER_TEST (Squeak, noAllocationOnTheAudioThread)
{
    // SQ-T10: shift after shift with squeak forced on.
    SqueakRig rig;
    const long count = allocationsWhile (rig.engine, 2000, [] (int b, juce::MidiBuffer& midi)
    {
        midi.addEvent (juce::MidiMessage::noteOn (6, 43 + (b % 2) * 5, (juce::uint8) 110), 0);
    });

    CHECK_MSG (count == 0, juce::String (count) + " allocations while squeaking");
}

LUTHIER_TEST (Buzz, noAllocationOnTheAudioThread)
{
    // FB-T10: "Needs a tech", then sitar mode, with hard notes on every string.
    for (const bool sitar : { false, true })
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.getMidiInterpreter().setPlayingMode (PlayingMode::GuitarController);

        auto setup = needsATech();
        setup.sitarMode = sitar;
        engine.setSetupGeometry (setup);

        const long count = allocationsWhile (engine, 1800, [] (int b, juce::MidiBuffer& midi)
        {
            if (b % 20 == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1 + (b / 20) % 6, 40 + (b / 20) % 24, (juce::uint8) 127), 0);
        });

        CHECK_MSG (count == 0, juce::String (count) + " allocations while buzzing" + (sitar ? " (sitar)" : ""));
    }
}

LUTHIER_TEST (Slide, noAllocationOnTheAudioThread)
{
    // SG-T10: bar moves and landings in lap-steel mode.
    LuthierEngine engine;
    engine.prepare (48000.0, 256);

    SlideSettings settings;
    settings.enabled = true;
    settings.mode = SlideMode::lapSteel;
    engine.setSlideSettings (settings);
    engine.getMidiInterpreter().setPlayingMode (PlayingMode::Mono);

    const long count = allocationsWhile (engine, 1800, [] (int b, juce::MidiBuffer& midi)
    {
        if (b % 10 == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 50 + (b / 10) % 12, (juce::uint8) 100), 0);

        if (b % 10 == 9)
            midi.addEvent (juce::MidiMessage::noteOff (1, 50 + (b / 10) % 12), 0);
    });

    CHECK_MSG (count == 0, juce::String (count) + " allocations while sliding");
}

LUTHIER_TEST (PracticeTrack, theTimePitchShiftDoesNotAllocate)
{
    // PT-28/29: the new audio-thread path.
    TimePitchShifter shifter;
    shifter.prepare();

    std::vector<float> left (512), right (512);
    double phase = 0.0;

    auto pull = [&phase] (float* l, float* r, int count)
    {
        for (int i = 0; i < count; ++i, phase += 0.05)
            l[i] = r[i] = (float) std::sin (phase);
    };

    shifter.process (left.data(), right.data(), 512, 0.5, 7.0, 512, pull);

   #if defined (LUTHIER_ALLOCATION_COUNTER)
    const auto before = allocationsOnThisThread();
   #endif

    for (int b = 0; b < 400; ++b)
        shifter.process (left.data(), right.data(), 512, b % 2 ? 1.7 : 0.4, b % 3 ? -12.0 : 5.0, 512, pull);

   #if defined (LUTHIER_ALLOCATION_COUNTER)
    CHECK_MSG (allocationsOnThisThread() == before, "the time/pitch shifter allocated");
   #endif

    for (auto v : left)
        CHECK (std::isfinite (v));
}
