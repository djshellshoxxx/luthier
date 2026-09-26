/*  SPEC-SWEEP phase 2 (dsp2): behaviour gaps closed in pick-noise, string-squeak,
    fret-buzz, slide, bass, practice-tools, tone-match and host-integration.

    Kept in a file of its own so the sweep's tests do not collide with the
    owning suites' files; each test's suite name is the spec's own.
*/

#include "TestFramework.h"

#include <set>

#include "../DSP/Noise/FretBuzz.h"
#include "../DSP/Noise/PlayingNoise.h"
#include "../DSP/Noise/ScrapeEngine.h"
#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../Practice/Metronome.h"
#include "../Practice/Looper.h"
#include "../DSP/Slide/SlideEngine.h"
#include "../DSP/Effects/PedalsMod.h"
#include "../DSP/Master/FreezeOverlay.h"
#include "../Practice/BackingTrack.h"
#include "../Practice/TimePitchShifter.h"
#include "../ToneMatch/ToneMatch.h"
#include "../Export/MidiImportTargets.h"
#include "../Capture/PerformanceCapture.h"
#include "../Notation/NotationExport.h"
#include "../Rhythm/ChordDetector.h"
#include "../Practice/PracticeRoutineSetup.h"
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

LUTHIER_TEST (PracticeGaps, sessionSaveMidiFollowsTheDefaultProfile)
{
    // MX-25 (midi-export 8): the session's MIDI is written like any export.
    for (const auto profile : { MidiProfile::generic, MidiProfile::luthier })
    {
        SessionRecorderSetup setup;
        setup.ringMinutes = 1.0;
        setup.recordAudio = false;
        setup.recordMidi = true;

        SessionRecorder recorder;
        CHECK (setup.applyTo (recorder, 48000.0));
        recorder.setEnabled (true);

        juce::AudioBuffer<float> block (2, 512);
        block.clear();

        for (int b = 0; b < 100; ++b)
        {
            juce::MidiBuffer midi;

            if (b == 3)  midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 17);
            if (b == 60) midi.addEvent (juce::MidiMessage::noteOff (1, 60), 5);

            recorder.captureMidi (midi, 512);
            recorder.processBlock (block, 512);
        }

        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-session-profile");
        dir.deleteRecursively();
        dir.createDirectory();

        MidiExportOptions options;
        options.profile = profile;
        CHECK (recorder.saveLastTake (dir, 0.0, &options));

        const auto files = dir.findChildFiles (juce::File::findFiles, false, "*.mid");
        CHECK (files.size() == 1);

        if (files.size() == 1)
        {
            MidiPerformance read (48000.0);
            const auto result = MidiProfiles::importFromFile (files[0], read, 48000.0);
            CHECK (result.ok);
            CHECK_MSG (result.detectedProfile == profile, "the session MIDI was not written in the chosen profile");
        }

        dir.deleteRecursively();
    }
}

LUTHIER_TEST (ToneMatch, anEqReferenceCanBeAFile)
{
    // TM-23 (tone-match 3.1): a dropped file is the reference; the wizard
    // goes straight on to recording Luthier's own pass.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    MatchWizard wizard (processor, MatchWizard::Kind::eqMatch);
    wizard.setSize (500, MatchWizard::preferredHeight);

    juce::WavAudioFormat wav;
    const auto file = writeSine (wav, ".wav", 440.0, 1.0, 44100.0);

    CHECK (wizard.isInterestedInFileDrag ({ file.getFullPathName() }));
    wizard.filesDropped ({ file.getFullPathName() }, 0, 0);

    CHECK (wizard.getStep() == 2);
    CHECK_MSG (std::abs (wizard.getReferenceLength() - 48000) < 100,
               "the reference was not resampled to the plugin's rate: " + juce::String (wizard.getReferenceLength()));
    CHECK (processor.getCapture().isRecording());
    CHECK (processor.getCapture().getSource() == Capture::Source::mainOut);

    MatchWizard cab (processor, MatchWizard::Kind::cabMatch);
    CHECK (! cab.isInterestedInFileDrag ({ file.getFullPathName() }));

    file.deleteFile();
}

LUTHIER_TEST (LiveTapTempo, midiClockDrivesTheTempoWhenTheHostIsStopped)
{
    // HI-32 (host-integration 7): 24 clocks a beat at 100 bpm, no host transport.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const double samplesPerClock = 48000.0 * 60.0 / (100.0 * 24.0);   // 1200
    double nextClock = 0.0;
    juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), 512);

    for (int b = 0; b < 300; ++b)
    {
        juce::MidiBuffer midi;
        const double blockStart = b * 512.0;

        while (nextClock < blockStart + 512.0)
        {
            midi.addEvent (juce::MidiMessage::midiClock(), (int) (nextClock - blockStart));
            nextClock += samplesPerClock;
        }

        buffer.clear();
        processor.processBlock (buffer, midi);
    }

    CHECK_NEAR (processor.getEffectiveTempo(), 100.0, 0.5);

    // Stop: the clock no longer sets the tempo.
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::midiStop(), 0);
        processor.processBlock (buffer, midi);
    }

    CHECK (std::abs (processor.getEffectiveTempo() - 100.0) > 0.5);

    // And the tracker on its own: a gap restarts the count.
    MidiClockTempo clock;
    juce::MidiBuffer midi;

    for (int i = 0; i < 30; ++i)
        midi.addEvent (juce::MidiMessage::midiClock(), i * 1000);   // 120 bpm

    clock.process (midi, 0.0, 48000.0);
    CHECK_NEAR (clock.getBpm(), 120.0, 0.01);

    juce::MidiBuffer later;
    later.addEvent (juce::MidiMessage::midiClock(), 0);
    clock.process (later, 10.0, 48000.0);
    CHECK (clock.getBpm() == 0.0);

    processor.releaseResources();
}

LUTHIER_TEST (PracticeTrainers, intervalTrainerScoresChoices)
{
    // PT-35 (practice-tools 4).
    ScaleTrainer trainer;
    trainer.setKey (0);
    trainer.setScale (ScaleType::ionian);
    trainer.setMode (ScaleTrainer::Mode::intervalTrainer);

    juce::Random random (11);
    int right = 0;

    for (int q = 0; q < 20; ++q)
    {
        trainer.nextQuestion (random);
        const int semis = trainer.getIntervalSemitones();
        CHECK (semis >= 0 && semis < 12);

        int notes[4] {};
        CHECK (trainer.getQuestionNotes (notes, 4) == 2);
        CHECK ((notes[1] - notes[0] + 12) % 12 == semis);

        const bool answerRight = q % 2 == 0;
        CHECK (trainer.answerInterval (answerRight ? semis : semis + 1) == answerRight);
        CHECK (! trainer.answerInterval (semis));   // one answer a question

        right += answerRight ? 1 : 0;
    }

    CHECK (trainer.getScore() == right);
    CHECK (trainer.getAsked() == 20);

    // Through the tab: Ask plays the two notes, a button answers.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    ScaleTab tab (processor);
    tab.setSize (700, 300);

    auto& shared = processor.getScaleTrainer();
    shared.setMode (ScaleTrainer::Mode::intervalTrainer);
    shared.resetScore();
    tab.ask();
    tab.chooseInterval (shared.getIntervalSemitones());
    CHECK (shared.getScore() == 1);

    runBlocks (processor, 2);
    int ringing = 0;

    for (int s = 0; s < processor.getEngine().getNumStrings(); ++s)
        ringing += processor.getEngine().getStringMidiNote (s) >= 0 ? 1 : 0;

    CHECK_MSG (ringing == 2, "the interval was not played: " + juce::String (ringing) + " strings ringing");
    processor.releaseResources();
}

LUTHIER_TEST (PracticeTrainers, chordToneTrainerNeedsThirdAndSeventhInTime)
{
    // PT-35 (practice-tools 4).
    ScaleTrainer trainer;
    trainer.setKey (7);   // G mixolydian: B and F
    trainer.setScale (ScaleType::mixolydian);
    trainer.setMode (ScaleTrainer::Mode::chordToneTrainer);
    trainer.setTimeLimitSeconds (5.0);

    juce::Random random (5);
    trainer.nextQuestion (random);

    int notes[8] {};
    CHECK (trainer.getQuestionNotes (notes, 8) == 4);

    CHECK (! trainer.answer (59, 1.0));      // B: the 3rd, half way
    CHECK (trainer.isQuestionOpen());
    CHECK (! trainer.answer (62, 1.5));      // D: not a wanted tone
    CHECK (trainer.answer (65, 2.0));        // F: the 7th, done
    CHECK (trainer.getScore() == 1);

    // Too late.
    trainer.nextQuestion (random);
    CHECK (! trainer.answer (59, 1.0));
    CHECK (! trainer.answer (65, 6.0));
    CHECK (! trainer.isQuestionOpen());
    CHECK (trainer.getScore() == 1);

    // A pentatonic has no 7th: the 3rd alone answers.
    trainer.setScale (ScaleType::majorPentatonic);
    trainer.nextQuestion (random);
    CHECK (trainer.answer (59, 0.5));
}

LUTHIER_TEST (Sustain, theLevelFloorIsSilent)
{
    // AR-10 (ambiguity-resolutions: freeze level -inf to 0 dB): the bottom of
    // freeze_level's range is silence.
    FreezeOverlay freeze;
    freeze.setLevelDb (-60.0);
    CHECK (freeze.getLevelGain() == 0.0);

    freeze.setLevelDb (-59.0);
    CHECK (freeze.getLevelGain() > 0.0);

    freeze.setLevelDb (0.0);
    CHECK_NEAR (freeze.getLevelGain(), 1.0, 1.0e-12);
}

LUTHIER_TEST (MidiExport, genericKeepsTheSlapGhostDistinction)
{
    // BT-25 (bass-techniques 10): without BASS_TECH, velocity carries it.
    MidiPerformance performance (48000.0);
    performance.setTempo (120.0);

    for (int i = 0; i < 2; ++i)
    {
        performance.addMessage (i * 24000, juce::MidiMessage::noteOn (1, 40, (juce::uint8) 80), 1);
        performance.addMessage (i * 24000 + 12000, juce::MidiMessage::noteOff (1, 40), 1);
    }

    performance.addEvent (LuthierEvent::make (LuthierEventClass::bassTech, 0, 1).set ("tech", "slap"));
    performance.addEvent (LuthierEvent::make (LuthierEventClass::bassTech, 24000, 1).set ("tech", "ghost"));

    auto file = juce::File::createTempFile (".mid");
    MidiExportOptions options;
    options.profile = MidiProfile::generic;
    juce::String error;
    CHECK_MSG (MidiProfiles::exportToFile (performance, options, file, &error), error);

    MidiPerformance read (48000.0);
    CHECK (MidiProfiles::importFromFile (file, read, 48000.0).ok);

    std::vector<int> velocities;

    for (const auto& m : read.getMessages())
        if (m.message.isNoteOn())
            velocities.push_back (m.message.getVelocity());

    CHECK (velocities.size() == 2);

    if (velocities.size() == 2)
    {
        CHECK_MSG (velocities[0] >= 110, "the slap came out at " + juce::String (velocities[0]));
        CHECK_MSG (velocities[1] <= 30, "the ghost came out at " + juce::String (velocities[1]));
    }

    // The Luthier profile leaves the velocities alone: BASS_TECH says it.
    options.profile = MidiProfile::luthier;
    CHECK (MidiProfiles::exportToFile (performance, options, file, &error));
    MidiPerformance luthier (48000.0);
    CHECK (MidiProfiles::importFromFile (file, luthier, 48000.0).ok);

    for (const auto& m : luthier.getMessages())
        if (m.message.isNoteOn())
            CHECK (m.message.getVelocity() == 80);

    file.deleteFile();
}

LUTHIER_TEST (Buzz, theCentreRisesWithTheContactFret)
{
    // FB-8 (fret-buzz.md 4): the buzz's centre sits in 3-6 kHz and rises
    // with the fret the string hits.
    auto centreFor = [] (double fretted)
    {
        NoiseEngine pool;
        pool.prepare (48000.0);

        FretBuzz buzz;
        buzz.setGeometry (needsATech());

        const std::array<double, 6> levels { 0.0, 0.0, 0.0, 0.0, 0.0, 1.0 };
        const std::array<double, 6> frets { 0.0, 0.0, 0.0, 0.0, 0.0, fretted };
        const std::array<double, 6> hz { 330.0, 247.0, 196.0, 147.0, 110.0, 82.4 };
        buzz.process (pool, levels.data(), frets.data(), hz.data(), 6, 0.2);

        return std::make_pair (activeLevel (pool, NoiseClass::fretBuzz) > 0.0 ? buzz.getBuzzingFret (5) : -1,
                               [&pool]
                               {
                                   for (int i = 0; i < pool.getPoolLimit (NoiseClass::fretBuzz); ++i)
                                       if (auto* g = pool.getGenerator (NoiseClass::fretBuzz, i); g != nullptr && g->isActive())
                                           return g->getEvent().startHz;
                                   return 0.0;
                               }());
    };

    const auto low = centreFor (0.0);
    const auto high = centreFor (9.0);

    CHECK (low.first > 0 && high.first > low.first);
    CHECK (low.second >= 3000.0 && low.second <= 6000.0);
    CHECK (high.second >= 3000.0 && high.second <= 6000.0);
    CHECK_MSG (high.second > low.second, "the buzz centre did not rise with the contact fret");
}

//==============================================================================
//  host-integration 4: the state envelope
//==============================================================================
namespace
{
    juce::var stateOf (LuthierAudioProcessor& processor)
    {
        juce::MemoryBlock block;
        processor.getStateInformation (block);
        return juce::JSON::parse (block.toString());
    }

    void setState (LuthierAudioProcessor& processor, const juce::var& state)
    {
        const auto json = juce::JSON::toString (state, true);
        processor.setStateInformation (json.toRawUTF8(), (int) json.getNumBytesAsUTF8());
    }
}

LUTHIER_TEST (HostState, theStateCarriesAFormatVersion)
{
    // HI-20 (host-integration 4).
    LuthierAudioProcessor processor;
    const auto state = stateOf (processor);
    CHECK (state.getDynamicObject() != nullptr);
    CHECK ((int) state["formatVersion"] == HostStateEnvelope::kFormatVersion);
}

LUTHIER_TEST (HostState, unknownSectionsSurviveWriteBack)
{
    // HI-24 (host-integration 4.1): a later build's blob loads, says so, and
    // what this build does not know goes back out unchanged.
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-state-envelope");
    folder.deleteRecursively();

    LuthierAudioProcessor processor;
    processor.getStateEnvelope().setBackupFolder (folder);
    processor.takeStateNotices();

    auto state = stateOf (processor);
    state.getDynamicObject()->setProperty ("formatVersion", HostStateEnvelope::kFormatVersion + 1);

    auto* future = new juce::DynamicObject();
    future->setProperty ("hologram", 42);
    state.getDynamicObject()->setProperty ("futureSection", juce::var (future));

    setState (processor, state);

    const auto notices = processor.takeStateNotices();
    CHECK_MSG (notices.joinIntoString (" ").contains ("newer version"), "no notice for a newer session");

    const auto written = stateOf (processor);
    CHECK ((int) written["futureSection"]["hologram"] == 42);
    CHECK ((int) written["formatVersion"] == HostStateEnvelope::kFormatVersion);

    // A newer blob is not backed up: nothing migrates it.
    CHECK (folder.findChildFiles (juce::File::findFiles, false, "*.json").isEmpty());
    folder.deleteRecursively();
}

LUTHIER_TEST (HostState, anOldBlobIsBackedUpBeforeMigration)
{
    // HI-25 (host-integration 4.2).
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-state-backup");
    folder.deleteRecursively();

    LuthierAudioProcessor processor;
    processor.getStateEnvelope().setBackupFolder (folder);

    auto state = stateOf (processor);
    state.getDynamicObject()->removeProperty ("formatVersion");   // as saved before HI-20
    const auto json = juce::JSON::toString (state, true);
    processor.setStateInformation (json.toRawUTF8(), (int) json.getNumBytesAsUTF8());

    const auto backups = folder.findChildFiles (juce::File::findFiles, false, "state-backup-*.json");
    CHECK (backups.size() == 1);

    if (backups.size() == 1)
        CHECK (backups[0].loadFileAsString() == json);

    // A current blob is not.
    const auto current = juce::JSON::toString (stateOf (processor), true);
    processor.setStateInformation (current.toRawUTF8(), (int) current.getNumBytesAsUTF8());
    CHECK (folder.findChildFiles (juce::File::findFiles, false, "state-backup-*.json").size() == 1);

    // Only the last few are kept.
    for (int i = 0; i < HostStateEnvelope::kMaxBackups + 3; ++i)
        processor.setStateInformation (json.toRawUTF8(), (int) json.getNumBytesAsUTF8());

    CHECK (folder.findChildFiles (juce::File::findFiles, false, "state-backup-*.json").size() == HostStateEnvelope::kMaxBackups);
    folder.deleteRecursively();
}

//==============================================================================
//  pick-noise
//==============================================================================
LUTHIER_TEST (PickNoise, decayWearAndPositionShapeTheClick)
{
    // PN-15 (pick-noise.md 3).
    PickSettings base;
    base.wear = 0.0;

    const auto plain = PlayingNoise::makeClick (base, 0, 0.8);
    CHECK (plain.decayMs >= 3.0 && plain.decayMs <= 15.0);
    CHECK (plain.subResonanceLevel == 0.0);

    auto sharp = base;
    sharp.tipRadiusMm = 0.3;
    CHECK (PlayingNoise::makeClick (sharp, 0, 0.8).decayMs < plain.decayMs);

    auto damped = base;
    damped.material = Excitation::Material::PickNylon;
    auto stiff = base;
    stiff.material = Excitation::Material::PickMetal;
    CHECK (PlayingNoise::getPickMaterial (damped.material).damping > PlayingNoise::getPickMaterial (stiff.material).damping);
    CHECK (PlayingNoise::makeClick (damped, 0, 0.8).decayMs < PlayingNoise::makeClick (stiff, 0, 0.8).decayMs);

    auto worn = base;
    worn.wear = 0.8;
    const auto wornClick = PlayingNoise::makeClick (worn, 0, 0.8);
    CHECK (wornClick.subResonanceLevel > 0.0);
    CHECK (wornClick.decayMs > plain.decayMs);

    auto nearBridge = base, nearNeck = base;
    nearBridge.pluckPosition = 0.05;
    nearNeck.pluckPosition = 0.4;
    CHECK (PlayingNoise::makeClick (nearBridge, 0, 0.8).brightness > PlayingNoise::makeClick (nearNeck, 0, 0.8).brightness);
}

//==============================================================================
//  string-scraping
//==============================================================================
LUTHIER_TEST (Scrape, aBendStretchesTheWindingSpacing)
{
    // SC-22 (string-scraping.md 5): a bent string is under more tension and
    // its winding is stretched slightly apart - fewer catches over the same path.
    auto catches = [] (double bendCents)
    {
        ScrapeEngine scrape;
        scrape.prepare (48000.0, 256);
        scrape.setNumStrings (6);
        scrape.setScaleLengthMm (648.0);

        StringNoiseInfo info;
        info.wound = true;
        info.windingPitchPerMm = 20.0;
        info.windingDepth = 0.9;

        for (int s = 0; s < 6; ++s)
            scrape.setString (s, info, 82.41, 0.0, s == 5 ? bendCents : 0.0);

        ScrapeGesture g;
        g.stringIndex = 5;
        g.startPositionMm = 100.0;
        g.endPositionMm = 600.0;
        g.durationMs = 500.0;
        scrape.trigger (g);

        for (int b = 0; b < 120; ++b)
            scrape.processBlock (256);

        return (double) scrape.getCatchCount (5);
    };

    const double flat = catches (0.0);
    const double bent = catches (200.0);
    const double r = std::pow (2.0, 200.0 / 1200.0);
    const double factor = 1.0 / (1.0 + 0.003 * (r * r - 1.0));

    CHECK (flat > 9000.0);
    CHECK_MSG (bent < flat, "a bend did not spread the winding");
    CHECK_NEAR (bent / flat, factor, 0.0005);
}

//==============================================================================
//  strum-dynamics
//==============================================================================
LUTHIER_TEST (StrumDynamics, aPalmMuteKeepsPitchAndAChuckDoesNot)
{
    // SD-18 (strum-dynamics 6.1): a palm-muted note still has its pitch; a
    // chuck is the hand flat on the strings and has none to speak of. Measured
    // as the energy that is left at the note's fundamental after the attack.
    auto render = [] (bool chuck)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.setGuitarType (GuitarType::Dreadnought);

        NoteOnEvent e;
        e.stringIndex = 4;
        e.fretPosition = 2.0;
        e.velocity = 0.8;
        e.pitchHz = engine.getTuningEngine().computeFrequency (4, 2.0);

        if (chuck)
            e.chuck = 1.0;
        else
            e.technique = Technique::PalmMute;

        engine.getTechniqueEngine().setPalmMuteAmount (0.7);
        engine.triggerNoteNow (e);

        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;

        for (int b = 0; b < 40; ++b)
        {
            buffer.clear();
            engine.processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 256);
        }

        // Goertzel at the fundamental over 60-200 ms, relative to the whole.
        const double hz = e.pitchHz;
        const int from = 2880, to = 9600;
        double s1 = 0.0, s2 = 0.0, energy = 1.0e-20;
        const double coeff = 2.0 * std::cos (juce::MathConstants<double>::twoPi * hz / 48000.0);

        for (int i = from; i < to; ++i)
        {
            const double s0 = out[(size_t) i] + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
            energy += (double) out[(size_t) i] * out[(size_t) i];
        }

        const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
        return power / (energy * (to - from));
    };

    const double muted = render (false);
    const double chucked = render (true);

    CHECK_MSG (muted > 0.05, "a palm mute lost its pitch: " + juce::String (muted));
    CHECK_MSG (chucked < muted * 0.5, "a chuck kept its pitch: " + juce::String (chucked) + " vs " + juce::String (muted));
}

//==============================================================================
LUTHIER_TEST (Buzz, squeakAndBuzzCoexist)
{
    // SQ-25 / FB-25 (string-squeak.md 10, fret-buzz.md 8): a shift across a
    // buzzing region produces both, with no ducking.
    LuthierEngine engine;
    engine.prepare (48000.0, 256);
    engine.setGuitarType (GuitarType::Dreadnought);
    engine.setSetupGeometry (needsATech());

    SqueakSettings always;
    always.probability = 1.0;
    always.moisture = 0.0;
    engine.setSqueak (always);

    const int low = engine.getNumStrings() - 1;
    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;

    NoteOnEvent pluck;
    pluck.stringIndex = low;
    pluck.fretPosition = 1.0;
    pluck.velocity = 1.0;
    pluck.pitchHz = engine.getTuningEngine().computeFrequency (low, 1.0);
    engine.triggerNoteNow (pluck);

    for (int b = 0; b < 4; ++b)
        engine.processBlock (buffer, midi);

    NoteOnEvent slide = pluck;
    slide.technique = Technique::Slide;
    slide.slideFromFret = 1.0;
    slide.fretPosition = 5.0;
    slide.slideSeconds = 0.12;
    slide.pitchHz = engine.getTuningEngine().computeFrequency (low, 5.0);
    engine.triggerNoteNow (slide);

    for (int b = 0; b < 8; ++b)
        engine.processBlock (buffer, midi);

    const auto& pool = engine.getPlayingNoise().getPool();
    CHECK_MSG (pool.getTriggerCount (NoiseClass::squeak) >= 1, "the shift did not squeak");
    CHECK_MSG (pool.getTriggerCount (NoiseClass::fretBuzz) >= 1, "the hard note on a bad setup did not buzz");
}

LUTHIER_TEST (Slide, aLowSetupBuzzesUnderTheBar)
{
    // SG-17 (slide-guitar.md 5 / fret-buzz.md 8): a bar note on "Needs a tech"
    // still rattles on the frets underneath.
    LuthierEngine engine;
    engine.prepare (48000.0, 256);
    engine.setGuitarType (GuitarType::Dreadnought);
    engine.setSetupGeometry (needsATech());

    SlideSettings settings;
    settings.enabled = true;
    settings.mode = SlideMode::lapSteel;
    engine.setSlideSettings (settings);
    engine.getMidiInterpreter().setPlayingMode (PlayingMode::Mono);

    juce::AudioBuffer<float> buffer (2, 256);

    for (int b = 0; b < 12; ++b)
    {
        juce::MidiBuffer midi;

        if (b == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 127), 0);

        buffer.clear();
        engine.processBlock (buffer, midi);
    }

    CHECK_MSG (engine.getPlayingNoise().getPool().getTriggerCount (NoiseClass::fretBuzz) >= 1,
               "a hard bar note on a low setup did not buzz");
}

//==============================================================================
namespace
{
    /** The slot's response to a unit impulse, 8192 samples, at 48 kHz. */
    std::vector<float> slotImpulseResponse (IrSlot& slot)
    {
        std::vector<float> out;
        juce::AudioBuffer<float> block (2, 512);

        for (int b = 0; b < 16; ++b)
        {
            block.clear();

            if (b == 0)
                block.setSample (0, 0, 1.0f), block.setSample (1, 0, 1.0f);

            slot.process (block.getArrayOfWritePointers(), 2, 512);
            out.insert (out.end(), block.getReadPointer (0), block.getReadPointer (0) + 512);
        }

        return out;
    }

    /** Magnitude in dB per FFT bin (order 13). */
    std::vector<double> magnitudeDb (const std::vector<float>& x)
    {
        juce::dsp::FFT fft (13);
        std::vector<float> data (16384, 0.0f);
        std::copy (x.begin(), x.begin() + juce::jmin ((size_t) 8192, x.size()), data.begin());
        fft.performFrequencyOnlyForwardTransform (data.data());

        std::vector<double> db (4097);

        for (size_t i = 0; i < db.size(); ++i)
            db[i] = 20.0 * std::log10 (juce::jmax (1.0e-12, (double) data[i]));

        return db;
    }
}

LUTHIER_TEST (ToneMatch, irsResampleWithinHalfADb)
{
    // TM-2 / TM-41 (tone-match 0.2): an impulse IR at any common rate, loaded
    // into a 48 kHz slot, stays flat within 0.5 dB up to 0.9 x the lower Nyquist.
    for (const double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        juce::WavAudioFormat wav;
        const auto file = writeIr (wav, ".wav", 0.05, 1, rate, true);

        IrSlot slot;
        slot.prepare (48000.0, 512);
        CHECK (slot.load (file));
        slot.setEngaged (true);
        slot.setMix (1.0);
        juce::Thread::sleep (200);   // the convolution swaps its response in off the audio thread

        {
            juce::AudioBuffer<float> warm (2, 512);
            warm.clear();
            slot.process (warm.getArrayOfWritePointers(), 2, 512);
        }

        const auto db = magnitudeDb (slotImpulseResponse (slot));
        const double binHz = 48000.0 / 8192.0;
        const double top = 0.9 * juce::jmin (24000.0, rate * 0.5);
        const double reference = db[(size_t) std::round (1000.0 / binHz)];
        double worst = 0.0;

        for (size_t i = (size_t) std::round (50.0 / binHz); i < (size_t) (top / binHz); ++i)
            worst = juce::jmax (worst, std::abs (db[i] - reference));

        CHECK_MSG (worst < 0.5, juce::String (rate) + " Hz IR deviates " + juce::String (worst, 2) + " dB");

        slot.unload();
        file.deleteFile();
    }
}

LUTHIER_TEST (ToneMatch, sampleRateChangeReResamplesTheIr)
{
    // TM-45 (tone-match 0.2): the IR keeps its length in time at a new rate.
    juce::WavAudioFormat wav;
    const auto file = writeIr (wav, ".wav", 0.25);

    IrSlot slot;
    slot.prepare (48000.0, 512);
    CHECK (slot.load (file));
    const double ms = slot.getLengthMs();

    slot.prepare (96000.0, 512);
    CHECK_NEAR (slot.getLengthMs(), ms, 0.5);
    CHECK (slot.isLoaded());

    slot.setEngaged (true);
    juce::AudioBuffer<float> block (2, 512);

    for (int b = 0; b < 8; ++b)
    {
        for (int i = 0; i < 512; ++i)
            block.setSample (0, i, (float) std::sin (0.02 * (b * 512 + i))), block.setSample (1, i, block.getSample (0, i));

        slot.process (block.getArrayOfWritePointers(), 2, 512);

        for (int i = 0; i < 512; ++i)
            CHECK (std::isfinite (block.getSample (0, i)));
    }

    slot.unload();
    file.deleteFile();
}

LUTHIER_TEST (ToneMatch, theLibraryTreeIsCreated)
{
    // TM-34 (tone-match 5).
    IrLibraryPaths::ensureExists();

    for (const auto& folder : { IrLibraryPaths::getRoot(), IrLibraryPaths::getBodies(), IrLibraryPaths::getBodiesAcoustic(),
                                IrLibraryPaths::getBodiesElectric(), IrLibraryPaths::getCabinets(),
                                IrLibraryPaths::getCabinetsUser(), IrLibraryPaths::getCabinetsMatch(),
                                IrLibraryPaths::getRooms(), IrLibraryPaths::getSpecial() })
        CHECK_MSG (folder.isDirectory(), folder.getFullPathName() + " was not created");
}

LUTHIER_TEST (ToneMatch, aSavedIrHasItsSidecar)
{
    // TM-21 (tone-match 2, 5): saveIr writes a readable WAV and a .json beside it.
    ImpulseResponse ir;
    ir.sampleRate = 48000.0;
    ir.samples.assign (2400, 0.0f);
    ir.samples[0] = 0.8f;
    ir.samples[100] = -0.3f;

    IrMetadata metadata;
    metadata.name = "Sweep test";
    metadata.type = "cabinet";
    metadata.tags = { "cab-match" };

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-saveir");
    dir.deleteRecursively();
    dir.createDirectory();
    const auto file = dir.getChildFile ("Sweep test.wav");

    CHECK (CabMatch::saveIr (ir, file, metadata));
    CHECK (file.existsAsFile());
    CHECK (file.withFileExtension ("json").existsAsFile());

    const auto read = IrMetadata::forFile (file);
    CHECK (read.name == "Sweep test");
    CHECK (read.tags.contains ("cab-match"));

    IrSlot slot;
    slot.prepare (48000.0, 512);
    CHECK (slot.load (file));
    CHECK_NEAR (slot.getLengthMs(), 50.0, 1.0);

    slot.unload();
    dir.deleteRecursively();
}

//==============================================================================
//  practice-tools: the tests the audit found missing
//==============================================================================
namespace
{
    juce::AudioBuffer<float> toneBuffer (double hz, int samples, float level = 0.3f)
    {
        juce::AudioBuffer<float> audio (2, samples);

        for (int i = 0; i < samples; ++i)
        {
            const float v = level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / 48000.0);
            audio.setSample (0, i, v);
            audio.setSample (1, i, v);
        }

        return audio;
    }

    juce::AudioBuffer<float> readWav (const juce::File& file)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

        if (reader == nullptr)
            return {};

        juce::AudioBuffer<float> audio ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&audio, 0, audio.getNumSamples(), 0, true, true);
        return audio;
    }
}

LUTHIER_TEST (PracticeLooper, exportWritesAMixdownAndOneStemPerLayer)
{
    // PT-21 (practice-tools 2).
    Looper looper;
    looper.prepare (48000.0, 10.0);
    CHECK (looper.loadLayerAudio (0, toneBuffer (220.0, 24000)));
    CHECK (looper.loadLayerAudio (1, toneBuffer (330.0, 24000, 0.2f)));
    CHECK (looper.loadLayerAudio (2, toneBuffer (440.0, 24000, 0.1f)));

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-loop-export");
    dir.deleteRecursively();
    dir.createDirectory();

    CHECK (looper.exportMixdown (dir.getChildFile ("mix.wav")));
    CHECK (looper.exportStems (dir.getChildFile ("stems")));

    const auto stems = dir.getChildFile ("stems").findChildFiles (juce::File::findFiles, false, "*.wav");
    CHECK (stems.size() == 3);

    const auto mix = readWav (dir.getChildFile ("mix.wav"));
    CHECK (mix.getNumSamples() == 24000);

    juce::AudioBuffer<float> sum (mix.getNumChannels(), mix.getNumSamples());
    sum.clear();

    for (const auto& stem : stems)
    {
        const auto audio = readWav (stem);

        for (int c = 0; c < sum.getNumChannels(); ++c)
            sum.addFrom (c, 0, audio, juce::jmin (c, audio.getNumChannels() - 1), 0, juce::jmin (sum.getNumSamples(), audio.getNumSamples()));
    }

    float worst = 0.0f;

    for (int c = 0; c < sum.getNumChannels(); ++c)
        for (int i = 0; i < sum.getNumSamples(); ++i)
            worst = juce::jmax (worst, std::abs (sum.getSample (c, i) - mix.getSample (c, i)));

    CHECK_MSG (worst < 1.0e-4f, "the mixdown is not the sum of its stems: " + juce::String (worst));
    dir.deleteRecursively();
}

LUTHIER_TEST (PracticeLooper, aSavedLoopReloadsIdentically)
{
    // PT-22 (practice-tools 2).
    Looper looper;
    looper.prepare (48000.0, 10.0);
    CHECK (looper.loadLayerAudio (0, toneBuffer (220.0, 24000)));
    CHECK (looper.loadLayerAudio (1, toneBuffer (330.0, 24000, 0.2f)));
    looper.getLayer (1).setPan (-0.4);
    looper.getLayer (1).setLevelDb (-3.0);
    looper.getLayer (1).setLowCutHz (150.0);
    looper.getLayer (0).getMidi().addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 90), 10.0);

    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-loop-save").getChildFile ("take.luthierloop");
    file.getParentDirectory().deleteRecursively();
    file.getParentDirectory().createDirectory();
    CHECK (looper.save (file));

    Looper restored;
    restored.prepare (48000.0, 10.0);
    CHECK (restored.load (file));

    CHECK (restored.getLoopLengthSamples() == looper.getLoopLengthSamples());
    CHECK (restored.getNumRecordedLayers() == 2);
    CHECK_NEAR (restored.getLayer (1).getPan(), -0.4, 1.0e-6);
    CHECK_NEAR (restored.getLayer (1).getLevelDb(), -3.0, 1.0e-6);
    CHECK_NEAR (restored.getLayer (1).getLowCutHz(), 150.0, 1.0e-6);
    CHECK (restored.getLayer (0).getMidi().getNumEvents() == looper.getLayer (0).getMidi().getNumEvents());

    float worst = 0.0f;

    for (int i = 0; i < 24000; ++i)
        worst = juce::jmax (worst, std::abs (restored.getLayer (0).readLeft()[i] - looper.getLayer (0).readLeft()[i]));

    CHECK_MSG (worst < 1.0e-4f, "the reloaded audio differs by " + juce::String (worst));
    file.getParentDirectory().deleteRecursively();
}

LUTHIER_TEST (PracticeSession, oldTempFilesAreCleanedAfterADay)
{
    // PT-48 (practice-tools 8).
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-session-tmp");
    dir.deleteRecursively();
    dir.createDirectory();

    const auto old = dir.getChildFile ("session-old.wav");
    const auto fresh = dir.getChildFile ("session-new.wav");
    old.replaceWithText ("x");
    fresh.replaceWithText ("y");
    old.setLastModificationTime (juce::Time::getCurrentTime() - juce::RelativeTime::hours (30));

    SessionRecorder::cleanUpOldTempFiles (dir, 24.0);

    CHECK (! old.existsAsFile());
    CHECK (fresh.existsAsFile());
    dir.deleteRecursively();
}

LUTHIER_TEST (PracticeTrack, levelAndMonoApply)
{
    // PT-25 (practice-tools 3): level in dB, and mono sums the sides.
    auto file = juce::File::createTempFile (".wav");
    {
        juce::AudioBuffer<float> audio (2, 48000);

        for (int i = 0; i < 48000; ++i)
        {
            audio.setSample (0, i, 0.5f * (float) std::sin (0.05 * i));
            audio.setSample (1, i, 0.0f);   // hard left
        }

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), 48000.0, 2, 24, {}, 0));
        stream.release();
        writer->writeFromAudioSampleBuffer (audio, 0, 48000);
    }

    auto rmsAt = [&file] (double levelDb, bool mono)
    {
        BackingTrackPlayer player;
        player.prepare (48000.0, 512);
        player.load (file);
        player.setLevelDb (levelDb);
        player.setMonoSum (mono);
        juce::Thread::sleep (300);
        player.play();

        juce::AudioBuffer<float> block (2, 512);
        double l = 0.0, r = 0.0;

        for (int b = 0; b < 20; ++b)
        {
            player.processBlock (block, 512);

            if (b >= 4)
                l += block.getRMSLevel (0, 0, 512), r += block.getRMSLevel (1, 0, 512);
        }

        player.unload();
        return std::make_pair (l, r);
    };

    const auto loud = rmsAt (0.0, false);
    const auto quiet = rmsAt (-12.0, false);
    const auto mono = rmsAt (0.0, true);

    CHECK_NEAR (20.0 * std::log10 (quiet.first / loud.first), -12.0, 0.2);
    CHECK (loud.second < loud.first * 0.01);
    CHECK_NEAR (mono.first, mono.second, mono.first * 0.01);
    file.deleteFile();
}

LUTHIER_TEST (PracticeTrack, loopPointsSnapToZeroCrossingsAndWrap)
{
    // PT-27 (practice-tools 3).
    juce::WavAudioFormat wav;
    const auto file = writeSine (wav, ".wav", 100.0, 3.0);   // a rising crossing every 10 ms

    BackingTrackPlayer player;
    player.prepare (48000.0, 512);
    CHECK (player.load (file));

    player.setLoopSeconds (0.5033, 1.2071);
    const double start = player.getLoopStartSeconds() * 48000.0;
    const double end = player.getLoopEndSeconds() * 48000.0;

    // Each end on a rising crossing: a multiple of 480 samples, within a sample.
    CHECK_MSG (std::abs (start - 480.0 * std::round (start / 480.0)) <= 1.0, "start " + juce::String (start));
    CHECK_MSG (std::abs (end - 480.0 * std::round (end / 480.0)) <= 1.0, "end " + juce::String (end));

    // And the loop goes round: playing past the end comes back to the start.
    player.setLoopEnabled (true);
    player.setPositionSeconds (player.getLoopEndSeconds() - 0.05);
    juce::Thread::sleep (300);
    player.play();

    juce::AudioBuffer<float> block (2, 512);

    for (int b = 0; b < 20; ++b)
        player.processBlock (block, 512);

    CHECK (player.isPlaying());
    CHECK (player.getPositionSeconds() < player.getLoopEndSeconds());
    CHECK (player.getPositionSeconds() >= player.getLoopStartSeconds() - 0.01);

    player.unload();
    file.deleteFile();
}

LUTHIER_TEST (PracticeTrack, estimatesTheTempoOfAClickTrack)
{
    // PT-31 (practice-tools 3): a 100 bpm click.
    auto file = juce::File::createTempFile (".wav");
    {
        const int n = 48000 * 12;
        juce::AudioBuffer<float> audio (1, n);
        audio.clear();

        for (int beat = 0; beat * 28800 < n; ++beat)
            for (int i = 0; i < 480 && beat * 28800 + i < n; ++i)
                audio.setSample (0, beat * 28800 + i, 0.8f * (float) std::exp (-i / 80.0) * (float) std::sin (0.3 * i));

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), 48000.0, 1, 24, {}, 0));
        stream.release();
        writer->writeFromAudioSampleBuffer (audio, 0, n);
    }

    BackingTrackPlayer player;
    player.prepare (48000.0, 512);
    CHECK (player.load (file));
    CHECK_MSG (std::abs (player.getDetectedTempo() - 100.0) <= 1.0,
               "detected " + juce::String (player.getDetectedTempo()) + " bpm");
    player.unload();
    file.deleteFile();
}

LUTHIER_TEST (ToneMatch, captureSavesThirtyTwoBitFloatWav)
{
    // TM-32 (tone-match 4).
    Capture capture;
    capture.prepare (48000.0, 2.0);
    capture.start (0.5);

    const auto audio = toneBuffer (440.0, 24000, 0.4f);
    capture.processBlock (audio.getArrayOfReadPointers(), 2, 24000);
    CHECK (capture.isComplete() || capture.getRecordedSamples() == 24000);

    auto file = juce::File::createTempFile (".wav");
    CHECK (capture.save (file));

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    CHECK (reader != nullptr);

    if (reader != nullptr)
    {
        CHECK (reader->bitsPerSample == 32);
        CHECK (reader->usesFloatingPointData);

        juce::AudioBuffer<float> back (2, 24000);
        reader->read (&back, 0, 24000, 0, true, true);

        float worst = 0.0f;

        for (int i = 0; i < 24000; ++i)
            worst = juce::jmax (worst, std::abs (back.getSample (0, i) - audio.getSample (0, i)));

        CHECK (worst == 0.0f);
    }

    reader.reset();
    file.deleteFile();
}

LUTHIER_TEST (PracticeMetronome, subdivisionsClickAtTheirOwnLevel)
{
    // PT-8 (practice-tools 1): each subdivision lands on its grid, and the
    // subdivision level moves only the off-beat clicks.
    auto render = [] (ClickSubdivision sub, double subDb)
    {
        Metronome m;
        m.prepare (48000.0, 512);
        m.setTempo (120.0);   // a beat every 24000 samples
        m.setSubdivision (sub);
        m.setSubdivisionLevelDb (subDb);
        m.setEnabled (true);

        std::vector<float> out (48000 * 2, 0.0f);

        for (int at = 0; at < (int) out.size(); at += 512)
            m.processBlock (out.data() + at, juce::jmin (512, (int) out.size() - at));

        return out;
    };

    auto peakNear = [] (const std::vector<float>& x, int at)
    {
        float peak = 0.0f;

        for (int i = juce::jmax (0, at - 20); i < juce::jmin ((int) x.size(), at + 600); ++i)
            peak = juce::jmax (peak, std::abs (x[(size_t) i]));

        return peak;
    };

    const struct { ClickSubdivision sub; int spacing; } grids[] =
    {
        { ClickSubdivision::eighth, 12000 }, { ClickSubdivision::triplet, 8000 },
        { ClickSubdivision::sixteenth, 6000 }, { ClickSubdivision::dottedEighth, 18000 }
    };

    for (const auto& g : grids)
    {
        const auto x = render (g.sub, -6.0);

        // The first off-beat click is on its grid; midway before it, silence.
        CHECK_MSG (peakNear (x, g.spacing) > 0.01f, juce::String (getClickSubdivisionName (g.sub)) + ": no click on its grid");
        CHECK_MSG (peakNear (x, g.spacing / 2) < 1.0e-4f, juce::String (getClickSubdivisionName (g.sub)) + ": a click off its grid");
    }

    const auto loud = render (ClickSubdivision::eighth, -6.0);
    const auto soft = render (ClickSubdivision::eighth, -18.0);

    CHECK_NEAR (peakNear (loud, 24000), peakNear (soft, 24000), 1.0e-6);   // the beat is untouched
    CHECK_NEAR (20.0 * std::log10 (peakNear (soft, 12000) / peakNear (loud, 12000)), -12.0, 0.5);
}

LUTHIER_TEST (HostState, programsEnumeratePresetsAndLoadByIndex)
{
    // HI-45 (host-integration 12).
    LuthierAudioProcessor processor;
    auto& presets = processor.getPresetManager();

    CHECK (processor.getNumPrograms() == juce::jmax (1, presets.getNumPresets()));
    CHECK (presets.getNumPresets() > 1);

    for (int i = 0; i < juce::jmin (5, presets.getNumPresets()); ++i)
        CHECK (processor.getProgramName (i) == presets.getPreset (i)->name);

    processor.setCurrentProgram (1);
    CHECK (processor.getCurrentProgram() == 1);
    CHECK (presets.getCurrentPresetName() == processor.getProgramName (1));
}

LUTHIER_TEST (HostState, typicalStateIsSmall)
{
    // HI-22 / HI-38 (host-integration 4, 9.2): a session is well under 200 KB
    // for every factory preset (guitar by reference), and under 500 KB always.
    LuthierAudioProcessor processor;
    auto& presets = processor.getPresetManager();
    size_t largest = 0;
    juce::String largestName;

    for (int i = 0; i < presets.getNumPresets(); ++i)
    {
        if (! presets.loadPreset (i))
            continue;

        juce::MemoryBlock block;
        processor.getStateInformation (block);

        if (block.getSize() > largest)
        {
            largest = block.getSize();
            largestName = presets.getCurrentPresetName();
        }
    }

    CHECK_MSG (largest < 200 * 1024, "the largest state (" + largestName + ") is " + juce::String ((int) (largest / 1024)) + " KB");
}

LUTHIER_TEST (HostState, aSnapshotRecallAndPresetLoadNotifyTheHost)
{
    // HI-16 (host-integration 3.1): the host hears about every parameter a
    // snapshot or a preset changes.
    LuthierAudioProcessor processor;

    struct Listener final : public juce::AudioProcessorParameter::Listener
    {
        std::set<int> changed;
        void parameterValueChanged (int index, float) override { changed.insert (index); }
        void parameterGestureChanged (int, bool) override {}
    } listener;

    auto& params = processor.getParameters();

    for (auto* p : params)
        p->addListener (&listener);

    // A snapshot of a different state, recalled.
    auto* gain = processor.getState().getParameter (ParamIDs::macroCharacter);
    CHECK (gain != nullptr);

    if (gain != nullptr)
    {
        processor.getSnapshots().setCrossfadeMs (0.0);   // applied inside the call
        gain->setValueNotifyingHost (0.9f);
        CHECK (processor.captureSnapshot (0, "A", 1));
        gain->setValueNotifyingHost (0.1f);

        listener.changed.clear();
        CHECK (processor.recallSnapshot (0));
        CHECK_MSG (listener.changed.count (gain->getParameterIndex()) == 1, "a snapshot recall did not notify the host");
    }

    // A preset load.
    listener.changed.clear();
    CHECK (processor.getPresetManager().loadPreset (1));
    CHECK_MSG (! listener.changed.empty(), "a preset load did not notify the host of anything");

    for (auto* p : params)
        p->removeListener (&listener);
}

//==============================================================================
//  bass-techniques
//==============================================================================
LUTHIER_TEST (BassTechniques, anImportedBassTechGhostsItsNote)
{
    // BT-12 (bass-techniques 10): a BASS_TECH ghost on a note plays it ghosted
    // when the file is rendered (the looper import); a slap plays it slapped.
    auto renderWith = [] (const char* tech)
    {
        MidiPerformance performance (48000.0);
        performance.setTempo (120.0);
        performance.addMessage (0, juce::MidiMessage::noteOn (1, 40, (juce::uint8) 100), 1);
        performance.addMessage (24000, juce::MidiMessage::noteOff (1, 40), 1);

        if (tech != nullptr)
            performance.addEvent (LuthierEvent::make (LuthierEventClass::bassTech, 0, 1).set ("tech", tech));

        return MidiImportTargets::render (performance, GuitarType::JazzBass, 48000.0, 1.0);
    };

    const auto plain = renderWith (nullptr);
    const auto ghost = renderWith ("ghost");
    const auto slap = renderWith ("slap");
    const int n = juce::jmin (24000, plain.getNumSamples());

    const float plainRms = plain.getRMSLevel (0, 0, n);
    CHECK (plainRms > 0.0f);
    CHECK_MSG (ghost.getRMSLevel (0, 0, n) < plainRms * 0.6f,
               "a BASS_TECH ghost was not ghosted: " + juce::String (ghost.getRMSLevel (0, 0, n) / plainRms));

    double difference = 0.0, energy = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const double d = slap.getSample (0, i) - plain.getSample (0, i);
        difference += d * d;
        energy += (double) plain.getSample (0, i) * plain.getSample (0, i);
    }

    CHECK_MSG (difference > energy * 0.05, "a BASS_TECH slap sounded like a plain note: " + juce::String (difference / energy));
}

//==============================================================================
//  notation-export
//==============================================================================
LUTHIER_TEST (Notation, polyphonicMaterialGetsASecondVoice)
{
    // NE-10 (notation-export 2.1): a bass note held under a moving melody is
    // a second voice, in the score, in MusicXML, and back again.
    PerformanceScore score;
    score.beginCapture (120.0, 4, 4);

    score.noteStarted (5, 0, 40, 82.4, 0.8, 0.0);   // low E, the whole bar

    const int melody[] = { 64, 67, 69, 67 };

    for (int i = 0; i < 4; ++i)
    {
        score.noteStarted (0, melody[i] - 64, melody[i], 440.0, 0.8, (double) i);
        score.noteEnded (0, i + 1.0);
    }

    score.noteEnded (5, 4.0);
    score.endCapture (4.0);

    const auto& measure = score.getTrack (0).measures[0];
    CHECK_MSG (measure.voices.size() == 2, juce::String ((int) measure.voices.size()) + " voices, expected 2");

    NotationExporter exporter;
    const auto xml = exporter.renderMusicXml (score);
    CHECK (xml.contains ("<backup>"));
    CHECK (xml.contains ("<voice>2</voice>"));

    NotationImporter importer;
    PerformanceScore back;
    CHECK (importer.readMusicXml (xml, back));
    CHECK (back.getTotalNoteCount() == 5);

    const auto notes = back.getTrack (0).measures[0].collectNotes();
    bool haveBass = false;

    for (const auto* n : notes)
        haveBass = haveBass || n->midiNote == 40;

    CHECK (haveBass);
}

LUTHIER_TEST (Notation, midiExportCarriesBendRangeBendsAndLegato)
{
    // NE-16 (notation-export 2.4): per-string tracks open with the pitch-bend
    // range RPN, a bent note has pitch-wheel moves, and a hammer-on is
    // bracketed by the legato pedal (CC 68).
    PerformanceScore score;
    score.beginCapture (120.0, 4, 4);

    score.noteStarted (2, 5, 60, 261.6, 0.8, 0.0);
    ScoreTechnique bend;
    bend.type = ScoreTechnique::Type::bend;
    bend.value = 1.0;
    bend.curve = { { 0.0, 0.0 }, { 0.5, 1.0 }, { 1.0, 1.0 } };
    score.addTechnique (2, bend);
    score.noteEnded (2, 1.0);

    score.noteStarted (1, 5, 64, 329.6, 0.8, 1.0);
    score.noteEnded (1, 1.5);
    score.noteStarted (1, 7, 66, 370.0, 0.8, 1.5);
    score.addTechnique (1, { ScoreTechnique::Type::hammerOn });
    score.noteEnded (1, 2.0);
    score.endCapture (4.0);

    const auto file = juce::File::createTempFile (".mid");
    NotationExporter exporter;
    CHECK_MSG (exporter.write (score, NotationFormat::midi, file), exporter.getLastError());

    juce::MidiFile midi;
    {
        juce::FileInputStream stream (file);
        CHECK (midi.readFrom (stream));
    }

    int rpnTracks = 0, noteTracks = 0, wheel = 0, legatoOn = 0, legatoOff = 0;

    for (int t = 0; t < midi.getNumTracks(); ++t)
    {
        const auto* track = midi.getTrack (t);
        bool notes = false, rpn101 = false, rpn100 = false, data6 = false;

        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto& m = track->getEventPointer (i)->message;
            notes = notes || m.isNoteOn();

            if (m.isController() && m.getTimeStamp() <= 0.0)
            {
                rpn101 = rpn101 || (m.getControllerNumber() == 101 && m.getControllerValue() == 0);
                rpn100 = rpn100 || (m.getControllerNumber() == 100 && m.getControllerValue() == 0);
                data6 = data6 || m.getControllerNumber() == 6;
            }

            if (m.isPitchWheel() && m.getPitchWheelValue() != 8192)
                ++wheel;

            if (m.isController() && m.getControllerNumber() == 68)
                (m.getControllerValue() >= 64 ? legatoOn : legatoOff)++;
        }

        noteTracks += notes ? 1 : 0;
        rpnTracks += (notes && rpn101 && rpn100 && data6) ? 1 : 0;
    }

    CHECK (noteTracks >= 2);
    CHECK_MSG (rpnTracks == noteTracks, "a string track does not set its bend range at tick 0");
    CHECK_MSG (wheel > 0, "the bend has no pitch-wheel movement");
    CHECK_MSG (legatoOn >= 1 && legatoOff >= 1, "the hammer-on is not bracketed by CC 68");
    file.deleteFile();
}

LUTHIER_TEST (BassTechniques, aCapturedStrikeCarriesItsForceAndContact)
{
    // BT-24 (midi-export 9): BASS_TECH keeps the strike's force and fret contact.
    LuthierEngine engine;
    engine.prepare (48000.0, 256);
    engine.setGuitarType (GuitarType::JazzBass);

    PerformanceCapture capture;
    capture.prepare (48000.0);
    engine.setPerformanceCapture (&capture);

    CaptureClock clock;
    clock.sampleRate = 48000.0;
    capture.beginBlock (clock);

    auto note = [&engine] (int s, double fret, double velocity, int type)
    {
        NoteOnEvent e;
        e.stringIndex = s;
        e.fretPosition = fret;
        e.velocity = velocity;
        e.pitchHz = engine.getTuningEngine().computeFrequency (s, fret);
        e.bassTechnique = type;
        return e;
    };

    engine.triggerNoteNow (note (3, 5.0, 1.0, 1));
    engine.triggerNoteNow (note (1, 7.0, 0.3, 1));

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    engine.processBlock (buffer, midi);
    engine.setPerformanceCapture (nullptr);
    capture.drain();

    std::vector<double> forces;

    for (const auto& ev : capture.getEvents())
    {
        if (ev.event.eventClass != LuthierEventClass::bassTech)
            continue;

        CHECK (ev.event.has ("force"));
        CHECK (ev.event.has ("contact"));
        CHECK_NEAR (ev.event.getReal ("contact"), engine.getSlapEngine().getSettings().fretContact, 0.01);
        forces.push_back (ev.event.getReal ("force"));
    }

    CHECK (forces.size() == 2);

    if (forces.size() == 2)
        CHECK_MSG (forces[0] > forces[1], "a harder slap was not captured with more force");
}

LUTHIER_TEST (SlapWiring, aStringUnderTheSlideBarIsNotSlapped)
{
    // SS-22 (string-slap-technique.md, technique-cascade 3.4): the bar holds
    // the string, and the thumb cannot get at it - no clack there.
    auto clacks = [] (bool slideOn)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.setGuitarType (GuitarType::JazzBass);

        SlideSettings settings;
        settings.enabled = slideOn;
        settings.mode = SlideMode::lapSteel;
        engine.setSlideSettings (settings);

        const int low = engine.getNumStrings() - 1;

        if (slideOn)
            engine.getSlideEngine().noteOn (low, engine.getNumStrings());

        NoteOnEvent e;
        e.stringIndex = low;
        e.fretPosition = 3.0;
        e.velocity = 1.0;
        e.pitchHz = engine.getTuningEngine().computeFrequency (low, 3.0);
        e.bassTechnique = 1;   // a thumb slap
        e.technique = slideOn ? Technique::SlideGuitar : Technique::Pluck;
        engine.triggerNoteNow (e);

        return engine.getPlayingNoise().getPool().getTriggerCount (NoiseClass::fretBuzz);
    };

    CHECK_MSG (clacks (false) >= 1, "a thumb slap off the bar did not clack");
    CHECK_MSG (clacks (true) == 0, "a string under the bar was slapped");
}

LUTHIER_TEST (StrumDynamics, upStrokesChirpMoreAndClickLess)
{
    // SD-5 (strum-dynamics 2.1): the same strike as part of a down-stroke and
    // of an up-stroke - up has more chirp and less click.
    auto levels = [] (int direction)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.setGuitarType (GuitarType::Stratocaster);   // picked (an acoustic defaults to fingers)

        NoteOnEvent e;
        e.stringIndex = 5;   // wound, so it chirps
        e.fretPosition = 0.0;
        e.velocity = 0.8;
        e.pitchHz = engine.getTuningEngine().computeFrequency (5, 0.0);
        e.strumDirection = direction;
        engine.triggerNoteNow (e);

        auto& pool = engine.getPlayingNoise().getPool();
        return std::make_pair (activeLevel (pool, NoiseClass::pickClick), activeLevel (pool, NoiseClass::pickChirp));
    };

    const auto down = levels (1);
    const auto up = levels (-1);
    const auto none = levels (0);

    CHECK (down.first > 0.0 && up.second > 0.0);
    CHECK_MSG (up.first < down.first, "the up-stroke clicked no less than the down-stroke");
    CHECK_MSG (up.second > down.second, "the up-stroke chirped no more than the down-stroke");

    // The stroke's angle is for that strike only: the next plain note is as before.
    CHECK (none.first > up.first && none.first < down.first);
}

LUTHIER_TEST (Controllers, mpeTrafficIsDetected)
{
    // HI-37 (host-integration 9.1): notes on member channels 2 and 3 with
    // per-channel bend, while MPE is off, raise the suggestion once.
    LuthierEngine engine;
    engine.prepare (48000.0, 256);
    auto& midi = engine.getMidiInterpreter();
    midi.setPlayingMode (PlayingMode::Poly);
    midi.setMpeEnabled (false);

    juce::AudioBuffer<float> buffer (2, 256);

    auto play = [&] (std::initializer_list<juce::MidiMessage> messages)
    {
        juce::MidiBuffer in;

        for (const auto& m : messages)
            in.addEvent (m, 0);

        buffer.clear();
        engine.processBlock (buffer, in);
    };

    // An ordinary keyboard on channel 1 bending: not MPE.
    play ({ juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), juce::MidiMessage::pitchWheel (1, 9000) });
    CHECK (! midi.takeMpeTrafficDetected());

    // One member channel alone: not yet.
    play ({ juce::MidiMessage::noteOn (2, 64, (juce::uint8) 100), juce::MidiMessage::pitchWheel (2, 9000) });
    CHECK (! midi.takeMpeTrafficDetected());

    // Two member channels with notes, and a per-channel bend.
    play ({ juce::MidiMessage::noteOn (3, 67, (juce::uint8) 100), juce::MidiMessage::pitchWheel (3, 7000) });
    CHECK (midi.takeMpeTrafficDetected());
    CHECK (! midi.takeMpeTrafficDetected());   // said once

    // With MPE on, nothing to suggest.
    midi.setMpeEnabled (true);
    play ({ juce::MidiMessage::noteOn (4, 60, (juce::uint8) 100), juce::MidiMessage::controllerEvent (4, 74, 90) });
    CHECK (! midi.takeMpeTrafficDetected());
}

LUTHIER_TEST (Slide, vibratoMovesTheBar)
{
    // SG-12 (slide-guitar.md 3.2): bar vibrato is a movement, so the same
    // travel is more cents higher up the neck, and scales with the travel.
    const double low = SlideEngine::vibratoCents (1.0, 3.0, 648.0);
    const double high = SlideEngine::vibratoCents (1.0, 15.0, 648.0);

    CHECK (SlideEngine::vibratoCents (0.0, 7.0, 648.0) == 0.0);
    CHECK (high > low * 1.9);
    CHECK_NEAR (SlideEngine::vibratoCents (2.0, 7.0, 648.0), 2.0 * SlideEngine::vibratoCents (1.0, 7.0, 648.0), 1.0e-9);

    // 1 mm at the 12th fret of a 648 mm scale: 1200/ln2 x 1/324 cents.
    CHECK_NEAR (SlideEngine::vibratoCents (1.0, 12.0, 648.0), 1200.0 / std::log (2.0) / 324.0, 1.0e-6);
}

LUTHIER_TEST (Slide, frictionFollowsMaterialAndSpeed)
{
    // SG-14 (slide-guitar.md 5.1): the bar's friction noise is amount x
    // material friction x speed - silent at amount 0, louder on steel than
    // glass, and louder for a fast move than a slow one.
    auto noiseOf = [] (SlideMaterial material, double amount, double seconds)
    {
        auto render = [&] (double noise)
        {
            LuthierEngine engine;
            engine.prepare (48000.0, 256);
            engine.setGuitarType (GuitarType::Dreadnought);

            SlideSettings settings;
            settings.enabled = true;
            settings.mode = SlideMode::lapSteel;
            settings.noiseAmount = noise;
            engine.setSlideSettings (settings);

            SlideBar bar;
            bar.material = material;
            engine.getSlideEngine().setBar (bar);

            NoteOnEvent first;
            first.stringIndex = 5;
            first.fretPosition = 3.0;
            first.technique = Technique::SlideGuitar;
            first.pitchHz = engine.getTuningEngine().computeFrequency (5, 3.0);
            engine.triggerNoteNow (first);

            NoteOnEvent move = first;
            move.slideFromFret = 3.0;
            move.fretPosition = 10.0;
            move.slideSeconds = seconds;
            move.pitchHz = engine.getTuningEngine().computeFrequency (5, 10.0);
            engine.triggerNoteNow (move);

            std::vector<float> out;
            juce::AudioBuffer<float> buffer (2, 256);
            juce::MidiBuffer midi;

            for (int b = 0; b < 40; ++b)
            {
                buffer.clear();
                engine.processBlock (buffer, midi);
                out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 256);
            }

            return out;
        };

        const auto with = render (amount);
        const auto without = render (0.0);
        double energy = 0.0;

        for (size_t i = 0; i < with.size(); ++i)
            energy += (double) (with[i] - without[i]) * (with[i] - without[i]);

        return energy;
    };

    CHECK (noiseOf (SlideMaterial::glass, 0.0, 0.2) == 0.0);

    const double glass = noiseOf (SlideMaterial::glass, 1.0, 0.2);
    const double steel = noiseOf (SlideMaterial::steel, 1.0, 0.2);
    const double slow = noiseOf (SlideMaterial::steel, 1.0, 0.4);

    CHECK (glass > 0.0);
    CHECK_MSG ((steel > glass) == (getSlideMaterial (SlideMaterial::steel).friction > getSlideMaterial (SlideMaterial::glass).friction),
               "friction noise does not follow the material");
    CHECK_MSG (steel > slow, "a fast move was no noisier than a slow one");
}

LUTHIER_TEST (Slide, aShortBarCoversFewerStrings)
{
    // SG-6 (slide-guitar.md 2): 40 mm at 10.5 mm spacing reaches 4 strings.
    SlideEngine slide;
    slide.prepare (48000.0);

    SlideSettings settings;
    settings.enabled = true;
    settings.mode = SlideMode::bottleneck;
    slide.setSettings (settings);

    SlideBar bar;
    bar.lengthMm = 40.0;
    slide.setBar (bar);

    CHECK (slide.noteOn (0, 6));
    CHECK (slide.noteOn (3, 6));
    CHECK_MSG (! slide.noteOn (5, 6), "a 40 mm bar reached six strings");

    bar.lengthMm = 70.0;
    slide.setBar (bar);
    CHECK (slide.noteOn (5, 6));
}

LUTHIER_TEST (Slide, diameterShapesTheContact)
{
    // SG-6: a fatter bar absorbs less and clanks lower.
    auto make = [] (double diameter)
    {
        auto slide = std::make_unique<SlideEngine>();
        slide->prepare (48000.0);

        SlideSettings settings;
        settings.enabled = true;
        settings.mode = SlideMode::lapSteel;
        slide->setSettings (settings);

        SlideBar bar;
        bar.diameterMm = diameter;
        slide->setBar (bar);
        slide->noteOn (2, 6);
        return slide;
    };

    const auto thin = make (15.0), fat = make (30.0);
    CHECK (fat->sustainScale (2) > thin->sustainScale (2));
    CHECK (fat->makeClank (2, 0.8).startHz < thin->makeClank (2, 0.8).startHz);
}

LUTHIER_TEST (NoisePool, clickIsExcitationAndTheRestAreSurface)
{
    // PN-6 (pick-noise.md 1): the click excites the string (so it rides the
    // instrument); chirp and squeak are surface noise.
    auto route = [] (const NoiseEvent& event)
    {
        NoiseEngine pool;
        pool.prepare (48000.0);
        pool.trigger (event);

        std::array<double, 6> excitation {}, surface {};
        double e = 0.0, s = 0.0;

        for (int i = 0; i < 2000; ++i)
        {
            pool.processSample (excitation.data(), surface.data(), 6);
            e += std::abs (excitation[(size_t) event.stringIndex]);
            s += std::abs (surface[(size_t) event.stringIndex]);
        }

        return std::make_pair (e, s);
    };

    PickSettings pick;
    const auto click = route (PlayingNoise::makeClick (pick, 2, 0.8));
    CHECK (click.first > 0.0);
    CHECK (click.second == 0.0);

    StringNoiseInfo wound;
    wound.wound = true;
    wound.windingPitchPerMm = 6.5;
    wound.windingDepth = 1.0;

    const auto chirp = route (PlayingNoise::makeChirp (pick, wound, 2, 0.8));
    CHECK (chirp.first == 0.0);
    CHECK (chirp.second > 0.0);

    SqueakSettings squeak;
    const auto sq = route (PlayingNoise::makeSqueak (squeak, wound, 2, 120.0, 0.25, 5.0));
    CHECK (sq.first == 0.0);
    CHECK (sq.second > 0.0);
}

LUTHIER_TEST (PickNoise, noiseRidesTheInstrument)
{
    // PN-T5 (pick-noise.md 1, 9): the same click through two instruments comes
    // out differently coloured; with every pick amount at 0 the noise path
    // changes nothing, sample for sample.
    auto render = [] (GuitarType type, double clickAmount, double chirpAmount)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.setGuitarType (type);
        engine.setUseFingers (false);

        PickSettings pick;
        pick.clickAmount = clickAmount;
        pick.chirpAmount = chirpAmount;
        pick.scrapeAmount = 0.0;
        engine.setPickNoise (pick);

        NoteOnEvent e;
        e.stringIndex = 1;
        e.velocity = 0.8;
        e.pitchHz = engine.getTuningEngine().computeFrequency (1, 0.0);
        engine.triggerNoteNow (e);

        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;

        for (int b = 0; b < 4; ++b)
        {
            buffer.clear();
            engine.processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 256);
        }

        return out;
    };

    auto centroid = [] (const std::vector<float>& x)
    {
        // Spectral centroid of the first 10 ms by zero-crossing density: a
        // crude proxy that is enough to tell two colourings apart.
        int crossings = 0;

        for (size_t i = 1; i < 480; ++i)
            crossings += ((x[i - 1] <= 0.0f) != (x[i] <= 0.0f)) ? 1 : 0;

        return (double) crossings;
    };

    auto clickOnly = [&] (GuitarType type)
    {
        const auto with = render (type, 1.0, 0.0);
        const auto without = render (type, 0.0, 0.0);
        std::vector<float> difference (with.size());

        for (size_t i = 0; i < with.size(); ++i)
            difference[i] = with[i] - without[i];

        return difference;
    };

    const auto dread = clickOnly (GuitarType::Dreadnought);
    const auto strat = clickOnly (GuitarType::Stratocaster);

    double dreadEnergy = 0.0, stratEnergy = 0.0;

    for (size_t i = 0; i < 480; ++i)
        dreadEnergy += (double) dread[i] * dread[i], stratEnergy += (double) strat[i] * strat[i];

    CHECK (dreadEnergy > 0.0 && stratEnergy > 0.0);
    // Different instruments colour the click differently: their first 10 ms,
    // each normalised, are far from the same waveform.
    double cross = 0.0;

    for (size_t i = 0; i < 480; ++i)
        cross += (double) dread[i] * strat[i];

    const double correlation = cross / std::sqrt (dreadEnergy * stratEnergy);
    juce::ignoreUnused (centroid);
    CHECK_MSG (correlation < 0.9, "the click sounds the same through a dreadnought and a Stratocaster (correlation "
                                    + juce::String (correlation, 3) + ")");

    // Zero is silent and free: two renders at all-zero are identical.
    const auto a = render (GuitarType::Stratocaster, 0.0, 0.0);
    const auto b = render (GuitarType::Stratocaster, 0.0, 0.0);
    CHECK (a == b);
}

LUTHIER_TEST (Buzz, aBendMovesTheBuzzUpTheNeck)
{
    // FB-26 (fret-buzz.md 8): a bend lifts the string next to the finger and
    // brings it closer further up.
    FretBuzz buzz;
    buzz.setGeometry (needsATech());

    const int s = 2;
    const double near0 = buzz.clearanceFor (s, 5.0, 6);
    const double far0 = buzz.clearanceFor (s, 5.0, 12);

    buzz.setBendCents (s, 200.0);
    CHECK (buzz.clearanceFor (s, 5.0, 6) > near0 + 0.1);
    CHECK (buzz.clearanceFor (s, 5.0, 12) < far0 - 0.1);

    // Unbent strings are untouched.
    CHECK_NEAR (buzz.clearanceFor (s + 1, 5.0, 6), buzz.getGeometry().clearanceMm (s + 1, 5.0, 6), 1.0e-12);
}

LUTHIER_TEST (Buzz, lightBuzzSitsThirtyToFortyDecibelsUnder)
{
    // FB-13 (fret-buzz.md 0.4): a light contact (0.05-0.1 mm of excess) buzzes
    // 30-40 dB under the note; a hard one comes up to about 22 dB under.
    FretBuzz buzz;
    SetupGeometry g;
    g.fretHeight = 1.0;
    buzz.setGeometry (g);

    for (const double excess : { 0.05, 0.075, 0.1 })
    {
        const double db = gainToDb (buzz.levelFor (excess) / PlayingNoise::kNoteReference);
        CHECK_MSG (db <= -30.0 && db >= -40.0, juce::String (excess) + " mm buzzes at " + juce::String (db, 1) + " dB");
    }

    CHECK_NEAR (gainToDb (buzz.levelFor (0.3) / PlayingNoise::kNoteReference), -22.0, 0.01);
    CHECK (buzz.levelFor (0.0) == 0.0);
}

LUTHIER_TEST (MidiExport, captureCarriesTheLiveNoiseEvents)
{
    // MX-1 (midi-export 6): what the live MIDI out sends as PICK (and SQUEAK,
    // BUZZ, CLANK) is in the captured take, and survives a Luthier export.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    if (auto* type = processor.getState().getParameter (ParamIDs::guitarType))
        type->setValueNotifyingHost (type->convertTo0to1 ((float) (int) GuitarType::Stratocaster));

    juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), 512);

    for (int b = 0; b < 20; ++b)
    {
        juce::MidiBuffer midi;

        if (b == 4)
            midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 110), 10);

        buffer.clear();
        processor.processBlock (buffer, midi);
    }

    processor.drainPerformanceCapture();
    const auto take = processor.getPerformanceCapture().toPerformance (48000.0);
    CHECK_MSG (take.countEvents (LuthierEventClass::pick) >= 1, "the take has no PICK event");

    auto file = juce::File::createTempFile (".mid");
    MidiExportOptions options;
    options.profile = MidiProfile::luthier;
    juce::String error;
    CHECK_MSG (MidiProfiles::exportToFile (take, options, file, &error), error);

    MidiPerformance back (48000.0);
    CHECK (MidiProfiles::importFromFile (file, back, 48000.0).ok);
    CHECK (back.countEvents (LuthierEventClass::pick) == take.countEvents (LuthierEventClass::pick));
    file.deleteFile();
}

LUTHIER_TEST (Notation, chordExtractionOnAHundredProgressions)
{
    // NE-37 (notation-export 4): 100 four-chord progressions of common
    // qualities, voiced as a guitarist would (root in the bass, the rest
    // spread above), named right at least 95 % of the time.
    struct Quality { const char* suffix; std::vector<int> intervals; };

    const Quality qualities[] =
    {
        { "", { 0, 4, 7 } }, { "m", { 0, 3, 7 } }, { "7", { 0, 4, 7, 10 } }, { "maj7", { 0, 4, 7, 11 } },
        { "m7", { 0, 3, 7, 10 } }, { "dim", { 0, 3, 6 } }, { "sus4", { 0, 5, 7 } }, { "sus2", { 0, 2, 7 } },
        { "m7b5", { 0, 3, 6, 10 } }, { "aug", { 0, 4, 8 } }
    };

    ChordDetector detector;
    detector.prepare (48000.0);
    juce::Random random (0x37);
    int right = 0, total = 0;
    juce::StringArray wrong;

    for (int progression = 0; progression < 100; ++progression)
    {
        for (int chord = 0; chord < 4; ++chord)
        {
            const int root = random.nextInt (12);
            const auto& q = qualities[random.nextInt ((int) std::size (qualities))];

            // Bass note in the low register, then the chord tones above it,
            // one voicing in three doubling the root an octave up.
            const int bass = 40 + ((root - 40 % 12) + 12) % 12;
            std::vector<int> notes { bass };

            for (size_t i = 1; i < q.intervals.size(); ++i)
                notes.push_back (bass + 12 + q.intervals[i]);

            if (random.nextInt (3) == 0)
                notes.push_back (bass + 12);

            const auto symbol = detector.detect (notes.data(), (int) notes.size());
            const auto expected = juce::String (getPitchClassName (root)) + q.suffix;

            ++total;

            if (symbol.toString() == expected)
                ++right;
            else if (wrong.size() < 8)
                wrong.add (expected + " read as " + symbol.toString());
        }
    }

    CHECK_MSG (right >= total * 95 / 100,
               juce::String (right) + " of " + juce::String (total) + " right: " + wrong.joinIntoString ("; "));
}

LUTHIER_TEST (Notation, guitarProCarriesDiagramsAndWhammy)
{
    // NE-12 / NE-13 (notation-export 2.2): one diagram per chord, from the
    // voicing played; whammy as beat properties; the other note techniques.
    PerformanceScore score;
    score.beginCapture (120.0, 4, 4);

    // An open C chord (x32010), then G (320003), then C again.
    const int open[6] = { 64, 59, 55, 50, 45, 40 };
    auto chord = [&] (const int* frets, double beat)
    {
        for (int s = 0; s < 6; ++s)
            if (frets[s] >= 0)
                score.noteStarted (s, frets[s], open[s] + frets[s], 440.0, 0.8, beat);

        for (int s = 0; s < 6; ++s)
            if (frets[s] >= 0)
                score.noteEnded (s, beat + 1.0);
    };

    const int c[6] = { 0, 1, 0, 2, 3, -1 };
    const int g[6] = { 3, 0, 0, 0, 2, 3 };
    score.addChordSymbol (0.0, "C");
    chord (c, 0.0);
    score.addChordSymbol (1.0, "G");
    chord (g, 1.0);
    score.addChordSymbol (2.0, "C");
    chord (c, 2.0);

    score.noteStarted (0, 5, 69, 880.0, 0.8, 3.0);
    ScoreTechnique whammy;
    whammy.type = ScoreTechnique::Type::whammy;
    whammy.curve = { { 0.0, 0.0 }, { 0.5, -1.0 }, { 1.0, 0.0 } };
    score.addTechnique (0, whammy);
    score.addTechnique (0, { ScoreTechnique::Type::letRing });
    score.noteEnded (0, 4.0);
    score.endCapture (4.0);

    NotationExporter exporter;
    NotationExportOptions options;
    options.chordDiagrams = true;
    const auto xml = exporter.renderGuitarProXml (score, options);

    CHECK (xml.contains ("<Property name=\"DiagramCollection\">"));
    CHECK (xml.contains ("name=\"C\"") && xml.contains ("name=\"G\""));

    int items = 0;

    for (int i = xml.indexOf ("<Item "); i >= 0; i = xml.indexOf (i + 1, "<Item "))
        ++items;

    CHECK_MSG (items == 2, juce::String (items) + " diagrams for two chords");

    // The C diagram has the five strings it was played on; the G all six.
    const auto cItem = xml.fromFirstOccurrenceOf ("name=\"C\"", false, false).upToFirstOccurrenceOf ("</Item>", false, false);
    const auto gItem = xml.fromFirstOccurrenceOf ("name=\"G\"", false, false).upToFirstOccurrenceOf ("</Item>", false, false);
    CHECK (juce::StringArray::fromTokens (cItem, "\n", "").size() > 0);
    CHECK (cItem.contains ("<Fret string=\"1\" fret=\"3\"/>"));   // C on the A string (GPIF counts up from the low E)
    CHECK (gItem.contains ("<Fret string=\"0\" fret=\"3\"/>"));

    // Beats reference the diagrams; the second C reuses the first.
    CHECK (xml.contains ("<Chord>0</Chord>") && xml.contains ("<Chord>1</Chord>"));

    CHECK (xml.contains ("<Property name=\"WhammyBar\"><Enable/></Property>"));
    CHECK (xml.contains ("WhammyBarMiddleValue\"><Float>-50.00</Float>"));
    CHECK (! xml.contains ("<!-- whammy"));
    CHECK (xml.contains ("<Property name=\"LetRing\">"));
}

LUTHIER_TEST (Notation, aGraceHammerIsAGraceNote)
{
    // NE-9 (notation-export 2.1): a quick hammered ornament is a grace note
    // slurred into its target, and reads back as the same two notes.
    PerformanceScore score;
    score.beginCapture (120.0, 4, 4);
    score.noteStarted (2, 5, 60, 261.6, 0.8, 0.0);
    score.noteEnded (2, 0.125);
    score.noteStarted (2, 7, 62, 293.7, 0.8, 0.125);
    score.addTechnique (2, { ScoreTechnique::Type::hammerOn });
    score.noteEnded (2, 1.0);
    score.noteStarted (2, 5, 60, 261.6, 0.8, 1.0);   // an ordinary note after it
    score.noteEnded (2, 2.0);
    score.endCapture (4.0);

    NotationExporter exporter;
    const auto xml = exporter.renderMusicXml (score);

    CHECK (xml.contains ("<grace"));
    CHECK (xml.contains ("<slur type=\"start\"/>") && xml.contains ("<slur type=\"stop\"/>"));
    CHECK (xml.indexOf ("<grace") == xml.lastIndexOf ("<grace"));   // one grace, not the ordinary note

    NotationImporter importer;
    PerformanceScore back;
    CHECK (importer.readMusicXml (xml, back));
    CHECK (back.getTotalNoteCount() == 3);

    const auto notes = back.getTrack (0).measures[0].collectNotes();

    if (notes.size() == 3)
    {
        CHECK (notes[0]->midiNote == 60 && notes[0]->durationBeats < 0.25);
        CHECK (notes[1]->midiNote == 62 && notes[1]->hasTechnique (ScoreTechnique::Type::hammerOn));
        CHECK_NEAR (notes[1]->startBeat, 0.125, 0.01);
        CHECK_NEAR (notes[2]->startBeat, 1.0, 1.0e-6);
    }
}

LUTHIER_TEST (Notation, guitarProRoundTripsStringsAndFrets)
{
    // NE-4 / NE-34 (notation-export 2.2): a .gp written by Luthier reads back
    // note for note - timing, string, fret and the techniques GPIF carries -
    // with chords as one beat, not an arpeggio.
    PerformanceScore score;
    score.beginCapture (100.0, 4, 4);

    const int open[6] = { 64, 59, 55, 50, 45, 40 };

    // A chord on beat 0, then a bent note, a hammer-on, a palm mute, a rest, a slide.
    for (int s = 2; s < 5; ++s)
        score.noteStarted (s, 2, open[s] + 2, 440.0, 0.8, 0.0);

    for (int s = 2; s < 5; ++s)
        score.noteEnded (s, 1.0);

    ScoreTechnique bend;
    bend.type = ScoreTechnique::Type::bend;
    bend.value = 1.0;
    score.noteStarted (1, 8, open[1] + 8, 440.0, 0.8, 1.0);
    score.addTechnique (1, bend);
    score.noteEnded (1, 2.0);

    score.noteStarted (1, 10, open[1] + 10, 440.0, 0.8, 2.0);
    score.addTechnique (1, { ScoreTechnique::Type::hammerOn });
    score.noteEnded (1, 2.5);

    score.noteStarted (5, 0, open[5], 440.0, 0.8, 2.5);
    score.addTechnique (5, { ScoreTechnique::Type::palmMute });
    score.noteEnded (5, 3.0);

    score.noteStarted (0, 12, open[0] + 12, 440.0, 0.8, 3.5);
    score.addTechnique (0, { ScoreTechnique::Type::slideUp });
    score.noteEnded (0, 4.0);
    score.endCapture (4.0);

    const auto file = juce::File::createTempFile (".gp");
    NotationExporter exporter;
    CHECK_MSG (exporter.write (score, NotationFormat::guitarPro, file), exporter.getLastError());
    CHECK (NotationImporter::canRead (file));

    NotationImporter importer;
    PerformanceScore back;
    CHECK_MSG (importer.read (file, back), importer.getLastError());

    const auto a = score.getTrack (0).measures[0].collectNotes();
    const auto b = back.getTrack (0).measures[0].collectNotes();
    CHECK_MSG (a.size() == b.size(), juce::String ((int) b.size()) + " notes back of " + juce::String ((int) a.size()));

    for (size_t i = 0; i < juce::jmin (a.size(), b.size()); ++i)
    {
        CHECK_MSG (a[i]->stringIndex == b[i]->stringIndex && a[i]->fret == b[i]->fret && a[i]->midiNote == b[i]->midiNote,
                   "note " + juce::String ((int) i) + " moved");
        CHECK_NEAR (b[i]->startBeat, a[i]->startBeat, 1.0e-6);

        for (const auto& t : a[i]->techniques)
            CHECK_MSG (b[i]->hasTechnique (t.type), "note " + juce::String ((int) i) + " lost " + getTechniqueName (t.type));
    }

    if (b.size() >= 4)
        CHECK_NEAR (b[3]->findTechnique (ScoreTechnique::Type::bend) != nullptr ? b[3]->findTechnique (ScoreTechnique::Type::bend)->value : 0.0, 1.0, 1.0e-6);

    CHECK (back.getMeta().tempoBpm > 99.0 && back.getMeta().tempoBpm < 101.0);
    file.deleteFile();
}

LUTHIER_TEST (ToneMatch, cabinetSlotsReplaceTheirOwnMic)
{
    // TM-7 (tone-match 1): cab slot 1 replaces mic 1, slot 2 mic 2 - so with
    // the second mic out of use, slot 2 changes nothing.
    juce::WavAudioFormat wav;
    const auto file = writeIr (wav, ".wav", 0.1);

    IrSlot slotA, slotB;

    for (auto* slot : { &slotA, &slotB })
    {
        slot->prepare (48000.0, 512);
        CHECK (slot->load (file));
        slot->setMix (1.0);
    }

    juce::Thread::sleep (300);

    auto render = [&] (bool engageA, bool engageB)
    {
        slotA.setEngaged (engageA);
        slotB.setEngaged (engageB);

        LuthierEngine engine;
        engine.prepare (48000.0, 512);
        engine.setGuitarType (GuitarType::Stratocaster);
        engine.getCabinetEngine().setEnabled (true);
        engine.getCabinetEngine().setDualMicEnabled (false);
        engine.getCabinetEngine().setUserIrSlots (&slotA, &slotB);

        NoteOnEvent e;
        e.stringIndex = 3;
        e.fretPosition = 2.0;
        e.pitchHz = engine.getTuningEngine().computeFrequency (3, 2.0);
        engine.triggerNoteNow (e);

        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;

        for (int b = 0; b < 20; ++b)
        {
            buffer.clear();
            engine.processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 512);
        }

        return out;
    };

    const auto none = render (false, false);
    const auto onlyB = render (false, true);
    const auto onlyA = render (true, false);

    double diffB = 0.0, diffA = 0.0, energy = 0.0;

    for (size_t i = 0; i < none.size(); ++i)
    {
        diffB = juce::jmax (diffB, (double) std::abs (onlyB[i] - none[i]));
        diffA += (double) (onlyA[i] - none[i]) * (onlyA[i] - none[i]);
        energy += (double) none[i] * none[i];
    }

    CHECK (energy > 0.0);
    CHECK_MSG (diffB == 0.0, "cab slot 2 changed the sound with the second mic off");
    CHECK_MSG (diffA > energy * 0.01, "cab slot 1 did not replace mic 1");

    slotA.unload();
    slotB.unload();
    file.deleteFile();
}
