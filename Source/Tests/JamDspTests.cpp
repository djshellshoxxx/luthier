/*  Jam mode, DSP and real time (jam-mode.md 17, JM-24 to JM-34): the kit's
    physics, the bass's pitch and string choice, and the band's cost. */

#include "JamTestHarness.h"
#include "../Support/ThreadProbe.h"

#if JUCE_LINUX
 #include <dlfcn.h>
 #include <pthread.h>
#endif

using namespace luthier;
using namespace luthier::tests;

namespace luthier::tests
{
    long allocationsOnThisThread() noexcept;   // CircuitTests.cpp's counter
}

//==============================================================================
/*  JM-33's lock trap. Blocking locks: AudioThreadSafetyTests.cpp interposes
    pthread_mutex_lock and counts it on a ThreadProbe-marked thread. Try-locks
    are counted here as well - the band takes none of either. The executable's
    definition wins over libc's, and forwards to it. */
#if JUCE_LINUX
namespace
{
    thread_local bool countLocks = false;
    std::atomic<int> lockCount { 0 };
}

extern "C" int pthread_mutex_trylock (pthread_mutex_t* m)
{
    using Fn = int (*) (pthread_mutex_t*);
    static std::atomic<void*> resolved { nullptr };   // no static guard: it could lock
    auto* raw = resolved.load();

    if (raw == nullptr)
    {
        raw = dlsym (RTLD_NEXT, "pthread_mutex_trylock");
        resolved.store (raw);
    }

    auto real = (Fn) raw;

    if (countLocks)
        lockCount.fetch_add (1);

    return real (m);
}
#endif

namespace
{
    constexpr double kSr = 48000.0;

    /** The frequency of the strongest component within +-`span` of `hz`,
        by a Hann-windowed DFT on a 0.05 % grid. */
    double peakNear (const std::vector<double>& x, double sr, double hz, double span)
    {
        const int n = (int) x.size();
        double best = hz, bestMag = -1.0;

        for (double f = hz * (1.0 - span); f <= hz * (1.0 + span); f += hz * 0.0005)
        {
            double re = 0.0, im = 0.0;
            const double w = constants::kTwoPi * f / sr;

            for (int i = 0; i < n; ++i)
            {
                const double win = 0.5 - 0.5 * std::cos (constants::kTwoPi * i / (n - 1));
                re += x[(size_t) i] * win * std::cos (w * i);
                im -= x[(size_t) i] * win * std::sin (w * i);
            }

            const double mag = re * re + im * im;

            if (mag > bestMag)
            {
                bestMag = mag;
                best = f;
            }
        }

        return best;
    }

    /** RMS of `x` in [from, to) after a 2-biquad band-pass. */
    double bandRms (const std::vector<double>& x, double sr, double lo, double hi, int from, int to)
    {
        Biquad h1, h2, l1, l2;
        h1.setHighpass (sr, lo, 0.707); h2.setHighpass (sr, lo, 0.707);
        l1.setLowpass (sr, hi, 0.707);  l2.setLowpass (sr, hi, 0.707);
        double sum = 0.0;
        int count = 0;

        for (int i = 0; i < juce::jmin (to, (int) x.size()); ++i)
        {
            const double y = l2.process (l1.process (h2.process (h1.process (x[(size_t) i]))));

            if (i >= from)
            {
                sum += y * y;
                ++count;
            }
        }

        return count > 0 ? std::sqrt (sum / count) : 0.0;
    }

    /** Autocorrelation pitch near `hz`, with parabolic interpolation. */
    double autocorrelationPitch (const std::vector<double>& x, int from, int length, double sr, double hz)
    {
        const double period = sr / hz;
        const int lo = (int) (period * 0.93), hi = (int) (period * 1.07) + 2;
        std::vector<double> r ((size_t) (hi + 2), 0.0);

        for (int lag = lo - 1; lag <= hi + 1; ++lag)
        {
            double s = 0.0, e1 = 0.0, e2 = 0.0;

            for (int i = from; i < from + length; ++i)
            {
                s += x[(size_t) i] * x[(size_t) (i + lag)];
                e1 += x[(size_t) i] * x[(size_t) i];
                e2 += x[(size_t) (i + lag)] * x[(size_t) (i + lag)];
            }

            r[(size_t) lag] = s / std::sqrt (juce::jmax (1.0e-30, e1 * e2));
        }

        int best = lo;

        for (int lag = lo; lag <= hi; ++lag)
            if (r[(size_t) lag] > r[(size_t) best])
                best = lag;

        const double a = r[(size_t) (best - 1)], b = r[(size_t) best], c = r[(size_t) (best + 1)];
        const double denom = a - 2.0 * b + c;
        const double shift = std::abs (denom) > 1.0e-12 ? 0.5 * (a - c) / denom : 0.0;
        return sr / ((double) best + shift);
    }

    std::vector<double> renderKit (JamDrumKit& kit, int samples)
    {
        std::vector<double> l ((size_t) samples), r ((size_t) samples), mono ((size_t) samples);
        kit.render (l.data(), r.data(), samples);

        for (size_t i = 0; i < mono.size(); ++i)
            mono[i] = 0.5 * (l[i] + r[i]);

        return mono;
    }
}

//==============================================================================
LUTHIER_TEST (JamDsp, JM24_kickModesAndPitchDrop)
{
    JamDrumKit kit;
    kit.prepare (kSr, 512);
    kit.setRoom (0.0);
    kit.trigger (DrumSound::kick, 1.0);
    const auto x = renderKit (kit, (int) kSr);

    const std::vector<double> tail (x.begin() + (int) (0.1 * kSr), x.begin() + (int) (0.9 * kSr));
    const double f0 = kit.getKickF0();

    for (double ratio : { 1.0, 1.594, 2.136 })
    {
        const double found = peakNear (tail, kSr, f0 * ratio, 0.05);
        CHECK_MSG (std::abs (found / (f0 * ratio) - 1.0) <= 0.02,
                   "mode " + juce::String (ratio) + " at " + juce::String (found, 2) + " Hz, expected " + juce::String (f0 * ratio, 2));
    }

    // The tension-modulation pitch drop, read from the membrane.
    MembranePiece& kick = kit.getKickVoice (0);
    kick.reset();
    kick.strike (1.0);
    double at5 = 0.0, at150 = 0.0;

    for (int i = 0; i < (int) (0.2 * kSr); ++i)
    {
        kick.process();

        if (i == (int) (0.005 * kSr)) at5 = kick.getCurrentFundamentalHz();
        if (i == (int) (0.150 * kSr)) at150 = kick.getCurrentFundamentalHz();
    }

    const double drop = at5 / at150 - 1.0;
    CHECK_MSG (drop >= 0.10 && drop <= 0.25, "pitch at 5 ms is " + juce::String (drop * 100.0, 1) + " % above 150 ms");
}

LUTHIER_TEST (JamDsp, JM25_snareWires)
{
    auto render = [] (double threshold)
    {
        SnarePiece snare;
        snare.prepare (kSr);
        snare.setWireThreshold (threshold);
        snare.strike (0.8, SnarePiece::Stroke::normal);
        std::vector<double> x ((size_t) (0.5 * kSr));

        for (auto& s : x)
            s = snare.process();

        return x;
    };

    const auto normal = render (0.06), tight = render (1.0);
    const int from = (int) (0.020 * kSr), to = (int) (0.300 * kSr);
    const double normalWires = bandRms (normal, kSr, 3000.0, 6000.0, from, to);
    const double tightWires = bandRms (tight, kSr, 3000.0, 6000.0, from, to);

    CHECK_MSG (tightWires < 0.1 * normalWires,
               "maximum threshold removes " + juce::String (100.0 * (1.0 - tightWires / normalWires), 1) + " % of the 3-6 kHz energy");

    // Default wires: gone within 250 ms (40 dB under their first 50 ms).
    const double early = bandRms (normal, kSr, 3000.0, 6000.0, 0, (int) (0.05 * kSr));
    const double late = bandRms (normal, kSr, 3000.0, 6000.0, (int) (0.250 * kSr), (int) (0.270 * kSr));
    CHECK_MSG (late < early * 0.01, "wires at 250 ms are " + juce::String (gainToDb (late / early), 1) + " dB");
}

LUTHIER_TEST (JamDsp, JM26_closingTheHatChokesIt)
{
    // The kit's own hat (its design and level).
    JamDrumKit kit;
    kit.prepare (kSr, 512);
    CymbalPiece& hat = kit.getHat();
    hat.strike (0.9, CymbalPiece::Hit::open);

    const int closeAt = (int) (0.300 * kSr);
    std::vector<double> x ((size_t) (0.4 * kSr));

    for (int i = 0; i < (int) x.size(); ++i)
    {
        if (i == closeAt)
            hat.strike (0.0, CymbalPiece::Hit::close);

        x[(size_t) i] = hat.process();
    }

    const double before = bandRms (x, kSr, 5000.0, 10000.0, closeAt - (int) (0.010 * kSr), closeAt);
    const double after = bandRms (x, kSr, 5000.0, 10000.0, closeAt + (int) (0.015 * kSr), closeAt + (int) (0.020 * kSr));
    CHECK_MSG (after <= before * 0.01, "15 ms after closing, 5-10 kHz is down " + juce::String (-gainToDb (after / before), 1) + " dB");

    // The choke itself must not click: the largest step from the close on.
    double step = 0.0;

    for (size_t i = (size_t) closeAt; i < x.size(); ++i)
        step = juce::jmax (step, std::abs (x[i] - x[i - 1]));

    CHECK_MSG (step < 0.05, "maximum sample step while closing " + juce::String (step, 4));
}

LUTHIER_TEST (JamDsp, JM27_rideRestrikeIsContinuous)
{
    CymbalPiece ride;
    ride.prepare (kSr, CymbalPiece::Kind::ride);
    ride.strike (0.8, CymbalPiece::Hit::bow);

    const int again = (int) (0.5 * kSr);
    std::vector<double> x ((size_t) (0.6 * kSr));

    for (int i = 0; i < (int) x.size(); ++i)
    {
        if (i == again)
            ride.strike (0.8, CymbalPiece::Hit::bow);

        x[(size_t) i] = ride.process();
    }

    auto maxStep = [&x] (int from, int to)
    {
        double s = 0.0;

        for (int i = juce::jmax (1, from); i < to; ++i)
            s = juce::jmax (s, std::abs (x[(size_t) i] - x[(size_t) (i - 1)]));

        return s;
    };

    const int w = (int) (0.002 * kSr);
    const double strike = maxStep (0, w), ringing = maxStep (again - w, again), restrike = maxStep (again, again + w);

    // Linear superposition: a re-strike adds its own step to the ring's, no more.
    CHECK_MSG (restrike <= strike + ringing + 1.0e-9,
               "re-strike step " + juce::String (restrike, 5) + " > strike " + juce::String (strike, 5) + " + ring " + juce::String (ringing, 5));
}

LUTHIER_TEST (JamDsp, JM28_kitTuningIsATensionChange)
{
    JamDrumKit kit;
    kit.prepare (kSr, 512);
    kit.setRoom (0.0);
    kit.setTuning (12.0, 0.4);
    kit.trigger (DrumSound::kick, 0.3);   // soft, so the tension glide is short
    const auto x = renderKit (kit, (int) kSr);
    const std::vector<double> tail (x.begin() + (int) (0.15 * kSr), x.begin() + (int) (0.9 * kSr));

    const double found = peakNear (tail, kSr, 110.0, 0.05);
    CHECK_MSG (std::abs (found / 110.0 - 1.0) <= 0.01, "+12 st kick at " + juce::String (found, 2) + " Hz");
    CHECK_NEAR (kit.getKickF0(), 110.0, 0.01);
}

LUTHIER_TEST (JamDsp, JM29_bassPitchAndClicklessChanges)
{
    JamBassVoice bass;
    bass.prepare (kSr, 512);
    double worstCents = 0.0, worstStep = 0.0;
    int worstNote = 0;

    // Pitch: each note on its own (the previous note's released tail is not
    // this note's pitch), measured on its fundamental partial after 100 ms.
    for (int note = 28; note <= 48; ++note)
    {
        bass.reset();
        bass.noteOn (note, 0.8, false);
        std::vector<double> x ((size_t) (0.6 * kSr));
        bass.render (x.data(), (int) x.size());

        const double hz = midiToHz ((double) note);
        const double measured = JamBassVoice::measureFundamental (x.data() + (int) (0.1 * kSr), (int) (0.45 * kSr), kSr, hz);
        const double cents = std::abs (ratioToCents (measured / hz));

        if (juce::SystemStats::getEnvironmentVariable ("LUTHIER_JAM_VERBOSE", {}).isNotEmpty())
            std::cout << "      note " << note << ": " << ratioToCents (measured / hz) << " cents" << std::endl;

        if (cents > worstCents)
        {
            worstCents = cents;
            worstNote = note;
        }
    }

    // Changes: a line of 8ths through the register, every sample step small.
    bass.reset();

    for (int k = 0; k < 42; ++k)
    {
        bass.noteOn (28 + (k * 7) % 21, 0.8, k % 5 == 4);
        std::vector<double> x ((size_t) (0.125 * kSr));
        bass.render (x.data(), (int) x.size());

        for (size_t i = 1; i < x.size(); ++i)
            worstStep = juce::jmax (worstStep, std::abs (x[i] - x[i - 1]));
    }

    CHECK_MSG (worstCents <= 3.0, "note " + juce::String (worstNote) + " is " + juce::String (worstCents, 2) + " cents off");
    CHECK_MSG (worstStep < 0.05, "a note change stepped " + juce::String (worstStep, 4));
}

LUTHIER_TEST (JamDsp, JM30_bassStaysInPositionAndAlternates)
{
    JamBassVoice bass;
    bass.prepare (kSr, 512);
    int lastInstance = -1, alternations = 0, maxFret = 0, minFret = 99;

    for (int round = 0; round < 3; ++round)
        for (int note : { 33, 38, 43 })   // A1 D2 G2
        {
            bass.noteOn (note, 0.8, false);
            std::vector<double> x (2048);
            bass.render (x.data(), (int) x.size());

            maxFret = juce::jmax (maxFret, bass.getLastFret());
            minFret = juce::jmin (minFret, bass.getLastFret());

            if (lastInstance >= 0 && bass.getActiveInstance() != lastInstance)
                ++alternations;

            lastInstance = bass.getActiveInstance();
        }

    CHECK_MSG (maxFret <= 7 && maxFret - minFret <= 4, "frets " + juce::String (minFret) + "-" + juce::String (maxFret));
    CHECK_MSG (alternations == 8, "the two waveguides alternated " + juce::String (alternations) + " times in 9 notes");
}

LUTHIER_TEST (JamDsp, JM31_aJamReadsNoFiles)
{
    JamBench b;
    b.host = b.hostPlaying = true;
    b.chord (0, { 45, 48, 52 });

    const int before = ThreadProbe::audioThreadFileAccesses.load();
    ThreadProbe::markAsAudioThread (true);
    b.runSeconds (60.0);
    ThreadProbe::markAsAudioThread (false);

    CHECK (ThreadProbe::audioThreadFileAccesses.load() == before);
}

LUTHIER_TEST (JamDsp, JM32_noNanAndNoDcAtEveryRate)
{
    // 10 minutes at every rate with LUTHIER_JAM_LONG set; 60 s by default,
    // which covers every style's fills and the kit's longest ring (6 s) ten
    // times over.
    const double seconds = juce::SystemStats::getEnvironmentVariable ("LUTHIER_JAM_LONG", {}).isNotEmpty() ? 600.0 : 60.0;

    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        JamBench b (rate, 512);
        b.host = b.hostPlaying = true;
        b.settings.intensity = 5;
        b.settings.humanise = 100.0;
        b.settings.fillEvery = 1;
        b.settings.style = 5;
        b.keepAudio = true;

        const int bars = (int) (seconds / 2.0);
        const int64_t bar = (int64_t) (2.0 * rate);
        const std::initializer_list<int> cycle[] = { { 45, 48, 52 }, { 41, 45, 48 }, { 48, 52, 55 }, { 43, 47, 50 } };

        for (int i = 0; i < bars; ++i)
            b.chord (i * bar + 100, cycle[i % 4]);

        b.run ((int64_t) (seconds * rate));

        bool finite = true;
        double sum = 0.0;

        for (size_t i = 0; i < b.left.size(); ++i)
        {
            finite = finite && std::isfinite (b.left[i]) && std::isfinite (b.right[i]);
            sum += b.left[i] + b.right[i];
        }

        const double dc = std::abs (sum / (2.0 * (double) b.left.size()));
        CHECK_MSG (finite, juce::String (rate) + " Hz rendered a non-finite sample");
        CHECK_MSG (dc < std::pow (10.0, -60.0 / 20.0), juce::String (rate) + " Hz DC at " + juce::String (gainToDb (dc), 1) + " dBFS");
    }

    CHECK (ModalResonatorBank::getNanResetCounter().load() == 0);
}

//==============================================================================
LUTHIER_TEST (JamDsp, JM33_noAllocationsAndNoLocks)
{
    JamBench b (48000.0, 256);
    b.host = b.hostPlaying = true;
    b.settings.humanise = 60.0;
    b.settings.fillEvery = 1;
    b.settings.dynamicsFollow = true;

    const double seconds = juce::SystemStats::getEnvironmentVariable ("LUTHIER_JAM_LONG", {}).isNotEmpty() ? 300.0 : 60.0;
    const std::initializer_list<int> cycle[] = { { 45, 48, 52 }, { 41, 45, 48 }, { 48, 52, 55 }, { 43, 47, 50 } };
    const int bars = (int) (seconds / 2.0);

    for (int i = 0; i < bars; ++i)
        b.chord (i * 96000 + 1000, cycle[i % 4]);

    // Style, kit and chord-map swaps every bar; the chord map is built outside
    // the measured region, as the message thread would.
    long allocations = 0;
    int bar = -1;
    std::vector<std::unique_ptr<JamChordMap>> maps;

    for (int i = 0; i < 8; ++i)
    {
        auto m = std::make_unique<JamChordMap>();
        m->count = 1;
        m->entries[0].ppq = 0.0;
        m->entries[0].root = i % 12;
        m->entries[0].templateIndex = 0;
        m->lengthPpq = 4.0;
        m->loop = true;
        maps.push_back (std::move (m));
    }

    const int64_t total = (int64_t) (seconds * 48000.0);
    long startCount = 0;

    const int locksBefore = ThreadProbe::audioThreadLocks.load();

    b.aroundProcess = [&] (bool starting)
    {
        if (starting)
        {
            startCount = allocationsOnThisThread();
           #if JUCE_LINUX
            countLocks = true;
           #endif
            ThreadProbe::markAsAudioThread (true);
        }
        else
        {
            ThreadProbe::markAsAudioThread (false);
           #if JUCE_LINUX
            countLocks = false;
           #endif
            allocations += allocationsOnThisThread() - startCount;
        }
    };

    while (b.position < total)
    {
        const int nowBar = (int) (b.hostPpq / 4.0);

        if (nowBar != bar)
        {
            bar = nowBar;
            b.settings.style = bar % 10;
            b.settings.kitAuto = false;
            b.settings.kit = bar % 5;
            b.settings.chordSource = bar % 3;
            b.engine.setChordMap (std::move (maps[(size_t) (bar % 8)]));
            maps[(size_t) (bar % 8)] = std::make_unique<JamChordMap>();
            b.engine.collectGarbage();
        }

        b.run (b.block);
    }

    CHECK_MSG (allocations == 0, juce::String (allocations) + " allocations on the audio path");
    CHECK_MSG (ThreadProbe::audioThreadLocks.load() == locksBefore,
               juce::String (ThreadProbe::audioThreadLocks.load() - locksBefore) + " blocking locks on the audio path");
   #if JUCE_LINUX
    CHECK_MSG (lockCount.load() == 0, juce::String (lockCount.load()) + " mutex locks on the audio path");
   #endif
}

LUTHIER_TEST (JamDsp, JM34_budgets)
{
    auto seconds = [] (auto&& fn)
    {
        // The thread's CPU clock (TestFramework), so a busy machine's scheduling does not count.
        const double start = threadCpuTimeSeconds();
        fn();
        return threadCpuTimeSeconds() - start;
    };

    constexpr double kAudio = 8.0;
    constexpr int kBlock = 128;
    const int blocks = (int) (kAudio * kSr / kBlock);

    // The kit with every piece ringing.
    auto measureKit = [&]
    {
        JamDrumKit kit;
        kit.prepare (kSr, kBlock);
        std::vector<double> l (kBlock), r (kBlock);
        const DrumSound all[] = { DrumSound::kick, DrumSound::snare, DrumSound::tomHigh, DrumSound::tomMid,
                                  DrumSound::tomFloor, DrumSound::hatOpen, DrumSound::ride, DrumSound::crash,
                                  DrumSound::rim, DrumSound::shaker };

        return 100.0 * seconds ([&]
        {
            for (int i = 0; i < blocks; ++i)
            {
                // Every piece struck once a second: each one's modes are
                // ringing all the time (5's "all pieces ringing, ~200 modes"),
                // one voice per piece.
                if (i % 375 == 0)
                    for (auto s : all)
                        kit.trigger (s, 0.9);

                kit.render (l.data(), r.data(), kBlock);
            }
        }) / kAudio;
    };

    auto measureBass = [&]
    {
        JamBassVoice bass;
        bass.prepare (kSr, kBlock);
        std::vector<double> x (kBlock);

        return 100.0 * seconds ([&]
        {
            for (int i = 0; i < blocks; ++i)
            {
                if (i % 47 == 0)
                    bass.noteOn (28 + (i / 47) % 20, 0.8, false);

                bass.render (x.data(), kBlock);
            }
        }) / kAudio;
    };

    auto measureTotal = [&]
    {
        JamBench b (kSr, kBlock);
        b.host = b.hostPlaying = true;
        b.settings.intensity = 5;
        b.settings.style = 5;

        for (int i = 0; i < 10; ++i)
            b.chord (i * 96000, { 45, 48, 52 });

        return 100.0 * seconds ([&] { b.runSeconds (kAudio); }) / kAudio;
    };

    auto measureArmed = [&]
    {
        JamBench b (kSr, kBlock);
        return 100.0 * seconds ([&] { b.runSeconds (kAudio); }) / kAudio;
    };

    /*  performance-budget.md's units are for its reference CPU, which CI is
        not. The yardstick is the table's own StringEngine row - 12 strings are
        2.5 units, one is 0.208 - measured here on this machine: every figure
        below is converted to reference units by that ratio. */
    auto measureString = [&]
    {
        StringEngine string;
        string.prepare (kSr, kBlock);
        Excitation::Params p;
        p.velocity = 0.8;

        return 100.0 * seconds ([&]
        {
            double sink = 0.0;

            for (int i = 0; i < blocks; ++i)
            {
                if (i % 94 == 0)
                {
                    string.snapToFrequency (110.0 + (i / 94) % 12 * 10.0);
                    string.excite (p);
                }

                for (int k = 0; k < kBlock; ++k)
                    sink += string.processSample (0.0);
            }

            juce::ignoreUnused (sink);
        }) / kAudio;
    };

    // Interleaved, the best of three each: a busy machine only ever adds time,
    // and interleaving keeps it from landing on one measurement alone.
    double kitUnits = 1.0e9, bassUnits = 1.0e9, totalUnits = 1.0e9, armedUnits = 1.0e9, stringUnits = 1.0e9;

    for (int round = 0; round < 3; ++round)
    {
        stringUnits = juce::jmin (stringUnits, measureString());
        kitUnits = juce::jmin (kitUnits, measureKit());
        bassUnits = juce::jmin (bassUnits, measureBass());
        totalUnits = juce::jmin (totalUnits, measureTotal());
        armedUnits = juce::jmin (armedUnits, measureArmed());
    }

    const double scale = (2.5 / 12.0) / juce::jmax (1.0e-6, stringUnits);
    std::cout << "    Jam cost (% of one core here, 48 kHz / 128): kit " << kitUnits << ", bass " << bassUnits
              << ", total " << totalUnits << ", armed " << armedUnits << "; one StringEngine " << stringUnits
              << " (0.208 reference units), so x" << scale << std::endl;

    CHECK_MSG (kitUnits * scale <= 0.9 * 1.10, "JamDrumKit " + juce::String (kitUnits * scale, 3) + " reference units");
    CHECK_MSG (bassUnits * scale <= 0.6 * 1.10, "JamBassVoice " + juce::String (bassUnits * scale, 3) + " reference units");
    CHECK_MSG (totalUnits * scale <= 1.7 * 1.10, "Jam total " + juce::String (totalUnits * scale, 3) + " reference units");
    CHECK_MSG (armedUnits * scale <= 0.02 * 1.10, "armed and stopped " + juce::String (armedUnits * scale, 4) + " reference units");
}


