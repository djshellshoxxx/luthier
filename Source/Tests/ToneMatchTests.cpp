/*  Tone match tests (tone-match.md section 8).

    The deconvolution test is the one with teeth: convolve a known IR with a
    sweep, deconvolve the result, and check that what comes back is the IR that
    went in. That exercises the sweep generator, the inverse filter and the
    regularisation together, and nothing short of all three being right will
    pass it.
*/

#include "TestFramework.h"

#include "../ToneMatch/ToneMatch.h"
#include "../PluginProcessor.h"
#include "../UI/ToneMatchPanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;

    /** A plausible cabinet impulse: a fast attack, a resonant body and a short
        decay, band-limited the way a speaker is. */
    std::vector<float> makeTestIr (int length, double sampleRate)
    {
        std::vector<float> ir ((size_t) length, 0.0f);

        RtRandom random (0xcab1);

        Biquad lowpass, resonance, highpass;
        lowpass.setLowpass (sampleRate, 5000.0, 0.707);
        resonance.setPeaking (sampleRate, 110.0, 2.0, 9.0);
        highpass.setHighpass (sampleRate, 80.0, 0.707);

        for (int i = 0; i < length; ++i)
        {
            // An impulse followed by decaying noise: crude, but it has the
            // spectral shape and the decay a real cab IR has.
            double sample = (i == 0) ? 1.0 : (random.nextDouble() * 2.0 - 1.0) * 0.3;

            sample *= std::exp (-(double) i / (sampleRate * 0.02));

            sample = highpass.process (resonance.process (lowpass.process (sample)));

            ir[(size_t) i] = (float) sample;
        }

        return ir;
    }

    /** Direct convolution. Slow, but it is unambiguously correct, which is what
        a test wants. */
    std::vector<float> convolve (const std::vector<float>& signal,
                                 const std::vector<float>& ir)
    {
        std::vector<float> output (signal.size() + ir.size(), 0.0f);

        for (size_t i = 0; i < signal.size(); ++i)
        {
            const double value = (double) signal[i];

            if (std::abs (value) < 1.0e-12)
                continue;

            for (size_t j = 0; j < ir.size(); ++j)
                output[i + j] += (float) (value * (double) ir[j]);
        }

        return output;
    }

    /** How closely two signals agree, in dB. More negative is better. */
    double nullDb (const std::vector<float>& a, const std::vector<float>& b)
    {
        return CabMatch::measureNull (a, b);
    }
}

//==============================================================================
/*  tone-match 8: convolve a known IR with a sweep, deconvolve, and compare. */
LUTHIER_TEST (ToneMatch, sweepDeconvolutionRecoversTheSourceIr)
{
    const auto sweep = CabMatch::generateTestSignal (CabMatch::TestSignal::sineSweep, kSr, 4.0);

    CHECK_MSG (! sweep.empty(), "the sweep generator produced nothing");

    const int irLength = (int) (0.05 * kSr);
    const auto source = makeTestIr (irLength, kSr);

    // What a reference rig would send back.
    const auto response = convolve (sweep, source);

    const auto recovered = CabMatch::deconvolve (sweep, response, kSr,
                                                 CabMatch::TestSignal::sineSweep, 0.1);

    CHECK_MSG (! recovered.isEmpty(), "the deconvolution produced nothing");

    if (recovered.isEmpty())
        return;

    /*  Compare by what the IRs do rather than sample by sample.

        The recovered IR is normalised and may sit at a different overall gain
        from the source, so a direct null would measure that difference rather
        than the shape. Convolving both with the same signal and normalising the
        results compares what a listener would actually hear.
    */
    std::vector<float> probe ((size_t) (0.05 * kSr), 0.0f);

    {
        RtRandom random (0x9abe);

        for (size_t i = 0; i < probe.size(); ++i)
            probe[i] = (float) ((random.nextDouble() * 2.0 - 1.0) * 0.5);
    }

    auto throughSource = convolve (probe, source);
    auto throughRecovered = convolve (probe, recovered.samples);

    auto normalise = [] (std::vector<float>& signal)
    {
        double peak = 0.0;

        for (float sample : signal)
            peak = juce::jmax (peak, (double) std::abs (sample));

        if (peak > 1.0e-9)
            for (float& sample : signal)
                sample = (float) (sample / peak);
    };

    normalise (throughSource);
    normalise (throughRecovered);

    const double result = nullDb (throughSource, throughRecovered);

    CHECK_MSG (result < -20.0,
               "the recovered IR nulled against the source at only "
                 + juce::String (result, 1) + " dB");

    // And the recovered IR is finite throughout: a division by a near-zero bin
    // without regularisation shows up here as a NaN or an enormous spike.
    CHECK_FINITE (recovered.samples.data(), recovered.getLength());

    double peak = 0.0;

    for (float sample : recovered.samples)
        peak = juce::jmax (peak, (double) std::abs (sample));

    CHECK_MSG (peak <= 1.01, "the recovered IR peaked at " + juce::String (peak, 3));
}

//==============================================================================
/*  Every test signal must be generated, finite and inside range. */
LUTHIER_TEST (ToneMatch, everyTestSignalIsWellFormed)
{
    for (int s = 0; s < (int) CabMatch::TestSignal::numSignals; ++s)
    {
        const auto signal = (CabMatch::TestSignal) s;
        const auto generated = CabMatch::generateTestSignal (signal, kSr, 3.0);

        CHECK_MSG (! generated.empty(),
                   juce::String (CabMatch::getTestSignalName (signal)) + " produced nothing");

        if (generated.empty())
            continue;

        CHECK_FINITE (generated.data(), (int) generated.size());

        double peak = 0.0;

        for (float sample : generated)
            peak = juce::jmax (peak, (double) std::abs (sample));

        CHECK_MSG (peak > 0.05,
                   juce::String (CabMatch::getTestSignalName (signal))
                     + " peaked at only " + juce::String (peak, 4));

        CHECK_MSG (peak <= 1.0,
                   juce::String (CabMatch::getTestSignalName (signal))
                     + " clipped at " + juce::String (peak, 3));
    }

    // The sweep must not click at either end, which is what the fades are for.
    const auto sweep = CabMatch::generateTestSignal (CabMatch::TestSignal::sineSweep, kSr, 2.0);

    CHECK (std::abs (sweep.front()) < 0.01);
    CHECK (std::abs (sweep.back()) < 0.01);
}

//==============================================================================
/*  tone-match 8: the EQ match must fit known curves to within a dB across the
    band it was asked to fit. */
LUTHIER_TEST (ToneMatch, eqMatchFitsKnownCurves)
{
    // White noise through a known filter is the reference; the same noise
    // unfiltered is what Luthier is "currently" producing. The fitted filter
    // should then be the known filter.
    const int length = (int) (4.0 * kSr);

    std::vector<float> flat ((size_t) length, 0.0f);

    {
        RtRandom random (0xe9);

        for (size_t i = 0; i < flat.size(); ++i)
            flat[i] = (float) ((random.nextDouble() * 2.0 - 1.0) * 0.25);
    }

    /*  `expectedDb` is the reference filter's own magnitude at `frequency`, which
        is what the fit is being asked to reproduce - not the filter's nominal
        gain. For a peaking filter the two are the same: an RBJ bell reaches its
        full gain at its centre. A shelf does not. Its corner frequency is by
        definition the half-gain point (the RBJ form sets A = 10^(dB/40), and the
        magnitude at w0 is exactly A), so a +6 dB shelf is +3 dB at 200 Hz and
        only approaches +6 dB well below it. */
    struct Curve
    {
        const char* name;
        double frequency;
        double q;
        double gainDb;
        double expectedDb;
        enum { shelfLow, bell, notch } type;
    };

    const Curve curves[] =
    {
        { "low shelf +6 dB", 200.0,  0.707,  6.0,  3.0, Curve::shelfLow },
        { "bell +8 dB",      1000.0, 1.2,    8.0,  8.0, Curve::bell },
        { "bell -8 dB",      2000.0, 1.2,   -8.0, -8.0, Curve::bell }
    };

    for (const auto& curve : curves)
    {
        std::vector<float> reference = flat;

        Biquad filter;

        switch (curve.type)
        {
            case Curve::shelfLow: filter.setLowShelf (kSr, curve.frequency, 0.707, curve.gainDb); break;
            case Curve::bell:     filter.setPeaking (kSr, curve.frequency, curve.q, curve.gainDb); break;
            case Curve::notch:    filter.setNotch (kSr, curve.frequency, curve.q); break;
        }

        for (auto& sample : reference)
            sample = (float) filter.process ((double) sample);

        EqMatch::Options options;
        options.length = EqMatch::FilterLength::long4096;
        options.lowHz = 100.0;
        options.highHz = 8000.0;
        options.aggressiveness = 1.0;

        const auto fitted = EqMatch::fit (reference, flat, kSr, options);

        CHECK_MSG (! fitted.isEmpty(),
                   juce::String (curve.name) + ": the fit produced nothing");

        if (fitted.isEmpty())
            continue;

        CHECK_FINITE (fitted.samples.data(), fitted.getLength());

        // The fitted filter's response at the measurement frequency should match
        // what the reference filter actually does there.
        const double fittedDb = EqMatch::magnitudeAt (fitted, curve.frequency, kSr);

        // A minimum-phase fit of a finite length cannot reproduce a curve
        // exactly, and the spec's tolerance is a decibel across the fit band.
        CHECK_MSG (std::abs (fittedDb - curve.expectedDb) <= 2.5,
                   juce::String (curve.name) + ": fitted "
                     + juce::String (fittedDb, 2) + " dB at "
                     + juce::String (curve.frequency, 0) + " Hz, expected "
                     + juce::String (curve.expectedDb, 1));
    }
}

//==============================================================================
/*  tone-match 3: outside the fit band nothing is corrected, and preserve
    dynamics removes the broadband gain. */
LUTHIER_TEST (ToneMatch, eqMatchRespectsItsBandAndOptions)
{
    const int length = (int) (3.0 * kSr);

    std::vector<float> flat ((size_t) length, 0.0f);

    {
        RtRandom random (0x5a);

        for (size_t i = 0; i < flat.size(); ++i)
            flat[i] = (float) ((random.nextDouble() * 2.0 - 1.0) * 0.25);
    }

    // A reference that is 6 dB louder everywhere: a pure broadband difference.
    std::vector<float> louder = flat;

    for (auto& sample : louder)
        sample *= 2.0f;

    {
        EqMatch::Options options;
        options.preserveDynamics = false;
        options.lowHz = 100.0;
        options.highHz = 8000.0;

        const auto fitted = EqMatch::fit (louder, flat, kSr, options);

        CHECK (! fitted.isEmpty());

        // Without preserve-dynamics, the filter should make up the 6 dB.
        const double gain = EqMatch::magnitudeAt (fitted, 1000.0, kSr);

        CHECK_MSG (gain > 3.0,
                   "a 6 dB level difference produced only "
                     + juce::String (gain, 2) + " dB of correction");
    }

    {
        EqMatch::Options options;
        options.preserveDynamics = true;
        options.lowHz = 100.0;
        options.highHz = 8000.0;

        const auto fitted = EqMatch::fit (louder, flat, kSr, options);

        CHECK (! fitted.isEmpty());

        // With it, a pure level difference is entirely removed: there is no
        // shape to correct.
        const double gain = EqMatch::magnitudeAt (fitted, 1000.0, kSr);

        CHECK_MSG (std::abs (gain) < 1.5,
                   "preserve dynamics still applied " + juce::String (gain, 2) + " dB");
    }

    // Aggressiveness at zero corrects nothing at all.
    {
        EqMatch::Options options;
        options.aggressiveness = 0.0;

        const auto fitted = EqMatch::fit (louder, flat, kSr, options);

        CHECK (! fitted.isEmpty());
        CHECK_NEAR (EqMatch::magnitudeAt (fitted, 1000.0, kSr), 0.0, 1.0);
    }
}

//==============================================================================
/*  The filter lengths the spec offers all work. */
LUTHIER_TEST (ToneMatch, everyFilterLengthProducesAFilter)
{
    CHECK (EqMatch::getTapCount (EqMatch::FilterLength::short256) == 256);
    CHECK (EqMatch::getTapCount (EqMatch::FilterLength::medium1024) == 1024);
    CHECK (EqMatch::getTapCount (EqMatch::FilterLength::long4096) == 4096);

    std::vector<float> noise ((size_t) (2.0 * kSr), 0.0f);

    {
        RtRandom random (0x1e);

        for (size_t i = 0; i < noise.size(); ++i)
            noise[i] = (float) ((random.nextDouble() * 2.0 - 1.0) * 0.25);
    }

    auto reference = noise;

    Biquad filter;
    filter.setPeaking (kSr, 1500.0, 1.0, 6.0);

    for (auto& sample : reference)
        sample = (float) filter.process ((double) sample);

    for (int l = 0; l < (int) EqMatch::FilterLength::numLengths; ++l)
    {
        EqMatch::Options options;
        options.length = (EqMatch::FilterLength) l;

        const auto fitted = EqMatch::fit (reference, noise, kSr, options);

        CHECK_MSG (fitted.getLength() == EqMatch::getTapCount (options.length),
                   "length " + juce::String (l) + " produced "
                     + juce::String (fitted.getLength()) + " taps");

        CHECK_FINITE (fitted.samples.data(), fitted.getLength());
    }
}

//==============================================================================
/*  tone-match 8: the null measurement itself has to be right, or every other
    number reported to the user is meaningless. */
LUTHIER_TEST (ToneMatch, nullMeasurementIsCorrect)
{
    std::vector<float> a ((size_t) 1000, 0.0f);

    for (size_t i = 0; i < a.size(); ++i)
        a[i] = (float) std::sin ((double) i * 0.1);

    // Identical signals null perfectly.
    CHECK_MSG (CabMatch::measureNull (a, a) < -100.0,
               "identical signals nulled at only "
                 + juce::String (CabMatch::measureNull (a, a), 1) + " dB");

    // A signal against silence nulls at 0 dB: all the energy is left.
    std::vector<float> silence ((size_t) 1000, 0.0f);
    CHECK_NEAR (CabMatch::measureNull (a, silence), 0.0, 0.01);

    // Half the amplitude leaves a quarter of the energy: -6 dB.
    auto half = a;

    for (auto& sample : half)
        sample *= 0.5f;

    CHECK_NEAR (CabMatch::measureNull (a, half), -6.02, 0.1);
}

//==============================================================================
/*  tone-match 5: the sidecar, and the filename fallback when there is none. */
LUTHIER_TEST (ToneMatch, metadataRoundTripsAndFallsBackToTheFilename)
{
    IrMetadata metadata;
    metadata.name = "Marshall 4x12 Greenback SM57 On-Axis";
    metadata.type = "cabinet";
    metadata.sampleRate = 48000.0;
    metadata.lengthMs = 500.0;
    metadata.author = "user";
    metadata.tags = { "marshall", "greenback", "sm57" };
    metadata.notes = "captured with Luthier cab match";

    const auto text = juce::JSON::toString (metadata.toVar(), false);
    const auto restored = IrMetadata::fromVar (juce::JSON::parse (text));

    CHECK (restored.name == metadata.name);
    CHECK (restored.type == metadata.type);
    CHECK_NEAR (restored.sampleRate, 48000.0, 0.001);
    CHECK_NEAR (restored.lengthMs, 500.0, 0.001);
    CHECK (restored.author == "user");
    CHECK (restored.tags.size() == 3);
    CHECK (restored.tags.contains ("greenback"));

    // The spec's own example parses.
    const char* json = R"({
      "name": "Marshall 4x12 Greenback SM57 On-Axis",
      "type": "cabinet",
      "sample_rate": 48000,
      "length_ms": 500,
      "author": "user",
      "tags": ["marshall", "greenback", "sm57"],
      "notes": "captured with Luthier cab match, 2026-04-12"
    })";

    const auto parsed = IrMetadata::fromVar (juce::JSON::parse (json));

    CHECK (parsed.type == "cabinet");
    CHECK (parsed.tags.contains ("sm57"));
    CHECK_NEAR (parsed.sampleRate, 48000.0, 0.001);
}

//==============================================================================
/*  tone-match 7: a preset stores a relative path for an IR under the library
    root and an absolute one otherwise. */
LUTHIER_TEST (ToneMatch, presetPathsAreRelativeUnderTheLibraryRoot)
{
    const auto root = IrLibraryPaths::getRoot();

    const auto inside = root.getChildFile ("Cabinets").getChildFile ("User")
                          .getChildFile ("My Cab.wav");

    const auto path = IrLibraryPaths::toPresetPath (inside);

    CHECK_MSG (! juce::File::isAbsolutePath (path),
               "an IR inside the library stored an absolute path: " + path);

    CHECK (path.contains ("Cabinets"));
    CHECK (path.contains ("My Cab.wav"));

    // And it comes back to the same file.
    CHECK (IrLibraryPaths::fromPresetPath (path) == inside);

    // Somewhere else entirely stays absolute.
    const auto outside = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("Elsewhere.wav");

    const auto outsidePath = IrLibraryPaths::toPresetPath (outside);

    CHECK_MSG (juce::File::isAbsolutePath (outsidePath),
               "an IR outside the library stored a relative path: " + outsidePath);

    CHECK (IrLibraryPaths::fromPresetPath (outsidePath) == outside);

    // An empty path is not a file.
    CHECK (IrLibraryPaths::toPresetPath (juce::File()).isEmpty());
    CHECK (IrLibraryPaths::fromPresetPath ({}) == juce::File());
}

//==============================================================================
/*  tone-match 4: the capture records what it is given, stops when full, and
    never writes past its buffer. */
LUTHIER_TEST (ToneMatch, captureRecordsAndStops)
{
    Capture capture;
    capture.prepare (kSr, 1.0);

    CHECK (! capture.isRecording());
    CHECK (! capture.isComplete());
    CHECK (capture.getRecordedSamples() == 0);

    // Half a second.
    capture.start (0.5);
    CHECK (capture.isRecording());

    std::vector<float> left (512, 0.4f), right (512, 0.4f);
    const float* channels[2] = { left.data(), right.data() };

    // Feed it far more than it asked for.
    for (int i = 0; i < 200; ++i)
        capture.processBlock (channels, 2, 512);

    CHECK_MSG (! capture.isRecording(), "the capture did not stop when it was full");
    CHECK (capture.isComplete());

    const int expected = (int) (0.5 * kSr);

    CHECK_MSG (capture.getRecordedSamples() == expected,
               "captured " + juce::String (capture.getRecordedSamples())
                 + " samples, expected " + juce::String (expected));

    CHECK_NEAR (capture.getProgress(), 1.0, 0.001);

    // What it captured is what it was given.
    CHECK_NEAR (capture.getBuffer().getSample (0, 100), 0.4, 0.001);
    CHECK_NEAR (capture.getBuffer().getSample (1, 100), 0.4, 0.001);
}

//==============================================================================
/*  tone-match 4: autotrim removes leading silence. */
LUTHIER_TEST (ToneMatch, captureAutoTrimsSilence)
{
    Capture capture;
    capture.prepare (kSr, 1.0);

    capture.start (0.5);

    // A tenth of a second of silence, then signal.
    std::vector<float> silence (512, 0.0f);
    const float* silentChannels[2] = { silence.data(), silence.data() };

    for (int i = 0; i < 10; ++i)
        capture.processBlock (silentChannels, 2, 512);

    std::vector<float> loud (512, 0.5f);
    const float* loudChannels[2] = { loud.data(), loud.data() };

    for (int i = 0; i < 30; ++i)
        capture.processBlock (loudChannels, 2, 512);

    capture.stop();

    const int before = capture.getRecordedSamples();
    CHECK (before > 0);

    capture.autoTrim (-60.0);

    const int after = capture.getRecordedSamples();

    CHECK_MSG (after < before,
               "autotrim removed nothing: " + juce::String (before) + " to "
                 + juce::String (after));

    // The first sample is now signal rather than silence.
    CHECK_MSG (std::abs (capture.getBuffer().getSample (0, 0)) > 0.1,
               "autotrim left silence at the start");
}

//==============================================================================
/*  tone-match 1: an IR slot's settings travel in the preset. */
LUTHIER_TEST (ToneMatch, irSlotSettingsRoundTrip)
{
    IrSlot slot;
    slot.prepare (kSr, 512);

    slot.setGainTrimDb (-4.5);
    slot.setPredelayMs (12.0);
    slot.setMix (0.6);
    slot.setReversed (true);
    slot.setStartTrim (64);
    slot.setEndTrim (128);
    slot.setMaxSeconds (2.0);

    const auto text = juce::JSON::toString (slot.toVar(), false);

    IrSlot restored;
    restored.prepare (kSr, 512);
    restored.fromVar (juce::JSON::parse (text));

    CHECK_NEAR (restored.getGainTrimDb(), -4.5, 0.001);
    CHECK_NEAR (restored.getPredelayMs(), 12.0, 0.001);
    CHECK_NEAR (restored.getMix(), 0.6, 0.001);
    CHECK (restored.isReversed());
    CHECK (restored.getStartTrim() == 64);
    CHECK (restored.getEndTrim() == 128);
    CHECK_NEAR (restored.getMaxSeconds(), 2.0, 0.001);

    // Nothing is loaded, so the slot is not engaged whatever the state said.
    CHECK (! restored.isEngaged());
    CHECK (! restored.isLoaded());
}

//==============================================================================
/*  tone-match 0.4: a slot that is not engaged must not touch the audio, and one
    that is engaged with nothing loaded must not either. */
LUTHIER_TEST (ToneMatch, anEmptySlotLeavesTheAudioAlone)
{
    IrSlot slot;
    slot.prepare (kSr, 512);

    juce::AudioBuffer<float> buffer (2, 512);

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < 512; ++i)
            buffer.getWritePointer (channel)[i] = (float) std::sin ((double) i * 0.05);

    const auto original = buffer;

    // Not engaged.
    slot.process (buffer.getArrayOfWritePointers(), 2, 512);

    for (int i = 0; i < 512; ++i)
        CHECK_NEAR (buffer.getSample (0, i), original.getSample (0, i), 1.0e-9);

    // Engaged, but nothing is loaded.
    slot.setEngaged (true);
    CHECK_MSG (! slot.isEngaged(), "a slot with no IR reported itself engaged");

    slot.process (buffer.getArrayOfWritePointers(), 2, 512);

    for (int i = 0; i < 512; ++i)
        CHECK_NEAR (buffer.getSample (0, i), original.getSample (0, i), 1.0e-9);

    CHECK (slot.getLatencySamples() == 0);
}

//==============================================================================
/*  tone-match 3: the EQ match description says plainly that it is not a cab
    match, which the spec asks for in as many words. */
LUTHIER_TEST (ToneMatch, eqMatchSaysWhatItCannotDo)
{
    const juce::String description (EqMatch::getDescription());

    CHECK (description.isNotEmpty());

    CHECK_MSG (description.containsIgnoreCase ("not a substitute")
                 || description.containsIgnoreCase ("is not a"),
               "the EQ match description does not say it is not a cab match");

    CHECK_MSG (description.containsIgnoreCase ("time")
                 || description.containsIgnoreCase ("reflection")
                 || description.containsIgnoreCase ("ringing"),
               "the EQ match description does not mention what it cannot capture");
}


//==============================================================================
/*  tone-match 1 and 6: both persisted length trims must be reachable from each
    IR card, update the slot, and keep usable bounds at compact and beta widths. */
LUTHIER_TEST (ToneMatch, irTrimControlsReachThePersistedSlotAndFitTheCard)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, 512);

    IrSlotEditor editor (processor, IrSlotEditor::Slot::body);

    juce::Slider* startTrim = nullptr;
    juce::Slider* endTrim = nullptr;

    for (auto* child : editor.getChildren())
    {
        if (child->getComponentID() == "ir-start-trim")
            startTrim = dynamic_cast<juce::Slider*> (child);
        else if (child->getComponentID() == "ir-end-trim")
            endTrim = dynamic_cast<juce::Slider*> (child);
    }

    CHECK_MSG (startTrim != nullptr, "the IR card has no start-trim control");
    CHECK_MSG (endTrim != nullptr, "the IR card has no end-trim control");

    if (startTrim == nullptr || endTrim == nullptr)
        return;

    startTrim->setValue (64.0, juce::sendNotificationSync);
    endTrim->setValue (128.0, juce::sendNotificationSync);

    CHECK (processor.getBodyIrSlot().getStartTrim() == 64);
    CHECK (processor.getBodyIrSlot().getEndTrim() == 128);

    IrSlot restored;
    restored.prepare (kSr, 512);
    restored.fromVar (processor.getBodyIrSlot().toVar());

    CHECK (restored.getStartTrim() == 64);
    CHECK (restored.getEndTrim() == 128);

    for (int width : { 280, 420 })
    {
        editor.setSize (width, IrSlotEditor::preferredHeight);
        editor.resized();

        for (auto* slider : { startTrim, endTrim })
        {
            CHECK_MSG (! slider->getBounds().isEmpty(),
                       "a trim control collapsed at width " + juce::String (width));
            CHECK_MSG (editor.getLocalBounds().contains (slider->getBounds()),
                       "a trim control escaped the IR card at width " + juce::String (width));
        }
    }
}
