/*  String scraping (string-scraping.md 6), and the pick-noise.md 5 rake's
    trigger.

    The ScrapeEngine's own tests measure its output directly: the catch train
    (what Aux 8 carries) for rate, timing and per-catch amplitude, and the
    per-string excitation for what the strings are handed. The engine tests
    then check what only the whole instrument can get wrong: the catches
    reaching the strings and the noise bus, the keyswitch never playing a
    note, the pick's load on the pitch, and the SysEx record.

    The ScrapeEngine tests need nothing but ScrapeEngine.cpp. The Engine tests
    need the LuthierEngine edits that wire it in (getScrapeEngine and the
    processSubBlock hooks).
*/

#include "TestFramework.h"

#include "../DSP/Noise/ScrapeEngine.h"
#include "../LuthierEngine.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;
    constexpr double kScale = 648.0;
    constexpr double kWindings = 3.1;   // wraps per mm; off the 1/60 mm grid the tests move on
    constexpr std::array<double, 6> kOpenHz { 329.63, 246.94, 196.0, 146.83, 110.0, 82.41 };

    StringNoiseInfo wound (double windingsPerMm = kWindings)
    {
        StringNoiseInfo info;
        info.wound = true;
        info.windingPitchPerMm = windingsPerMm;
        info.windingDepth = 0.9;
        return info;
    }

    StringNoiseInfo plain()
    {
        return {};
    }

    /** A six-string whose low three are wound, as on most electric sets. */
    std::unique_ptr<ScrapeEngine> makeScrape()
    {
        auto scrape = std::make_unique<ScrapeEngine>();
        scrape->prepare (kSr, kBlock);
        scrape->setNumStrings (6);
        scrape->setScaleLengthMm (kScale);

        for (int s = 0; s < 6; ++s)
            scrape->setString (s, s >= 3 ? wound() : plain(), kOpenHz[(size_t) s], 0.0, 0.0);

        return scrape;
    }

    ScrapeGesture gestureOn (int stringIndex, double pressure = 0.5,
                             double fromMm = 200.0, double toMm = 600.0, double ms = 500.0)
    {
        ScrapeGesture g;
        g.stringIndex = stringIndex;
        g.startPositionMm = fromMm;
        g.endPositionMm = toMm;
        g.durationMs = ms;
        g.pressure = pressure;
        return g;
    }

    struct Capture
    {
        std::vector<double> noise;
        std::array<std::vector<double>, 6> excitation;
    };

    /** Renders `blocks` blocks, recording zeros for blocks with no output. */
    void render (ScrapeEngine& scrape, Capture& c, int blocks)
    {
        for (int b = 0; b < blocks; ++b)
        {
            scrape.processBlock (kBlock);
            const bool out = scrape.hasOutput();

            for (int i = 0; i < kBlock; ++i)
            {
                c.noise.push_back (out ? scrape.getNoiseOutput()[i] : 0.0);

                for (int s = 0; s < 6; ++s)
                    c.excitation[(size_t) s].push_back (out ? scrape.getExcitation (s)[i] : 0.0);
            }
        }
    }

    /** The first sample of every pulse above `threshold`. */
    std::vector<int> onsets (const std::vector<double>& x, double threshold)
    {
        std::vector<int> at;
        bool above = false;

        for (int i = 0; i < (int) x.size(); ++i)
        {
            const bool now = x[(size_t) i] > threshold;

            if (now && ! above)
                at.push_back (i);

            above = now;
        }

        return at;
    }

    /** Each pulse's peak: the largest sample in the few after its onset. */
    std::vector<double> pulsePeaks (const std::vector<double>& x, const std::vector<int>& at)
    {
        std::vector<double> peaks;

        for (int i : at)
        {
            double p = 0.0;

            for (int k = i; k < juce::jmin ((int) x.size(), i + 6); ++k)
                p = juce::jmax (p, x[(size_t) k]);

            peaks.push_back (p);
        }

        return peaks;
    }

    double maxAbs (const std::vector<double>& x)
    {
        double m = 0.0;

        for (double v : x)
            m = juce::jmax (m, std::abs (v));

        return m;
    }

    double rmsOf (const std::vector<double>& x)
    {
        return x.empty() ? 0.0 : rms (x.data(), (int) x.size());
    }

    /** Power-weighted mean frequency of a signal's spectrum, Hann-windowed. */
    double spectralCentroid (const std::vector<double>& x, double sampleRate)
    {
        constexpr int order = 14;
        constexpr int size = 1 << order;

        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) size * 2, 0.0f);

        const int n = juce::jmin (size, (int) x.size());

        for (int i = 0; i < n; ++i)
        {
            const double w = 0.5 * (1.0 - std::cos (constants::kTwoPi * i / (n - 1)));
            data[(size_t) i] = (float) (x[(size_t) i] * w);
        }

        fft.performFrequencyOnlyForwardTransform (data.data());

        double weighted = 0.0, total = 0.0;

        for (int k = 1; k < size / 2; ++k)
        {
            const double power = (double) data[(size_t) k] * (double) data[(size_t) k];
            weighted += power * k * sampleRate / size;
            total += power;
        }

        return total > 0.0 ? weighted / total : 0.0;
    }

    juce::MidiBuffer keyswitch (int note, int offset, int channel = 1)
    {
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) 100), offset);
        return m;
    }

    /** A Stratocaster with the rig after the body left alone: the tests read pre-body. */
    std::unique_ptr<LuthierEngine> makeEngine()
    {
        auto engine = std::make_unique<LuthierEngine>();
        engine->prepare (kSr, kBlock);
        engine->setGuitarType (GuitarType::Stratocaster);
        return engine;
    }

    /** Renders blocks through the engine, keeping the pre-body signal (the strings' sum). */
    std::vector<double> renderEngine (LuthierEngine& engine, int blocks,
                                      const juce::MidiBuffer& firstBlockMidi = {})
    {
        juce::AudioBuffer<float> block (2, kBlock);
        std::vector<double> preBody;

        for (int b = 0; b < blocks; ++b)
        {
            block.clear();
            juce::MidiBuffer midi;

            if (b == 0)
                midi = firstBlockMidi;

            engine.processBlock (block, midi);

            const double* sum = engine.getPreBodyBuffer();

            for (int i = 0; i < kBlock; ++i)
                preBody.push_back (sum[i]);
        }

        return preBody;
    }
}

//==============================================================================
//  string-scraping.md 6, on the ScrapeEngine's own output
//==============================================================================
LUTHIER_TEST (Scrape, catchesComeAtWindingsPerMmTimesSpeed)
{
    // Wound low E, 500 ms, pressure 0.5: 400 mm at 800 mm/s over 3.1 wraps/mm
    // is 1240 catches at 2480 a second.
    auto scrape = makeScrape();
    scrape->trigger (gestureOn (5));

    Capture c;
    render (*scrape, c, (int) std::ceil (0.6 * kSr / kBlock));

    const auto expected = (juce::int64) std::floor (600.0 * kWindings) - (juce::int64) std::floor (200.0 * kWindings);
    CHECK_MSG (std::abs (scrape->getCatchCount (5) - expected) <= 1,
               "crossed " + juce::String (scrape->getCatchCount (5)) + " windings, expected " + juce::String (expected));

    // Measured from the audio: discrete impulses, one per catch, at the rate.
    const auto at = onsets (c.noise, 0.3 * maxAbs (c.noise));

    CHECK_MSG (std::abs ((juce::int64) at.size() - expected) <= 2,
               juce::String ((int) at.size()) + " impulses for " + juce::String (expected) + " catches");

    CHECK (at.size() > 2);

    if (at.size() > 2)
    {
        const double seconds = (at.back() - at.front()) / kSr;
        const double rate = (double) (at.size() - 1) / seconds;
        const double expectedRate = kWindings * (400.0 / 0.5);

        CHECK_MSG (std::abs (rate / expectedRate - 1.0) < 0.02,
                   "catch rate " + juce::String (rate, 1) + " /s, expected " + juce::String (expectedRate, 1));

        // Discrete: between catches the train is back at zero, not a noise bed.
        int silent = 0;

        for (int i = at.front(); i < at.back(); ++i)
            if (c.noise[(size_t) i] == 0.0)
                ++silent;

        CHECK_MSG (silent > (at.back() - at.front()) / 2, "the catch train is not a stream of separate impulses");
    }
}

LUTHIER_TEST (Scrape, aPlainStringIsNearSilent)
{
    // 0.2: the same gesture on the plain high E has no winding to catch.
    auto woundRun = makeScrape(), plainRun = makeScrape();
    woundRun->trigger (gestureOn (5));
    plainRun->trigger (gestureOn (0));

    Capture w, p;
    render (*woundRun, w, 100);
    render (*plainRun, p, 100);

    const double below = gainToDb (juce::jmax (1.0e-12, rmsOf (p.excitation[0])) / rmsOf (w.excitation[5]));

    CHECK (rmsOf (w.excitation[5]) > 0.0);
    CHECK_MSG (below < -30.0, "plain string only " + juce::String (-below, 1) + " dB under the wound one");
    CHECK (plainRun->getCatchCount (0) == 0);
    CHECK (plainRun->getNumRecords() == 0);
}

LUTHIER_TEST (Scrape, doublingPressureDoublesEachCatch)
{
    auto measure = [] (double pressure, double& cents)
    {
        auto scrape = makeScrape();
        scrape->trigger (gestureOn (5, pressure));

        Capture c;
        render (*scrape, c, 20);
        cents = scrape->getPitchOffsetCents (5);
        render (*scrape, c, 100);

        return pulsePeaks (c.noise, onsets (c.noise, 0.3 * maxAbs (c.noise)));
    };

    double lightCents = 0.0, hardCents = 0.0;
    const auto light = measure (0.4, lightCents), hard = measure (0.8, hardCents);

    CHECK_MSG (light.size() == hard.size() && ! light.empty(),
               "pressure changed the number of catches: " + juce::String ((int) light.size())
                 + " vs " + juce::String ((int) hard.size()));

    double ratioSum = 0.0;
    int within = 0;
    const size_t n = juce::jmin (light.size(), hard.size());

    for (size_t k = 0; k < n; ++k)
    {
        const double r = hard[k] / light[k];
        ratioSum += r;

        if (std::abs (r - 2.0) <= 0.4)
            ++within;
    }

    CHECK_MSG (within == (int) n, juce::String ((int) n - within) + " catches were not doubled within 20%");
    CHECK_MSG (n > 0 && std::abs (ratioSum / n - 2.0) <= 0.4,
               "mean catch ratio " + juce::String (n > 0 ? ratioSum / n : 0.0, 3));

    // 1: deeper into the winding - the pick's load on the pitch grows too, and stays slight.
    CHECK_MSG (hardCents > lightCents && lightCents > 0.0,
               "load " + juce::String (lightCents, 2) + " -> " + juce::String (hardCents, 2) + " cents");
    CHECK (hardCents < 10.0);
}

LUTHIER_TEST (Scrape, aReversedScrapeIsTheForwardOneMirrored)
{
    auto forward = makeScrape(), backward = makeScrape();
    forward->trigger (gestureOn (5, 0.5, 200.0, 600.0, 500.0));
    backward->trigger (gestureOn (5, 0.5, 600.0, 200.0, 500.0));

    Capture f, b;
    render (*forward, f, 100);
    render (*backward, b, 100);

    const auto fa = onsets (f.noise, 0.3 * maxAbs (f.noise));
    const auto ba = onsets (b.noise, 0.3 * maxAbs (b.noise));
    const auto fp = pulsePeaks (f.noise, fa), bp = pulsePeaks (b.noise, ba);

    CHECK_MSG (fa.size() == ba.size() && ! fa.empty(),
               juce::String ((int) fa.size()) + " catches forward, " + juce::String ((int) ba.size()) + " back");

    const int n = (int) juce::jmin (fa.size(), ba.size());
    const int gestureSamples = (int) std::round (0.5 * kSr);
    int timingMisses = 0, levelMisses = 0;

    for (int k = 0; k < n; ++k)
    {
        const int mirror = n - 1 - k;

        // The k-th catch forward is the k-th from last backward, at the mirrored time...
        if (std::abs (fa[(size_t) k] + ba[(size_t) mirror] - (gestureSamples - 1)) > 1)
            ++timingMisses;

        // ...on the same winding, so with the same irregularity.
        if (std::abs (fp[(size_t) k] - bp[(size_t) mirror]) > 1.0e-9 * juce::jmax (1.0, fp[(size_t) k]))
            ++levelMisses;
    }

    CHECK_MSG (timingMisses == 0, juce::String (timingMisses) + " catches not at their mirrored time");
    CHECK_MSG (levelMisses == 0, juce::String (levelMisses) + " catches not at their mirrored level");
}

LUTHIER_TEST (Scrape, aModwheelSweepCatchesWithinOneBlock)
{
    auto scrape = makeScrape();

    auto settings = ScrapeSettings::fromPreset (ScrapePreset::modwheelSweep, 6);
    settings.stringMask = 1 << 5;
    scrape->setSettings (settings);

    CHECK (settings.direction == ScrapeDirection::holdAndSweep);
    CHECK (settings.sweepSource == ScrapeSweepSource::modWheel);

    juce::MidiBuffer filtered;
    filtered.ensureSize (4096);

    std::vector<double> train;
    std::vector<juce::int64> catchesAfterBlock;
    int firstCatchBlock = -1, firstCcBlock = -1;

    constexpr int rampStart = 8, rampBlocks = 16, totalBlocks = 60;
    int value = 0;

    for (int b = 0; b < totalBlocks; ++b)
    {
        juce::MidiBuffer midi;

        if (b == 0)
        {
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 0), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, ScrapeEngine::kScrapeKeyswitch, (juce::uint8) 100), 0);
        }

        // A player's push on the wheel: one step every 64 samples, to 64.
        if (b >= rampStart && b < rampStart + rampBlocks)
        {
            for (int o = 0; o < kBlock; o += 64)
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, ++value), o);

            if (firstCcBlock < 0)
                firstCcBlock = b;
        }

        const auto& passed = scrape->handleMidi (midi, filtered);

        // The keyswitch is the technique's; nothing downstream sees it.
        for (const auto m : passed)
            CHECK (! (m.getMessage().isNoteOnOrOff() && m.getMessage().getNoteNumber() == ScrapeEngine::kScrapeKeyswitch));

        scrape->processBlock (kBlock);

        for (int i = 0; i < kBlock; ++i)
            train.push_back (scrape->hasOutput() ? scrape->getNoiseOutput()[i] : 0.0);

        catchesAfterBlock.push_back (scrape->getCatchCount (5));

        if (firstCatchBlock < 0 && scrape->getCatchCount (5) > 0)
            firstCatchBlock = b;

        if (b == rampStart + 2)
            CHECK_MSG (scrape->ownsVibratoControllers(), "the wheel drove vibrato and the scrape at once");
    }

    // Nothing moves while the wheel rests, and the first catch comes in the
    // block the wheel first moved.
    CHECK_MSG (catchesAfterBlock[(size_t) rampStart - 1] == 0, "catches before the wheel moved");
    CHECK_MSG (firstCatchBlock == firstCcBlock,
               "first catch in block " + juce::String (firstCatchBlock) + ", wheel moved in block "
                 + juce::String (firstCcBlock));

    // Every block the wheel moves in has catches in it.
    for (int b = rampStart + 1; b < rampStart + rampBlocks; ++b)
        CHECK_MSG (catchesAfterBlock[(size_t) b] > catchesAfterBlock[(size_t) b - 1],
                   "no catches in block " + juce::String (b) + " while the wheel moved");

    // And once it stops they stop, having crossed exactly the windings between
    // where the pick was and where the wheel put it.
    const double farMm = kScale - ScrapeEngine::kEdgeMm;
    const double endMm = 200.0 + (64.0 / 127.0) * (farMm - 200.0);
    const auto expected = (juce::int64) std::floor (endMm * kWindings) - (juce::int64) std::floor (200.0 * kWindings);

    CHECK (catchesAfterBlock.back() == catchesAfterBlock[(size_t) totalBlocks - 20]);
    CHECK_MSG (std::abs (catchesAfterBlock.back() - expected) <= 1,
               juce::String (catchesAfterBlock.back()) + " catches, expected " + juce::String (expected));

    // Letting go of the keyswitch lifts the pick.
    juce::MidiBuffer off;
    off.addEvent (juce::MidiMessage::noteOff (1, ScrapeEngine::kScrapeKeyswitch), 0);
    scrape->handleMidi (off, filtered);
    scrape->processBlock (kBlock);

    CHECK (! scrape->isStringMoving (5));
    CHECK (! scrape->ownsVibratoControllers());
}

LUTHIER_TEST (Scrape, aRetriggerInsideTheThresholdIsDropped)
{
    auto scrape = makeScrape();

    ScrapeSettings settings;
    settings.armed = true;
    settings.durationMs = 100.0;
    settings.retriggerMs = 200.0;
    scrape->setSettings (settings);

    juce::MidiBuffer filtered;
    filtered.ensureSize (4096);

    // Keyswitches at 0, 150 ms and 450 ms.
    const int at150 = (int) (0.150 * kSr), at450 = (int) (0.450 * kSr);
    juce::uint32 firedAfterSecond = 0;

    for (int b = 0; b < 120; ++b)
    {
        const int start = b * kBlock;
        juce::MidiBuffer midi;

        for (int t : { 0, at150, at450 })
            if (t >= start && t < start + kBlock)
                midi.addEvent (juce::MidiMessage::noteOn (1, ScrapeEngine::kScrapeKeyswitch, (juce::uint8) 100), t - start);

        scrape->handleMidi (midi, filtered);
        scrape->processBlock (kBlock);

        if (start <= at150 + kBlock && at150 < start + kBlock)
            firedAfterSecond = scrape->getFiredCount();
    }

    // The default mask is the wound strings: three gestures per trigger.
    CHECK_MSG (firedAfterSecond == 3, "the retrigger at 150 ms fired: " + juce::String ((int) firedAfterSecond));
    CHECK (scrape->getDroppedTriggerCount() == 1);
    CHECK_MSG (scrape->getFiredCount() == 6, "the trigger at 450 ms did not fire");
}

LUTHIER_TEST (Scrape, idleCostsNothing)
{
    auto scrape = makeScrape();

    CHECK (! scrape->isBusy());

    // Disarmed, the MIDI is not even looked at: the same buffer comes back.
    juce::MidiBuffer filtered, midi = keyswitch (ScrapeEngine::kScrapeKeyswitch, 0);
    CHECK (&scrape->handleMidi (midi, filtered) == &midi);

    constexpr int blocks = 100000;
    double best = 1.0e9;

    for (int run = 0; run < 3; ++run)
    {
        const auto start = juce::Time::getHighResolutionTicks();

        for (int b = 0; b < blocks; ++b)
            scrape->processBlock (512);

        best = juce::jmin (best, juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start));
    }

    const double realtime = blocks * 512.0 / kSr;

    CHECK (! scrape->hasOutput());
    CHECK_MSG (best / realtime < 0.0005,
               "idle cost " + juce::String (100.0 * best / realtime, 4) + "% of real time (budget 0.05%)");
}

LUTHIER_TEST (Scrape, anActiveScrapeStaysInBudget)
{
    // 6: an active scrape under 0.5% of real time - the default gesture on
    // every wound string, a second of it.
    double best = 1.0e9;

    for (int run = 0; run < 5; ++run)
    {
        auto scrape = makeScrape();

        ScrapeSettings settings;
        settings.armed = true;
        settings.durationMs = 1000.0;
        scrape->setSettings (settings);
        scrape->triggerFromSettings (0);

        const auto start = juce::Time::getHighResolutionTicks();

        for (int b = 0; b < (int) (kSr / kBlock); ++b)
            scrape->processBlock (kBlock);

        best = juce::jmin (best, juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start));

        CHECK (scrape->getCatchCount (5) > 0);
    }

    CHECK_MSG (best < 0.005, "a second of scraping took " + juce::String (best * 1000.0, 2) + " ms (budget 5 ms)");
}

LUTHIER_TEST (Scrape, resetRepeatsExactly)
{
    auto scrape = makeScrape();
    scrape->setSeed (1234);

    auto run = [&scrape]
    {
        scrape->reset();
        scrape->setSweepValue (0.3, 0);
        scrape->trigger (gestureOn (4, 0.7, 150.0, 500.0, 300.0));
        scrape->trigger (gestureOn (5, 0.6, 600.0, 250.0, 400.0), 77);

        Capture c;
        render (*scrape, c, 90);
        return c;
    };

    const auto a = run(), b = run();

    CHECK (a.noise == b.noise);
    CHECK (a.excitation[4] == b.excitation[4]);
    CHECK (a.excitation[5] == b.excitation[5]);

    // And the seed is what the windings' irregularity comes from.
    scrape->setSeed (99);
    const auto c = run();
    CHECK (c.noise != a.noise);
}

LUTHIER_TEST (Scrape, aScrapeOnAScrapeQueues)
{
    // technique-cascade.md 2: scrape x scrape on one string is "queue".
    auto scrape = makeScrape();
    scrape->trigger (gestureOn (5, 0.5, 200.0, 400.0, 100.0));

    Capture c;
    render (*scrape, c, 2);
    CHECK (scrape->getNumRecords() == 0);   // the first started in block 0

    scrape->trigger (gestureOn (5, 0.5, 200.0, 400.0, 100.0));
    render (*scrape, c, 1);
    CHECK_MSG (scrape->getNumRecords() == 0, "the second scrape started over the first");

    // The first ends at 100 ms; the second follows it rather than being lost.
    int secondStartedInBlock = -1;

    for (int b = 3; b < 60; ++b)
    {
        render (*scrape, c, 1);

        if (scrape->getNumRecords() > 0 && secondStartedInBlock < 0)
            secondStartedInBlock = b;
    }

    const auto one = (juce::int64) std::floor (400.0 * kWindings) - (juce::int64) std::floor (200.0 * kWindings);

    CHECK_MSG (secondStartedInBlock >= (int) (0.1 * kSr / kBlock) - 1,
               "second scrape started in block " + juce::String (secondStartedInBlock));
    CHECK_MSG (std::abs (scrape->getCatchCount (5) - 2 * one) <= 2,
               juce::String (scrape->getCatchCount (5)) + " catches for two scrapes of " + juce::String (one));
}

LUTHIER_TEST (Scrape, aPreemptedScrapeFadesOutInTenMilliseconds)
{
    // technique-cascade.md 3.3 and string-scraping.md 5: a tap on the string
    // damps the scrape - gracefully, over 10 ms.
    auto scrape = makeScrape();
    scrape->trigger (gestureOn (5, 0.5, 200.0, 600.0, 500.0));

    Capture c;
    render (*scrape, c, 20);
    CHECK (scrape->isStringActive (5));

    scrape->preempt (5);
    const size_t preemptAt = c.excitation[5].size();
    render (*scrape, c, 10);

    const size_t fadeEnd = preemptAt + (size_t) std::ceil (ScrapeEngine::kPreemptFadeSeconds * kSr) + 2;
    double after = 0.0, during = 0.0;

    for (size_t i = preemptAt; i < c.excitation[5].size(); ++i)
    {
        const double v = std::abs (c.excitation[5][i]);

        if (i < fadeEnd)
            during = juce::jmax (during, v);
        else
            after = juce::jmax (after, v);
    }

    CHECK_MSG (during > 0.0, "the scrape stopped dead instead of fading");
    CHECK_MSG (after == 0.0, "the scrape is still sounding 10 ms after it was preempted");
    CHECK (! scrape->isStringActive (5));

    // And a string the slide bar holds cannot be scraped at all (3.4).
    scrape->setStringBlocked (4, true);
    scrape->trigger (gestureOn (4));
    render (*scrape, c, 10);
    CHECK (scrape->getCatchCount (4) == 0);
}

LUTHIER_TEST (Scrape, aMuteMakesItDullerAndThumpier)
{
    // 5: scrape through a palm mute - duller, thumpier.
    auto run = [] (double mute)
    {
        auto scrape = makeScrape();
        scrape->setMuteAmount (mute);
        scrape->trigger (gestureOn (5, 0.5, 200.0, 600.0, 900.0));

        Capture c;
        render (*scrape, c, 100);
        return c.noise;
    };

    const auto open = run (0.0), muted = run (1.0);
    const double openCentroid = spectralCentroid (open, kSr), mutedCentroid = spectralCentroid (muted, kSr);

    CHECK_MSG (mutedCentroid < openCentroid * 0.8,
               "centroid " + juce::String (openCentroid, 0) + " Hz open, " + juce::String (mutedCentroid, 0) + " muted");
    CHECK (maxAbs (muted) < maxAbs (open));
}

LUTHIER_TEST (Scrape, theFactoryScrapesAreWhatSectionFourSays)
{
    using P = ScrapePreset;
    const int lowThree = (1 << 3) | (1 << 4) | (1 << 5);

    const auto rock = ScrapeSettings::fromPreset (P::classicRock, 6);
    CHECK (rock.armed && rock.tool == ScrapeTool::pick && rock.pressure == 0.5);
    CHECK (rock.direction == ScrapeDirection::bridgeToNut && rock.durationMs == 600.0 && rock.stringMask == lowThree);

    const auto metal = ScrapeSettings::fromPreset (P::metalZipper, 6);
    CHECK (metal.pressure > rock.pressure && metal.durationMs == 300.0 && metal.stringMask == (lowThree | (1 << 2)));

    const auto ratchet = ScrapeSettings::fromPreset (P::slowRatchet, 6);
    CHECK (ratchet.pressure < rock.pressure && ratchet.durationMs == 2000.0 && ratchet.stringMask == (1 << 5));

    const auto nail = ScrapeSettings::fromPreset (P::nailScrape, 6);
    CHECK (nail.tool == ScrapeTool::nail && nail.pressure < rock.pressure && nail.durationMs == 800.0
           && nail.stringMask == lowThree);

    const auto wheel = ScrapeSettings::fromPreset (P::modwheelSweep, 6);
    CHECK (wheel.direction == ScrapeDirection::holdAndSweep && wheel.sweepSource == ScrapeSweepSource::modWheel);

    // Seven strings: "low 3" follows the set down.
    CHECK (ScrapeSettings::fromPreset (P::classicRock, 7).stringMask == ((1 << 4) | (1 << 5) | (1 << 6)));

    // 0.3: the zipper is a zipper and the ratchet is separate clicks.
    auto rate = [] (const ScrapeSettings& s)
    {
        auto scrape = makeScrape();
        auto one = s;
        one.stringMask = 1 << 5;
        scrape->setSettings (one);
        scrape->triggerFromSettings (0);

        Capture c;
        render (*scrape, c, (int) std::ceil ((s.durationMs * 0.001 + 0.05) * kSr / kBlock));

        const auto at = onsets (c.noise, 0.3 * maxAbs (c.noise));
        return at.size() > 1 ? (double) (at.size() - 1) / ((at.back() - at.front()) / kSr) : 0.0;
    };

    const double zipper = rate (metal), slow = rate (ratchet);

    CHECK_MSG (zipper > 5.0 * slow, "zipper " + juce::String (zipper, 0) + "/s, ratchet " + juce::String (slow, 0) + "/s");
    CHECK_MSG (1000.0 / slow > 1.0, "the ratchet's clicks are under a millisecond apart");
}

LUTHIER_TEST (Scrape, triggersListenOnlyWhereTheyAreTold)
{
    juce::MidiBuffer filtered;
    filtered.ensureSize (4096);

    // Keyswitch: note 12 is taken, an ordinary note goes on.
    {
        auto scrape = makeScrape();
        ScrapeSettings s;
        s.armed = true;
        scrape->setSettings (s);

        auto midi = keyswitch (ScrapeEngine::kScrapeKeyswitch, 10);
        midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 90), 20);

        const auto& passed = scrape->handleMidi (midi, filtered);
        CHECK (&passed == &filtered);
        CHECK (passed.getNumEvents() == 1);

        for (const auto m : passed)
            CHECK (m.getMessage().getNoteNumber() == 64 && m.samplePosition == 20);

        scrape->processBlock (kBlock);
        CHECK (scrape->getNumRecords() == 3);
        CHECK (scrape->getRecord (0).offset == 10);
    }

    // The MPE zone: channel 16 triggers, channel 1 plays.
    {
        auto scrape = makeScrape();
        ScrapeSettings s;
        s.armed = true;
        s.trigger = ScrapeTriggerSource::mpeZone;
        scrape->setSettings (s);

        auto midi = keyswitch (60, 0, ScrapeEngine::kZoneChannel);
        midi.addEvent (juce::MidiMessage::noteOn (1, ScrapeEngine::kScrapeKeyswitch, (juce::uint8) 90), 5);

        const auto& passed = scrape->handleMidi (midi, filtered);
        CHECK (passed.getNumEvents() == 1);   // note 12 is an ordinary note here

        scrape->processBlock (kBlock);
        CHECK (scrape->getNumRecords() == 3);
    }

    // The trigger CC fires on its rising edge and passes on.
    {
        auto scrape = makeScrape();
        ScrapeSettings s;
        s.armed = true;
        s.trigger = ScrapeTriggerSource::controller;
        scrape->setSettings (s);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, s.triggerCc, 127), 3);
        midi.addEvent (juce::MidiMessage::controllerEvent (1, s.triggerCc, 120), 4);   // still down: no second trigger

        CHECK (&scrape->handleMidi (midi, filtered) == &midi);
        scrape->processBlock (kBlock);
        CHECK (scrape->getNumRecords() == 3);
        CHECK (scrape->getDroppedTriggerCount() == 0);
    }

    // Button only: MIDI does nothing, the button does - when armed.
    {
        auto scrape = makeScrape();
        ScrapeSettings s;
        s.armed = true;
        s.trigger = ScrapeTriggerSource::buttonOnly;
        scrape->setSettings (s);

        auto midi = keyswitch (ScrapeEngine::kScrapeKeyswitch, 0);
        CHECK (&scrape->handleMidi (midi, filtered) == &midi);
        scrape->processBlock (kBlock);
        CHECK (scrape->getNumRecords() == 0);

        scrape->requestTrigger();
        CHECK (scrape->isBusy());
        scrape->processBlock (kBlock);
        CHECK (scrape->getNumRecords() == 3);

        s.armed = false;
        scrape->setSettings (s);
        scrape->requestTrigger();
        scrape->processBlock (kBlock);
        CHECK_MSG (scrape->getNumRecords() == 0, "the button scraped with the technique disarmed");
    }
}

LUTHIER_TEST (Scrape, theLevelTrimScalesAndZeroIsFree)
{
    // Coverage C-29: pick_scrape_amount trims the scrape; pick-noise.md 0.5:
    // at zero nothing starts at all.
    auto peakAt = [] (double level)
    {
        auto scrape = makeScrape();
        ScrapeSettings s;
        s.level = level;
        scrape->setSettings (s);
        scrape->trigger (gestureOn (5));

        Capture c;
        render (*scrape, c, 1);
        const int records = scrape->getNumRecords();
        render (*scrape, c, 60);

        return std::make_pair (maxAbs (c.noise), records);
    };

    const auto unity = peakAt (1.0), half = peakAt (0.5), off = peakAt (0.0);

    CHECK (unity.second == 1 && half.second == 1);
    CHECK_NEAR (half.first / unity.first, 0.5, 1.0e-9);
    CHECK (off.first == 0.0);
    CHECK_MSG (off.second == 0, "a scrape at level 0 still started");
}

//==============================================================================
//  Through the whole engine
//==============================================================================
LUTHIER_TEST (ScrapeEngineWiring, thePlainHighEIsThirtyDecibelsUnderTheWoundLowE)
{
    // 6, measured where the strings sum, as the difference a scrape makes.
    auto silent = makeEngine();
    const auto baseline = renderEngine (*silent, 120);

    auto scrapeString = [&baseline] (int stringIndex)
    {
        auto engine = makeEngine();
        engine->getScrapeEngine().trigger (gestureOn (stringIndex, 0.5, 200.0, 600.0, 500.0));
        const auto out = renderEngine (*engine, 120);

        std::vector<double> diff (out.size());

        for (size_t i = 0; i < out.size(); ++i)
            diff[i] = out[i] - baseline[i];

        return rmsOf (diff);
    };

    auto probe = makeEngine();
    CHECK (probe->getStringSpec (5).wound);
    CHECK (! probe->getStringSpec (0).wound);

    const double woundRms = scrapeString (5), plainRms = scrapeString (0);
    const double below = gainToDb (juce::jmax (1.0e-12, plainRms) / juce::jmax (1.0e-12, woundRms));

    CHECK_MSG (woundRms > 1.0e-4, "the wound low E's scrape did not reach the strings' output");
    CHECK_MSG (below < -30.0, "the plain high E is only " + juce::String (-below, 1) + " dB under the low E");
}

LUTHIER_TEST (ScrapeEngineWiring, harderScrapingLoadsThePitchSlightly)
{
    auto frequencyAfter = [] (double pressure)
    {
        auto engine = makeEngine();

        if (pressure > 0.0)
            engine->getScrapeEngine().trigger (gestureOn (5, pressure, 200.0, 600.0, 600.0));

        renderEngine (*engine, 40);   // ~210 ms, mid-scrape
        return engine->getStringFrequency (5);
    };

    const double f0 = frequencyAfter (0.0), f1 = frequencyAfter (0.5), f2 = frequencyAfter (1.0);
    const double c1 = ratioToCents (f1 / f0), c2 = ratioToCents (f2 / f0);

    CHECK_MSG (c1 > 0.5, "pressure 0.5 moved the pitch " + juce::String (c1, 2) + " cents");
    CHECK_MSG (c2 > 1.5 * c1, "pressure 1.0 moved it " + juce::String (c2, 2) + " cents, 0.5 moved it "
                                + juce::String (c1, 2));
    CHECK_MSG (c2 < 10.0, "a scrape bent the string " + juce::String (c2, 2) + " cents");
}

LUTHIER_TEST (ScrapeEngineWiring, theKeyswitchScrapesAndNeverPlaysANote)
{
    auto engine = makeEngine();

    ScrapeSettings settings;
    settings.armed = true;
    engine->setScrapeSettings (settings);

    juce::AudioBuffer<float> block (2, kBlock);
    auto midi = keyswitch (ScrapeEngine::kScrapeKeyswitch, 37);
    engine->processBlock (block, midi);

    // midi-export.md 6: the gesture goes out as a SysEx record, at its sample.
    const auto& pool = engine->getNoisePool();
    int scrapeRecords = 0;

    for (int i = 0; i < pool.getNumBlockTriggers(); ++i)
    {
        const auto& t = pool.getBlockTrigger (i);

        if (t.noiseClass == NoiseClass::pickScrape)
        {
            ++scrapeRecords;
            CHECK (t.offset == 37);
            CHECK (engine->getStringSpec (t.stringIndex).wound);
        }
    }

    int woundCount = 0;

    for (int s = 0; s < engine->getNumStrings(); ++s)
        woundCount += engine->getStringSpec (s).wound ? 1 : 0;

    CHECK_MSG (scrapeRecords == woundCount, juce::String (scrapeRecords) + " scrape records for "
                                               + juce::String (woundCount) + " wound strings");

    // pick-noise.md 1.3: the catches are on the noise bus.
    double busPeak = 0.0;

    for (int b = 0; b < 8; ++b)
    {
        const double* bus = engine->getNoiseBusData();

        for (int i = 0; i < kBlock; ++i)
            busPeak = juce::jmax (busPeak, std::abs (bus[i]));

        block.clear();
        juce::MidiBuffer none;
        engine->processBlock (block, none);
    }

    CHECK_MSG (busPeak > 0.0, "the scrape never reached Aux 8");

    // And note 12 was the technique's, not a note.
    CHECK (engine->getMidiInterpreter().getActiveNoteCount() == 0);

    for (int s = 0; s < engine->getNumStrings(); ++s)
        CHECK (engine->getStringMidiNote (s) < 0);
}

LUTHIER_TEST (ScrapeEngineWiring, theRakeHasATrigger)
{
    // pick-noise.md 5: the rake down the wound strings, by its keyswitch...
    {
        auto engine = makeEngine();

        ScrapeSettings settings;
        settings.armed = true;
        settings.durationMs = 300.0;
        engine->setScrapeSettings (settings);

        renderEngine (*engine, 60, keyswitch (ScrapeEngine::kRakeDownKeyswitch, 0));

        CHECK_MSG (engine->getNoisePool().getTriggerCount (NoiseClass::pickScrape) > 0,
                   "the rake keyswitch raked nothing");
        CHECK (engine->getScrapeEngine().getCatchCount (5) == 0);   // a rake, not a scrape along the string
    }

    // ...and by the Easy-mode gesture, which is the player's hand, armed or not.
    {
        auto engine = makeEngine();
        engine->getScrapeEngine().requestRake (true);
        renderEngine (*engine, 60);

        CHECK_MSG (engine->getNoisePool().getTriggerCount (NoiseClass::pickScrape) > 0,
                   "the UI's rake raked nothing");
    }
}
