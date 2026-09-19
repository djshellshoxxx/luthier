/*  Practice tool tests (practice-tools.md section 11).

    The metronome test is the one that matters most and is the hardest to pass:
    half a millisecond of drift over a minute at 48 kHz is one part in 120 000,
    which no implementation that counts samples between clicks can reach. It is
    measured here by timing every click in a full minute of rendered audio, not
    by trusting the position the metronome reports.
*/

#include "TestFramework.h"

#include "../Practice/Metronome.h"
#include "../Practice/Looper.h"
#include "../Practice/Trainers.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;

    /** Renders `seconds` of metronome into one buffer, a block at a time, and
        returns the sample index of every click's onset.

        Onsets are found by looking for the leading edge of each click rather
        than by asking the metronome where it thinks it is: the question is
        whether the audio lands on time, which the metronome's own opinion cannot
        answer. */
    std::vector<int> renderAndFindClicks (Metronome& metronome, double seconds, int blockSize)
    {
        const int total = (int) (seconds * kSr);

        std::vector<float> rendered ((size_t) total, 0.0f);

        for (int position = 0; position < total; position += blockSize)
        {
            const int count = juce::jmin (blockSize, total - position);
            metronome.processBlock (rendered.data() + position, count);
        }

        std::vector<int> onsets;

        // A click starts from silence, so an onset is the first sample above a
        // floor after a run of samples below it.
        constexpr float floorLevel = 0.002f;

        bool inClick = false;
        int quietFor = 0;

        for (int i = 0; i < total; ++i)
        {
            const float magnitude = std::abs (rendered[(size_t) i]);

            if (magnitude > floorLevel)
            {
                if (! inClick && quietFor > 32)
                    onsets.push_back (i);

                inClick = true;
                quietFor = 0;
            }
            else
            {
                ++quietFor;

                if (quietFor > 32)
                    inClick = false;
            }
        }

        return onsets;
    }
}

//==============================================================================
/*  practice-tools 11: the inter-click interval must be within half a millisecond
    at 48 kHz, over sixty seconds at 120 bpm. */
LUTHIER_TEST (PracticeMetronome, interClickIntervalIsWithinHalfAMillisecond)
{
    Metronome metronome;
    metronome.prepare (kSr, 512);

    metronome.setTempo (120.0);
    metronome.setTimeSignature (4, 4);
    metronome.setSubdivision (ClickSubdivision::quarter);
    metronome.setLevelDb (0.0);
    metronome.setEnabled (true);

    // A block size that is not a divisor of the beat, so the clicks cannot
    // accidentally land on block boundaries.
    const auto onsets = renderAndFindClicks (metronome, 60.0, 511);

    // 120 bpm for 60 seconds is 120 beats.
    CHECK_MSG (onsets.size() >= 118,
               "only " + juce::String ((int) onsets.size()) + " clicks in a minute at 120 bpm");

    if (onsets.size() < 3)
        return;

    const double expectedInterval = 0.5 * kSr;         // half a second, in samples
    const double toleranceSamples = 0.0005 * kSr;      // half a millisecond

    double worstInterval = 0.0;

    for (size_t i = 1; i < onsets.size(); ++i)
    {
        const double interval = (double) (onsets[i] - onsets[i - 1]);
        worstInterval = juce::jmax (worstInterval, std::abs (interval - expectedInterval));
    }

    CHECK_MSG (worstInterval <= toleranceSamples,
               "worst inter-click interval was off by "
                 + juce::String (worstInterval / kSr * 1000.0, 4) + " ms");

    // And no accumulated drift across the whole minute, which is the failure a
    // per-click sample counter would show.
    const double totalSpan = (double) (onsets.back() - onsets.front());
    const double expectedSpan = expectedInterval * (double) (onsets.size() - 1);

    CHECK_MSG (std::abs (totalSpan - expectedSpan) <= toleranceSamples,
               "the metronome drifted "
                 + juce::String ((totalSpan - expectedSpan) / kSr * 1000.0, 3)
                 + " ms over a minute");
}

//==============================================================================
/*  The same accuracy at a tempo whose beat is not a whole number of samples,
    which is where a naive implementation rounds and drifts. */
LUTHIER_TEST (PracticeMetronome, staysAccurateAtAnAwkwardTempo)
{
    Metronome metronome;
    metronome.prepare (kSr, 512);

    // 137 bpm: a beat is 21021.897... samples, so every click falls between two.
    metronome.setTempo (137.0);
    metronome.setLevelDb (0.0);
    metronome.setEnabled (true);

    const auto onsets = renderAndFindClicks (metronome, 30.0, 433);

    CHECK (onsets.size() > 60);

    if (onsets.size() < 3)
        return;

    const double expectedInterval = 60.0 / 137.0 * kSr;

    const double totalSpan = (double) (onsets.back() - onsets.front());
    const double expectedSpan = expectedInterval * (double) (onsets.size() - 1);

    CHECK_MSG (std::abs (totalSpan - expectedSpan) <= 0.0005 * kSr,
               "drift at 137 bpm was "
                 + juce::String ((totalSpan - expectedSpan) / kSr * 1000.0, 3) + " ms");
}

//==============================================================================
/*  practice-tools 1: every Nth bar is silent, and the bar still advances. */
LUTHIER_TEST (PracticeMetronome, silentBarsMuteTheClickWithoutStoppingTheCount)
{
    Metronome metronome;
    metronome.prepare (kSr, 512);

    metronome.setTempo (120.0);
    metronome.setTimeSignature (4, 4);
    metronome.setLevelDb (0.0);

    // Every second bar is silent, so four bars should produce two bars of clicks.
    metronome.setSilentBarPeriod (2);
    metronome.setEnabled (true);

    // Four bars at 120 bpm in 4/4 is eight seconds.
    const auto onsets = renderAndFindClicks (metronome, 8.0, 512);

    CHECK_MSG ((int) onsets.size() >= 7 && (int) onsets.size() <= 9,
               juce::String ((int) onsets.size())
                 + " clicks over four bars with every second bar silent; expected about 8");

    // The bar counter has to have reached bar 3, or the metronome stopped
    // counting rather than stopped clicking.
    CHECK_MSG (metronome.getCurrentBar() >= 3,
               "the bar counter stopped at " + juce::String (metronome.getCurrentBar()));
}

//==============================================================================
/*  practice-tools 1: accents, and the silent accent level. */
LUTHIER_TEST (PracticeMetronome, accentPatternIsObeyed)
{
    Metronome metronome;
    metronome.prepare (kSr, 512);

    metronome.setTempo (120.0);
    metronome.setTimeSignature (4, 4);
    metronome.setLevelDb (0.0);

    // Silence beats 2, 3 and 4: only the downbeat should click.
    metronome.resetAccents();

    for (int beat = 1; beat < 4; ++beat)
        metronome.setBeatAccent (beat, BeatAccent::silent);

    metronome.setEnabled (true);

    // Two bars is four seconds, and should give two clicks.
    const auto onsets = renderAndFindClicks (metronome, 4.0, 512);

    CHECK_MSG ((int) onsets.size() <= 3,
               juce::String ((int) onsets.size())
                 + " clicks over two bars with three beats silenced; expected about 2");

    CHECK (metronome.getBeatAccent (0) == BeatAccent::accent);
    CHECK (metronome.getBeatAccent (1) == BeatAccent::silent);
}

//==============================================================================
/*  practice-tools 0.1: a closed practice tool costs nothing, and a disabled
    metronome writes silence. */
LUTHIER_TEST (PracticeMetronome, silentWhenDisabled)
{
    Metronome metronome;
    metronome.prepare (kSr, 512);

    metronome.setTempo (120.0);
    metronome.setLevelDb (0.0);
    metronome.setEnabled (false);

    std::vector<float> rendered (48000, 1.0f);

    for (int position = 0; position < 48000; position += 512)
        metronome.processBlock (rendered.data() + position, juce::jmin (512, 48000 - position));

    double peak = 0.0;

    for (float sample : rendered)
        peak = juce::jmax (peak, (double) std::abs (sample));

    CHECK_MSG (peak < 1.0e-6, "a disabled metronome produced audio at " + juce::String (peak));
}

//==============================================================================
/*  Every click sound must actually make a sound, and none may produce anything
    non-finite - a NaN in the click would poison the monitor bus. */
LUTHIER_TEST (PracticeMetronome, everyClickSoundIsAudibleAndFinite)
{
    for (int s = 0; s < (int) ClickSound::numSounds; ++s)
    {
        Metronome metronome;
        metronome.prepare (kSr, 512);

        metronome.setTempo (120.0);
        metronome.setSound ((ClickSound) s);
        metronome.setLevelDb (0.0);
        metronome.setEnabled (true);

        std::vector<float> rendered (48000, 0.0f);

        for (int position = 0; position < 48000; position += 512)
            metronome.processBlock (rendered.data() + position,
                                    juce::jmin (512, 48000 - position));

        CHECK_FINITE (rendered.data(), 48000);

        double peak = 0.0;

        for (float sample : rendered)
            peak = juce::jmax (peak, (double) std::abs (sample));

        CHECK_MSG (peak > 0.01,
                   juce::String (getClickSoundName ((ClickSound) s))
                     + " produced a peak of only " + juce::String (peak, 5));

        CHECK_MSG (peak <= 1.5,
                   juce::String (getClickSoundName ((ClickSound) s))
                     + " clipped at " + juce::String (peak, 3));
    }
}

//==============================================================================
/*  practice-tools 1: progressive tempo ramps from A to B over N bars. */
LUTHIER_TEST (PracticeMetronome, progressiveTempoRampsAndStops)
{
    Metronome metronome;
    metronome.prepare (kSr, 512);

    metronome.setTimeSignature (4, 4);
    metronome.setLevelDb (0.0);
    metronome.setEnabled (true);

    metronome.startProgressiveTempo (80.0, 120.0, 4);

    CHECK (metronome.isProgressiveTempoRunning());
    CHECK_NEAR (metronome.getTempo(), 80.0, 0.5);

    std::vector<float> rendered (2048, 0.0f);

    // Four bars at a tempo averaging 100 bpm is about ten seconds; render
    // fifteen to be sure the ramp completes.
    const int total = (int) (15.0 * kSr);

    for (int position = 0; position < total; position += 2048)
        metronome.processBlock (rendered.data(), 2048);

    CHECK_MSG (! metronome.isProgressiveTempoRunning(),
               "the tempo ramp never finished");

    CHECK_MSG (std::abs (metronome.getTempo() - 120.0) <= 1.0,
               "the ramp ended at " + juce::String (metronome.getTempo(), 2) + " bpm");
}

//==============================================================================
/*  The metronome travels in the preset, so its settings have to round-trip. */
LUTHIER_TEST (PracticeMetronome, settingsRoundTrip)
{
    Metronome metronome;
    metronome.prepare (kSr, 512);

    metronome.setTempo (143.0);
    metronome.setTimeSignature (7, 8);
    metronome.setSubdivision (ClickSubdivision::triplet);
    metronome.setSound (ClickSound::cowbell);
    metronome.setLevelDb (-12.0);
    metronome.setSilentBarPeriod (3);
    metronome.setBeatAccent (2, BeatAccent::ghost);

    const auto text = juce::JSON::toString (metronome.toVar(), false);

    Metronome restored;
    restored.prepare (kSr, 512);
    restored.fromVar (juce::JSON::parse (text));

    CHECK_NEAR (restored.getTempo(), 143.0, 0.001);
    CHECK (restored.getTimeSignature().numerator == 7);
    CHECK (restored.getTimeSignature().denominator == 8);
    CHECK (restored.getSubdivision() == ClickSubdivision::triplet);
    CHECK (restored.getSound() == ClickSound::cowbell);
    CHECK_NEAR (restored.getLevelDb(), -12.0, 0.001);
    CHECK (restored.getSilentBarPeriod() == 3);
    CHECK (restored.getBeatAccent (2) == BeatAccent::ghost);
}

//==============================================================================
/*  practice-tools 2: the looper records, closes a loop, plays it back and
    overdubs onto a second layer. */
LUTHIER_TEST (PracticeLooper, recordsClosesAndOverdubs)
{
    Looper looper;
    looper.prepare (kSr, 10.0);

    CHECK (looper.getState() == Looper::State::stopped);
    CHECK (looper.getLoopLengthSamples() == 0);

    juce::AudioBuffer<float> buffer (2, 512);

    auto renderBlocks = [&looper, &buffer] (int blocks, float value)
    {
        for (int i = 0; i < blocks; ++i)
        {
            for (int channel = 0; channel < 2; ++channel)
                for (int s = 0; s < 512; ++s)
                    buffer.getWritePointer (channel)[s] = value;

            looper.processBlock (buffer, 512);
        }
    };

    // ---- record the first layer -------------------------------------------------
    looper.press();
    CHECK (looper.getState() == Looper::State::recordingFirst);

    renderBlocks (20, 0.25f);

    // ---- close the loop ----------------------------------------------------------
    looper.press();
    renderBlocks (1, 0.25f);

    CHECK_MSG (looper.getState() == Looper::State::playing,
               "the loop did not close");

    const int length = looper.getLoopLengthSamples();

    CHECK_MSG (length > 0, "the closed loop has no length");
    CHECK (looper.getLayer (0).hasContent());
    CHECK (looper.getNumRecordedLayers() == 1);

    // ---- it plays back ------------------------------------------------------------
    buffer.clear();
    looper.processBlock (buffer, 512);

    CHECK_MSG (buffer.getMagnitude (0, 0, 512) > 0.1,
               "the loop played back nothing");

    // ---- overdub onto the next layer ------------------------------------------------
    looper.press();
    CHECK (looper.getState() == Looper::State::overdubbing);

    const int overdubLayer = looper.getActiveLayer();
    CHECK_MSG (overdubLayer != 0, "the overdub went onto the layer it was recording over");

    renderBlocks (4, 0.1f);

    looper.press();
    CHECK (looper.getState() == Looper::State::playing);

    CHECK (looper.getLayer (overdubLayer).hasContent());
    CHECK (looper.getNumRecordedLayers() == 2);

    // The loop length is unchanged by an overdub: the first layer defines it.
    CHECK (looper.getLoopLengthSamples() == length);
}

//==============================================================================
/*  practice-tools 2: a loop quantises to bars when the metronome is running. */
LUTHIER_TEST (PracticeLooper, loopLengthQuantisesToBars)
{
    Looper looper;
    looper.prepare (kSr, 30.0);

    // One bar of 4/4 at 120 bpm is two seconds.
    const int barSamples = (int) (2.0 * kSr);
    looper.setBarLengthSamples (barSamples);

    juce::AudioBuffer<float> buffer (2, 512);
    buffer.clear();

    looper.press();

    // Record for a bar and a bit: 2.3 seconds. The loop should close at two
    // bars' worth only if it is nearer that, and at one bar otherwise.
    const int blocks = (int) (2.3 * kSr / 512.0);

    for (int i = 0; i < blocks; ++i)
        looper.processBlock (buffer, 512);

    looper.press();
    looper.processBlock (buffer, 512);

    const int length = looper.getLoopLengthSamples();

    CHECK_MSG (length % barSamples == 0,
               "the loop closed at " + juce::String (length)
                 + " samples, which is not a whole number of "
                 + juce::String (barSamples) + "-sample bars");

    CHECK_MSG (length == barSamples,
               "2.3 seconds of recording rounded to " + juce::String (length / barSamples)
                 + " bars; the nearest is 1");
}

//==============================================================================
/*  practice-tools 2: undo and redo, per layer. */
LUTHIER_TEST (PracticeLooper, layerUndoAndRedo)
{
    Looper looper;
    looper.prepare (kSr, 5.0);

    auto& layer = looper.getLayer (0);

    CHECK (! layer.canUndo());
    CHECK (! layer.canRedo());

    std::vector<float> left (512, 0.5f), right (512, 0.5f);

    layer.record (left.data(), right.data(), 0, 512);
    CHECK (layer.hasContent());

    // Keep what is there, then record something different over it.
    layer.pushUndo();
    CHECK (layer.canUndo());

    std::vector<float> quiet (512, 0.01f);
    layer.setMode (LayerMode::replace);
    layer.record (quiet.data(), quiet.data(), 0, 512);

    CHECK_NEAR (layer.readLeft()[0], 0.01, 0.001);

    CHECK (layer.undo());
    CHECK_NEAR (layer.readLeft()[0], 0.5, 0.001);
    CHECK (layer.canRedo());

    CHECK (layer.redo());
    CHECK_NEAR (layer.readLeft()[0], 0.01, 0.001);

    // A new take clears the redo: there is nothing to go forward to any more.
    layer.pushUndo();
    CHECK (! layer.canRedo());
}

//==============================================================================
/*  practice-tools 2: reverse and half-speed are audio-only, and must not change
    what the layer holds. */
LUTHIER_TEST (PracticeLooper, reverseAndHalfSpeedDoNotAlterTheRecording)
{
    Looper looper;
    looper.prepare (kSr, 5.0);

    auto& layer = looper.getLayer (0);
    layer.prepareFilters (kSr);

    // A ramp, so reversal is visible in the output.
    std::vector<float> ramp (1024);

    for (int i = 0; i < 1024; ++i)
        ramp[(size_t) i] = (float) i / 1024.0f;

    layer.record (ramp.data(), ramp.data(), 0, 1024);

    const float firstSample = layer.readLeft()[0];
    const float lastSample = layer.readLeft()[1023];

    layer.setReversed (true);
    layer.setHalfSpeed (true);

    std::vector<float> outL (512, 0.0f), outR (512, 0.0f);
    layer.playInto (outL.data(), outR.data(), 0, 512, 1024);

    CHECK_FINITE (outL.data(), 512);

    // The stored audio is untouched by either setting.
    CHECK_NEAR (layer.readLeft()[0], firstSample, 1.0e-6);
    CHECK_NEAR (layer.readLeft()[1023], lastSample, 1.0e-6);

    // Reversed, the output starts at the loud end of the ramp.
    CHECK_MSG (outL[0] > 0.5f,
               "reversed playback started at " + juce::String (outL[0], 4)
                 + ", expected the end of the ramp");
}

//==============================================================================
/*  practice-tools 2: a muted layer contributes nothing. */
LUTHIER_TEST (PracticeLooper, mutedLayersAreSilent)
{
    Looper looper;
    looper.prepare (kSr, 5.0);

    auto& layer = looper.getLayer (0);
    layer.prepareFilters (kSr);

    std::vector<float> loud (512, 0.5f);
    layer.record (loud.data(), loud.data(), 0, 512);

    std::vector<float> outL (512, 0.0f), outR (512, 0.0f);

    layer.playInto (outL.data(), outR.data(), 0, 512, 512);

    double peak = 0.0;

    for (float sample : outL)
        peak = juce::jmax (peak, (double) std::abs (sample));

    CHECK (peak > 0.1);

    // Muted, and it adds nothing at all.
    std::fill (outL.begin(), outL.end(), 0.0f);
    std::fill (outR.begin(), outR.end(), 0.0f);

    layer.setMuted (true);
    layer.playInto (outL.data(), outR.data(), 0, 512, 512);

    peak = 0.0;

    for (float sample : outL)
        peak = juce::jmax (peak, (double) std::abs (sample));

    CHECK_MSG (peak < 1.0e-9, "a muted layer produced " + juce::String (peak));
}

//==============================================================================
/*  practice-tools 11: the session recorder's ring buffer never allocates in the
    audio thread, and never grows past its capacity. */
LUTHIER_TEST (PracticeSession, ringBufferNeverGrows)
{
    SessionRecorder recorder;

    // The shortest ring the recorder will allocate is a minute.
    CHECK (recorder.prepare (kSr, 1.0));

    CHECK (recorder.getRecordedSamples() == 0);

    const int capacity = (int) std::llround (recorder.getCapacityMinutes() * 60.0 * kSr);
    CHECK (capacity > 0);

    recorder.setEnabled (true);

    juce::AudioBuffer<float> buffer (2, 512);

    for (int channel = 0; channel < 2; ++channel)
        for (int s = 0; s < 512; ++s)
            buffer.getWritePointer (channel)[s] = 0.3f;

    // Twice round the ring, so the wrap is exercised rather than just the fill.
    const int blocks = (capacity / 512) * 2;

    for (int i = 0; i < blocks; ++i)
        recorder.processBlock (buffer, 512);

    const int recorded = recorder.getRecordedSamples();

    CHECK_MSG (recorded <= capacity,
               "the ring reports " + juce::String (recorded) + " samples in a "
                 + juce::String (capacity) + "-sample buffer");

    CHECK_MSG (recorded == capacity,
               "after two passes the ring should be full, but holds "
                 + juce::String (recorded) + " of " + juce::String (capacity));

    // Disabled, it stops accepting audio.
    recorder.setEnabled (false);
    recorder.reset();

    for (int i = 0; i < 10; ++i)
        recorder.processBlock (buffer, 512);

    CHECK_MSG (recorder.getRecordedSamples() == 0,
               "a disabled recorder recorded anyway");
}

//==============================================================================
/*  practice-tools 8: the recorder is off until it is asked for. */
LUTHIER_TEST (PracticeSession, disabledByDefault)
{
    SessionRecorder recorder;
    CHECK_MSG (! recorder.isEnabled(), "the session recorder was on by default");
}

//==============================================================================
/*  practice-tools 4: the scale trainer knows its scales. */
LUTHIER_TEST (PracticeTrainers, scaleTrainerKnowsItsScales)
{
    ScaleTrainer trainer;

    // C major: the white notes, and nothing else.
    trainer.setKey (0);
    trainer.setScale (ScaleType::ionian);

    CHECK (trainer.getNumDegrees() == 7);

    for (int pitchClass : { 0, 2, 4, 5, 7, 9, 11 })
        CHECK_MSG (trainer.containsPitchClass (pitchClass),
                   "C major is missing pitch class " + juce::String (pitchClass));

    for (int pitchClass : { 1, 3, 6, 8, 10 })
        CHECK_MSG (! trainer.containsPitchClass (pitchClass),
                   "C major wrongly contains pitch class " + juce::String (pitchClass));

    CHECK (trainer.getDegreeOf (0) == 1);
    CHECK (trainer.getDegreeOf (7) == 5);
    CHECK (trainer.getDegreeOf (1) == 0);

    CHECK (trainer.getPitchClassOfDegree (1) == 0);
    CHECK (trainer.getPitchClassOfDegree (5) == 7);

    // A minor pentatonic in A: A C D E G.
    trainer.setKey (9);
    trainer.setScale (ScaleType::minorPentatonic);

    CHECK (trainer.getNumDegrees() == 5);

    for (int pitchClass : { 9, 0, 2, 4, 7 })
        CHECK_MSG (trainer.containsPitchClass (pitchClass),
                   "A minor pentatonic is missing " + juce::String (pitchClass));

    // Every scale has to produce a sensible set.
    for (int s = 0; s < (int) ScaleType::custom; ++s)
    {
        ScaleTrainer each;
        each.setScale ((ScaleType) s);

        CHECK_MSG (each.getNumDegrees() >= 5,
                   juce::String (getScaleTypeName ((ScaleType) s)) + " has too few degrees");

        CHECK_MSG (each.containsPitchClass (each.getKey()),
                   juce::String (getScaleTypeName ((ScaleType) s)) + " does not contain its root");
    }

    // And a custom scale is whatever the user says it is.
    ScaleTrainer custom;
    const int wholeTone[] = { 0, 2, 4, 6, 8, 10 };
    custom.setCustomIntervals (wholeTone, 6);

    CHECK (custom.getScale() == ScaleType::custom);
    CHECK (custom.getNumDegrees() == 6);
    CHECK (custom.containsPitchClass (6));
    CHECK (! custom.containsPitchClass (7));
}

//==============================================================================
/*  The quiz asks answerable questions and scores them. */
LUTHIER_TEST (PracticeTrainers, scaleQuizScoresAnswers)
{
    ScaleTrainer trainer;
    trainer.setKey (0);
    trainer.setScale (ScaleType::ionian);
    trainer.setMode (ScaleTrainer::Mode::quiz);

    juce::Random random (0x5ca1e);

    for (int i = 0; i < 50; ++i)
    {
        const auto question = trainer.nextQuestion (random);

        CHECK_MSG (question.isNotEmpty(), "the quiz asked an empty question");

        const int expected = trainer.getExpectedPitchClass();

        CHECK_MSG (expected >= 0 && expected < 12,
                   "the quiz expects pitch class " + juce::String (expected));

        CHECK_MSG (trainer.containsPitchClass (expected),
                   "the quiz asked for a note outside its own scale");

        // The right answer in any octave counts.
        CHECK (trainer.answer (60 + expected));
        CHECK (trainer.answer (48 + expected));

        // And a note outside the scale does not.
        CHECK (! trainer.answer (60 + ((expected + 1) % 12)));
    }

    CHECK (trainer.getAsked() == 50);
    CHECK (trainer.getScore() > 0);
}

//==============================================================================
/*  practice-tools 5: the ear trainer poses answerable questions across every
    exercise and difficulty. */
LUTHIER_TEST (PracticeTrainers, earTrainerPosesAnswerableQuestions)
{
    juce::Random random (0x3a12);

    for (int e = 0; e < (int) EarTrainer::Exercise::numExercises; ++e)
    {
        for (int level = 0; level <= 4; ++level)
        {
            EarTrainer trainer;
            trainer.setExercise ((EarTrainer::Exercise) e);
            trainer.setAdaptive (false);
            trainer.setDifficulty (level);

            for (int trial = 0; trial < 20; ++trial)
            {
                int notes[EarTrainer::kMaxNotesInQuestion] {};
                double offsets[EarTrainer::kMaxNotesInQuestion] {};

                const int count = trainer.nextQuestion (random, notes, offsets,
                                                        EarTrainer::kMaxNotesInQuestion);

                CHECK_MSG (count >= 2, "an exercise produced only "
                                         + juce::String (count) + " notes");

                for (int i = 0; i < count; ++i)
                {
                    CHECK_MSG (notes[i] >= 0 && notes[i] <= 127,
                               "the exercise asked for MIDI note " + juce::String (notes[i]));

                    CHECK (offsets[i] >= 0.0);
                }

                const auto choices = trainer.getChoices();

                CHECK_MSG (choices.size() >= 2,
                           "an exercise offered " + juce::String (choices.size()) + " choices");

                CHECK_MSG (juce::isPositiveAndBelow (trainer.getCorrectChoice(), choices.size()),
                           "the correct choice is outside the list offered");

                CHECK (trainer.getQuestionText().isNotEmpty());

                // The right answer scores, and a wrong one does not.
                const int wrong = (trainer.getCorrectChoice() + 1) % choices.size();

                CHECK (! trainer.answer (wrong));
                CHECK (trainer.answer (trainer.getCorrectChoice()));
            }
        }
    }
}

//==============================================================================
/*  practice-tools 5: the difficulty follows the player. */
LUTHIER_TEST (PracticeTrainers, earTrainerDifficultyAdapts)
{
    juce::Random random (0x9d1);

    // A player who gets everything right should be moved up.
    {
        EarTrainer trainer;
        trainer.setExercise (EarTrainer::Exercise::interval);
        trainer.setAdaptive (true);
        trainer.setDifficulty (0);

        for (int i = 0; i < 60; ++i)
        {
            int notes[8] {};
            double offsets[8] {};

            trainer.nextQuestion (random, notes, offsets, 8);
            trainer.answer (trainer.getCorrectChoice());
        }

        CHECK_MSG (trainer.getDifficulty() > 0,
                   "a player answering everything correctly stayed at difficulty 0");
    }

    // And one who gets everything wrong should be moved down.
    {
        EarTrainer trainer;
        trainer.setExercise (EarTrainer::Exercise::interval);
        trainer.setAdaptive (true);
        trainer.setDifficulty (4);

        for (int i = 0; i < 60; ++i)
        {
            int notes[8] {};
            double offsets[8] {};

            trainer.nextQuestion (random, notes, offsets, 8);

            const auto choices = trainer.getChoices();
            trainer.answer ((trainer.getCorrectChoice() + 1) % juce::jmax (1, choices.size()));
        }

        CHECK_MSG (trainer.getDifficulty() < 4,
                   "a player answering everything wrongly stayed at difficulty 4");
    }

    // With adaptation off, the level does not move at all.
    {
        EarTrainer trainer;
        trainer.setAdaptive (false);
        trainer.setDifficulty (2);

        for (int i = 0; i < 60; ++i)
        {
            int notes[8] {};
            double offsets[8] {};

            trainer.nextQuestion (random, notes, offsets, 8);
            trainer.answer (trainer.getCorrectChoice());
        }

        CHECK (trainer.getDifficulty() == 2);
    }
}

//==============================================================================
/*  The stats file travels between sessions. */
LUTHIER_TEST (PracticeTrainers, earTrainerStatsRoundTrip)
{
    EarTrainer trainer;
    trainer.setDifficulty (3);
    trainer.setAdaptive (false);

    juce::Random random (0x77);

    for (int i = 0; i < 10; ++i)
    {
        int notes[8] {};
        double offsets[8] {};

        trainer.nextQuestion (random, notes, offsets, 8);
        trainer.answer (trainer.getCorrectChoice());
    }

    const auto text = juce::JSON::toString (trainer.statsToVar(), false);

    EarTrainer restored;
    restored.statsFromVar (juce::JSON::parse (text));

    CHECK (restored.getDifficulty() == 3);
    CHECK (! restored.isAdaptive());
}

//==============================================================================
/*  practice-tools 7: a progression is entered the way a musician writes one. */
LUTHIER_TEST (PracticeTrainers, progressionParsing)
{
    ProgressionLooper looper;

    CHECK (looper.parse ("Am - F - C - G x4"));

    CHECK (looper.getNumChords() == 4);
    CHECK (looper.getRepeats() == 4);

    CHECK (looper.getChord (0).symbol == "Am");
    CHECK (looper.getChord (0).rootPitchClass == 9);      // A
    CHECK (looper.getChord (1).rootPitchClass == 5);      // F
    CHECK (looper.getChord (2).rootPitchClass == 0);      // C
    CHECK (looper.getChord (3).rootPitchClass == 7);      // G

    // Am is minor: a minor third.
    CHECK (looper.getChord (0).numIntervals == 3);
    CHECK (looper.getChord (0).intervals[1] == 3);

    // F is major: a major third.
    CHECK (looper.getChord (1).intervals[1] == 4);

    // ---- qualities ----------------------------------------------------------------
    CHECK (looper.parse ("Cmaj7 - Dm7 - G7 - Cmaj7"));
    CHECK (looper.getNumChords() == 4);
    CHECK (looper.getRepeats() == 1);

    CHECK (looper.getChord (0).numIntervals == 4);
    CHECK (looper.getChord (0).intervals[3] == 11);       // major seventh
    CHECK (looper.getChord (2).intervals[3] == 10);       // dominant seventh

    // ---- accidentals and slash chords ------------------------------------------------
    CHECK (looper.parse ("Bb - F/A - Gm - Eb"));
    CHECK (looper.getChord (0).rootPitchClass == 10);     // Bb
    CHECK (looper.getChord (1).rootPitchClass == 5);      // F
    CHECK (looper.getChord (1).bassPitchClass == 9);      // over A

    // ---- the beat map ---------------------------------------------------------------
    CHECK (looper.parse ("C - G - Am - F"));

    CHECK (looper.getChordAtBeat (0.0) == 0);
    CHECK (looper.getChordAtBeat (4.0) == 1);
    CHECK (looper.getChordAtBeat (9.0) == 2);
    CHECK (looper.getChordAtBeat (13.0) == 3);

    // It wraps.
    CHECK (looper.getChordAtBeat (16.0) == 0);
    CHECK (looper.getChordAtBeat (20.0) == 1);

    // ---- notes ------------------------------------------------------------------------
    int notes[8] {};
    const int count = looper.getMidiNotes (0, notes, 8);

    CHECK (count == 3);

    for (int i = 0; i < count; ++i)
        CHECK (notes[i] >= 0 && notes[i] <= 127);

    // C major at octave 3: C3, E3, G3.
    CHECK (notes[0] % 12 == 0);
    CHECK (notes[1] % 12 == 4);
    CHECK (notes[2] % 12 == 7);

    // ---- nonsense is refused rather than half-parsed ------------------------------------
    ProgressionLooper fresh;
    CHECK (! fresh.parse (""));
    CHECK (! fresh.parse ("hello - there"));

    // And a failed parse leaves the previous progression in place.
    CHECK (looper.parse ("C - G"));
    CHECK (! looper.parse ("!!!"));
    CHECK (looper.getNumChords() == 2);
}
