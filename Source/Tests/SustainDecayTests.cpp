/*  Sustain and decay (sustain-and-decay.md 11), group SustainDecay.

    Most tests drive one StringEngine, built the way LuthierEngine builds it
    (StringMaterials, nickel-plated steel, regular gauge, 648 mm), so there is
    no body, no pickup and no amp between the string and the measurement. The
    legacy and style tests run the whole engine or the processor.

    Pitch is measured by autocorrelation (parabolic peak, so a cent is well
    inside its resolution); the envelope is RMS over 20 ms windows.
*/

#include "TestFramework.h"

#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../Presets/RealismStyles.h"
#include "../Presets/RealismStyleActions.h"
#include "../Model/Guitar/StringMaterials.h"

#include <cstdio>

using namespace luthier;
using namespace luthier::tests;

namespace luthier::tests { long realismCAllocationCount() noexcept; }

namespace
{
    // Standard tuning, engine order: 0 = high E.
    constexpr double kOpenHz[6] = { 329.628, 246.942, 195.998, 146.832, 110.0, 82.407 };

    struct TestString
    {
        TestString (int index, double sampleRate = 48000.0, StringEngine::SustainShape shape = {})
            : sr (sampleRate), stringIndex (index)
        {
            str = std::make_unique<StringEngine>();
            str->setIndex (index);
            str->prepare (sr, 128);

            const auto spec = StringMaterials::computeSpec (StringMaterial::NickelPlatedSteel, StringGauge::Regular,
                                                            StringAge::BrokenIn, index, kOpenHz[index], 648.0);
            auto physical = StringMaterials::toPhysical (spec, 648.0);
            physical.coreDiameterMm = spec.coreDiameterMm;
            physical.tensionNewtons = spec.tensionNewtons;
            physical.youngsModulus = StringMaterials::get (StringMaterial::NickelPlatedSteel).youngsModulusPa;
            str->setPhysical (physical);
            str->setSustainShape (shape);
            str->snapToFrequency (kOpenHz[index]);
            str->reset();
        }

        void pluck (double velocity, double fret = 0.0, Excitation::Kind kind = Excitation::Kind::Pluck)
        {
            const double hz = kOpenHz[stringIndex] * std::pow (2.0, fret / 12.0);
            str->snapToFrequency (hz);
            str->setTargetFrequency (hz);
            str->setStoppedFret (fret);
            str->setDamping (StringEngine::Damping::Open, 1.0);

            Excitation::Params p;
            p.velocity = velocity;
            p.kind = kind;
            str->excite (p);
        }

        std::vector<double> run (double seconds)
        {
            std::vector<double> out;
            const int n = (int) (seconds * sr);
            out.reserve ((size_t) n);

            for (int i = 0; i < n; ++i)
                out.push_back (str->processSample (0.0));

            return out;
        }

        double sr;
        int stringIndex;
        std::unique_ptr<StringEngine> str;
    };

    StringEngine::SustainShape shapeWith (std::function<void (StringEngine::SustainShape&)> f)
    {
        StringEngine::SustainShape s;
        f (s);
        return s;
    }

    /** RMS in dB over a window. */
    double levelDb (const std::vector<double>& x, double sr, double from, double length = 0.020)
    {
        const int a = juce::jlimit (0, (int) x.size() - 1, (int) (from * sr));
        const int n = juce::jlimit (1, (int) x.size() - a, (int) (length * sr));
        return gainToDb (juce::jmax (1.0e-15, rms (x.data() + a, n)));
    }

    /** The late line: a least-squares fit of the 20 ms envelope in dB. Returns
        slope in dB/s and the intercept at t = 0. */
    std::pair<double, double> fitLine (const std::vector<double>& x, double sr, double from, double to)
    {
        double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
        int n = 0;

        for (double t = from; t + 0.020 <= to; t += 0.020, ++n)
        {
            const double y = levelDb (x, sr, t);
            const double tc = t + 0.010;
            sx += tc; sy += y; sxx += tc * tc; sxy += tc * y;
        }

        const double slope = (n * sxy - sx * sy) / (n * sxx - sx * sx);
        return { slope, (sy - slope * sx) / n };
    }

    /** Pitch by normalised autocorrelation near `expectedHz`, in Hz. */
    double pitchAt (const std::vector<double>& x, double sr, double from, double length, double expectedHz)
    {
        const int a = (int) (from * sr);
        const int n = (int) (length * sr);
        const double expectedLag = sr / expectedHz;
        const int lo = juce::jmax (2, (int) (expectedLag * 0.94)), hi = (int) (expectedLag * 1.06) + 1;

        std::vector<double> r ((size_t) (hi + 2), 0.0);

        for (int lag = lo - 1; lag <= hi + 1; ++lag)
        {
            double xy = 0.0, xx = 0.0, yy = 0.0;

            for (int i = a; i < a + n - lag && i + lag < (int) x.size(); ++i)
            {
                xy += x[(size_t) i] * x[(size_t) (i + lag)];
                xx += x[(size_t) i] * x[(size_t) i];
                yy += x[(size_t) (i + lag)] * x[(size_t) (i + lag)];
            }

            r[(size_t) lag] = xy / std::sqrt (juce::jmax (1.0e-30, xx * yy));
        }

        int best = lo;
        for (int lag = lo; lag <= hi; ++lag)
            if (r[(size_t) lag] > r[(size_t) best]) best = lag;

        const double y0 = r[(size_t) best - 1], y1 = r[(size_t) best], y2 = r[(size_t) best + 1];
        const double denom = y0 - 2.0 * y1 + y2;
        const double offset = std::abs (denom) > 1.0e-12 ? 0.5 * (y0 - y2) / denom : 0.0;
        return sr / ((double) best + offset);
    }

    double cents (double hz, double reference) { return 1200.0 * std::log2 (hz / reference); }

    /** Spectral centroid over a window, by a direct DFT at 50 Hz steps. */
    double centroid (const std::vector<double>& x, double sr, double from, double length)
    {
        const int a = (int) (from * sr), n = (int) (length * sr);
        double num = 0.0, den = 0.0;

        for (double f = 50.0; f < 12000.0; f += 50.0)
        {
            double re = 0.0, im = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double w = 0.5 - 0.5 * std::cos (constants::kTwoPi * i / (n - 1));
                re += x[(size_t) (a + i)] * w * std::cos (constants::kTwoPi * f * i / sr);
                im += x[(size_t) (a + i)] * w * std::sin (constants::kTwoPi * f * i / sr);
            }
            const double m = re * re + im * im;
            num += f * m;
            den += m;
        }

        return num / juce::jmax (1.0e-30, den);
    }

    /** Energy between two frequencies over a window, by a direct DFT. */
    double bandEnergy (const std::vector<double>& x, double sr, double from, double length, double lo, double hi)
    {
        const int a = (int) (from * sr), n = (int) (length * sr);
        double e = 0.0;

        for (double f = lo; f < hi; f += 1.0 / length)
        {
            double re = 0.0, im = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double w = 0.5 - 0.5 * std::cos (constants::kTwoPi * i / (n - 1));
                re += x[(size_t) (a + i)] * w * std::cos (constants::kTwoPi * f * i / sr);
                im += x[(size_t) (a + i)] * w * std::sin (constants::kTwoPi * f * i / sr);
            }
            e += re * re + im * im;
        }

        return e;
    }

    /** The frequency of the largest spectral peak above `minHz`. */
    double largestPeakAbove (const std::vector<double>& x, double sr, double minHz, double from, double length)
    {
        const int a = (int) (from * sr), n = (int) (length * sr);
        double best = 0.0, bestHz = 0.0;

        for (double f = minHz; f < juce::jmin (12000.0, sr * 0.45); f += 10.0)
        {
            double re = 0.0, im = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double w = 0.5 - 0.5 * std::cos (constants::kTwoPi * i / (n - 1));
                re += x[(size_t) (a + i)] * w * std::cos (constants::kTwoPi * f * i / sr);
                im += x[(size_t) (a + i)] * w * std::sin (constants::kTwoPi * f * i / sr);
            }
            if (re * re + im * im > best) { best = re * re + im * im; bestHz = f; }
        }

        return bestHz;
    }

    std::vector<double> minus (const std::vector<double>& a, const std::vector<double>& b)
    {
        std::vector<double> d (a.size());
        for (size_t i = 0; i < a.size(); ++i) d[i] = a[i] - b[i];
        return d;
    }

    /** SUS-04's measurement: cents sharp 50-90 ms after a pluck, v against v=10/127. */
    double tensionOffset (int stringIndex, double velocity, double sr = 48000.0, double atTime = 0.050)
    {
        const auto shape = shapeWith ([] (auto& s) { s.tensionMod = 1.0; });
        TestString hard (stringIndex, sr, shape), soft (stringIndex, sr, shape);
        hard.pluck (velocity);
        soft.pluck (10.0 / 127.0);
        const auto h = hard.run (atTime + 0.1), s = soft.run (atTime + 0.1);
        return cents (pitchAt (h, sr, atTime, 0.040, kOpenHz[stringIndex]),
                      pitchAt (s, sr, atTime, 0.040, kOpenHz[stringIndex]));
    }
}

//==============================================================================
// SUS-01
LUTHIER_TEST (SustainDecay, legacyIsBitIdentical)
{
    auto run = [] (bool bypass)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 128);
        engine.setGuitarType (GuitarType::Stratocaster);
        engine.setSustainShapeBypassedForTest (bypass);
        engine.reset();

        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, 128);

        for (int b = 0; b < (int) (10.0 * 48000.0 / 128); ++b)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            const int step = b % 375;
            const int note = 45 + (b / 375) % 10;

            if (step == 0)   midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), 0);
            if (step == 40)  midi.addEvent (juce::MidiMessage::noteOn (1, note + 2, (juce::uint8) 90), 3);   // legato
            if (step == 80)  midi.addEvent (juce::MidiMessage::pitchWheel (1, 12000), 0);                     // bend
            if (step == 120) midi.addEvent (juce::MidiMessage::pitchWheel (1, 8192), 0);
            if (step == 150) midi.addEvent (juce::MidiMessage::controllerEvent (1, 67, 127), 0);              // palm mute
            if (step == 160) midi.addEvent (juce::MidiMessage::noteOn (1, note - 5, (juce::uint8) 100), 0);
            if (step == 200) midi.addEvent (juce::MidiMessage::controllerEvent (1, 67, 0), 0);
            if (step == 250) midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);                           // note-offs
            if (step == 250) for (int k = note - 5; k <= note + 2; ++k) midi.addEvent (juce::MidiMessage::noteOff (1, k), 1);

            engine.processBlock (buffer, midi);

            for (int i = 0; i < 128; ++i)
                out.push_back (buffer.getSample (0, i));
        }

        return out;
    };

    CHECK (run (false) == run (true));
}

//==============================================================================
// SUS-02
LUTHIER_TEST (SustainDecay, twoStageKnee)
{
    const double sr = 48000.0;
    TestString legacy (4, sr);
    TestString shaped (4, sr, shapeWith ([] (auto& s) { s.fastShare = 0.5; s.fastRatio = 0.2; }));
    legacy.pluck (0.8);
    shaped.pluck (0.8);
    const auto l = legacy.run (3.2), s = shaped.run (3.2);

    const double early = fitLine (s, sr, 0.05, 0.25).first;
    const double late = fitLine (s, sr, 2.0, 3.0).first;
    const double legacyLate = fitLine (l, sr, 2.0, 3.0).first;

    std::printf ("      slopes: early %.1f dB/s, late %.1f dB/s, legacy late %.1f dB/s\n", early, late, legacyLate);
    /*  The spec asked for 3x. Its own envelope E(t) cannot give that: with
        a = 0.5, rho = 0.2 and a 4.4 s T60 the energy falls from -1.8 dB at
        50 ms to -6.2 dB at 250 ms, 22 dB/s against the late 13.7 dB/s, which
        is 1.6x. The measured ratio is 1.8x; the threshold is 1.5x
        (sustain-and-decay.md 11 corrected). */
    CHECK_MSG (early <= 1.5 * late, "the early slope is not 1.5x the late one");
    CHECK_MSG (std::abs (late / legacyLate - 1.0) <= 0.10, "the late slope moved from the legacy one");
}

//==============================================================================
// SUS-03
LUTHIER_TEST (SustainDecay, kneeDepth)
{
    const double sr = 48000.0;
    TestString legacy (4, sr);
    legacy.pluck (0.8);
    const auto l = legacy.run (3.2);
    const double legacyIntercept = fitLine (l, sr, 2.0, 3.0).second;

    for (double a : { 0.3, 0.5, 0.8 })
    {
        TestString shaped (4, sr, shapeWith ([a] (auto& s) { s.fastShare = a; }));
        shaped.pluck (0.8);
        const auto s = shaped.run (3.2);
        const double depth = fitLine (s, sr, 2.0, 3.0).second - legacyIntercept;
        const double expected = 10.0 * std::log10 (1.0 - a);
        std::printf ("      a %.1f: knee %.2f dB (expected %.2f)\n", a, depth, expected);
        CHECK_MSG (std::abs (depth - expected) <= 1.0, "a = " + juce::String (a) + ": " + juce::String (depth, 2) + " dB");
    }
}

//==============================================================================
// SUS-04
LUTHIER_TEST (SustainDecay, tensionMagnitude)
{
    const double lowE = tensionOffset (5, 1.0);
    const double highE = tensionOffset (0, 1.0);
    std::printf ("      tension at v127, 50-90 ms: low E %.2f c, high E %.2f c\n", lowE, highE);
    CHECK_MSG (lowE >= 5.0 && lowE <= 12.0, "low E " + juce::String (lowE, 2) + " cents");
    CHECK_MSG (highE < 2.0, "high E " + juce::String (highE, 2) + " cents");

    const double settled = tensionOffset (5, 1.0, 48000.0, 1.5);
    CHECK_MSG (std::abs (settled) < 1.0, "after 1.5 s: " + juce::String (settled, 2) + " cents");
}

//==============================================================================
// SUS-05
LUTHIER_TEST (SustainDecay, tensionScalesWithLevelSquared)
{
    const double ratio = tensionOffset (5, 1.0) / tensionOffset (5, 64.0 / 127.0);
    std::printf ("      v127 / v64 offset ratio %.2f\n", ratio);
    /*  The spec's [3, 5] assumed level linear in velocity. The excitation's
        loudness law is 0.1 + 0.9 v^1.45 (Excitation.cpp), so level(127) /
        level(64) is 2.3 and its square 5.3; the window is [4, 6.5]. */
    CHECK_MSG (ratio >= 4.0 && ratio <= 6.5, "ratio " + juce::String (ratio, 2));
}

//==============================================================================
// SUS-06
LUTHIER_TEST (SustainDecay, brightnessOvershoot)
{
    /*  The spec measured the spectral centroid. The string's own output is
        dominated by its fundamental (a centroid near 200 Hz on the open A), so
        no change to the upper partials moves the centroid by more than about
        1%. The overshoot acts on the upper partials, so that is what is
        measured: 2-8 kHz energy over 0-30 ms at least 1.2 dB (15% in
        amplitude) above the A = 0 render; and over 300-330 ms the centroid
        within 3% of it (sustain-and-decay.md 11 corrected). */
    const double sr = 48000.0;
    TestString plain (4, sr), shaped (4, sr, shapeWith ([] (auto& s) { s.attackTransient = 1.0; s.attackTimeSeconds = 0.030; }));
    plain.pluck (0.9);
    shaped.pluck (0.9);
    const auto p = plain.run (0.4), s = shaped.run (0.4);

    const double early = 10.0 * std::log10 (bandEnergy (s, sr, 0.0, 0.030, 2000.0, 8000.0)
                                            / bandEnergy (p, sr, 0.0, 0.030, 2000.0, 8000.0));
    const double late = centroid (s, sr, 0.300, 0.030) / centroid (p, sr, 0.300, 0.030);
    std::printf ("      2-8 kHz over 0-30 ms +%.2f dB; centroid ratio 300-330 ms %.3f\n", early, late);
    CHECK_MSG (early >= 1.2, "the attack's upper partials are only " + juce::String (early, 2) + " dB up");
    CHECK_MSG (std::abs (late - 1.0) <= 0.03, "the tail is " + juce::String ((late - 1.0) * 100.0, 1) + "% off");
}

//==============================================================================
// SUS-07 (and SUS-15's ping at 44.1 and 96 kHz)
LUTHIER_TEST (SustainDecay, longitudinalPing)
{
    for (double sr : { 48000.0, 44100.0, 96000.0 })
    {
        auto pingHz = [sr] (int stringIndex, double fret)
        {
            // The attack time at its advanced minimum, 1 ms, so the brightness
            // half of the transient is out of the difference and the ping is
            // what is left of it.
            TestString plain (stringIndex, sr), shaped (stringIndex, sr, shapeWith ([] (auto& s) { s.attackTransient = 1.0; s.attackTimeSeconds = 0.001; }));
            plain.pluck (1.0, fret);
            shaped.pluck (1.0, fret);
            const auto d = minus (shaped.run (0.06), plain.run (0.06));
            return largestPeakAbove (d, sr, 1000.0, 0.0, 0.05);
        };

        const double highE = pingHz (0, 0.0), lowE = pingHz (5, 0.0), lowE12 = pingHz (5, 12.0);
        std::printf ("      %g Hz: ping high E %.0f Hz, low E %.0f Hz, low E at 12 %.0f Hz\n", sr, highE, lowE, lowE12);

        CHECK_MSG (std::abs (highE / 3900.0 - 1.0) <= 0.10, "plain high E ping at " + juce::String (highE));
        CHECK_MSG (std::abs (lowE / 2000.0 - 1.0) <= 0.15, "wound low E ping at " + juce::String (lowE));
        CHECK_MSG (std::abs (lowE12 / (2.0 * lowE) - 1.0) <= 0.10, "fret 12 ping at " + juce::String (lowE12));

        // SUS-15: within 3% of the 48 kHz answer, which is the string's f_L.
        TestString probe (0, sr);
        CHECK_MSG (std::abs (highE / probe.str->getLongitudinalHz() - 1.0) <= 0.03, "the ping moved with the rate");
    }
}

//==============================================================================
// SUS-08
LUTHIER_TEST (SustainDecay, releaseRamp)
{
    const double sr = 48000.0;
    auto render = [sr] (double releaseSeconds)
    {
        auto shape = shapeWith ([releaseSeconds] (auto& s) { s.releaseSeconds = releaseSeconds; s.tensionMod = 1.0e-9; });
        TestString t (0, sr, shape);
        t.pluck (0.9, 12.0);
        auto out = t.run (0.5);
        t.str->release (false, 12.0);
        const auto after = t.run (0.4);
        out.insert (out.end(), after.begin(), after.end());
        return out;
    };

    const auto legacy = render (0.0), ramped = render (0.040);
    const double atOff = levelDb (legacy, sr, 0.48);
    const double legacy10 = levelDb (legacy, sr, 0.505), ramped10 = levelDb (ramped, sr, 0.505);
    const double legacy250 = levelDb (legacy, sr, 0.75), ramped250 = levelDb (ramped, sr, 0.75);

    std::printf ("      at note-off %.1f; +10 ms legacy %.1f ramped %.1f; +250 ms legacy %.1f ramped %.1f dB\n",
                 atOff, legacy10, ramped10, legacy250, ramped250);
    CHECK_MSG (ramped10 - legacy10 >= 6.0, "10 ms after the release the ramp is only " + juce::String (ramped10 - legacy10, 1) + " dB louder");
    CHECK (legacy250 < atOff - 40.0);
    CHECK (ramped250 < atOff - 40.0);
}

//==============================================================================
// SUS-09
LUTHIER_TEST (SustainDecay, releaseSag)
{
    /*  The released damping lowers the loop filter's cutoff, and the filter's
        group delay moves the pitch a few cents on its own (the legacy release
        does too). So the sag is measured against the same release with d = 0,
        which is the sag alone. Over 10-30 ms of a 30 ms glide it averages
        two-thirds of the -17.7 c target. */
    const double sr = 48000.0;

    auto after = [sr] (double fret, double sagMm)
    {
        TestString t (4, sr, shapeWith ([sagMm] (auto& s) { s.releaseSeconds = 0.030; s.releaseSagMm = sagMm; s.tensionMod = 1.0e-9; }));
        t.pluck (0.9, fret);
        t.run (0.3);
        t.str->release (false, fret);
        const auto out = t.run (0.05);
        const double hz = kOpenHz[4] * std::pow (2.0, fret / 12.0);
        return pitchAt (out, sr, 0.010, 0.020, hz);
    };

    const double sag = cents (after (5.0, 5.0), after (5.0, 0.0));
    std::printf ("      fret 5 sag %.2f cents\n", sag);
    CHECK_MSG (sag <= -10.0 && sag >= -25.0, "sag " + juce::String (sag, 2) + " cents");

    const double openSag = cents (after (0.0, 5.0), after (0.0, 0.0));
    CHECK_MSG (std::abs (openSag) < 1.0, "the open string sagged " + juce::String (openSag, 2));
}

//==============================================================================
// SUS-10
LUTHIER_TEST (SustainDecay, releaseRing)
{
    const double sr = 48000.0;
    TestString t (4, sr, shapeWith ([] (auto& s) { s.releaseRing = 0.3; }));
    t.pluck (0.9, 7.0);
    const auto before = t.run (0.5);
    t.str->release (false, 7.0);
    const auto after = t.run (0.2);

    const double hz = pitchAt (after, sr, 0.080, 0.040, kOpenHz[4]);
    /*  The level is read at 20-40 ms, once the open period that passed the
        gain R has come round (the pitch is read at 100 ms as the spec says).
        By 100 ms the light touch's own decay (T60 x 0.61, cutoff toward
        2 kHz) has taken about 1.6 dB more, which is that damping working,
        not the ring's size (sustain-and-decay.md 11 corrected). */
    const double drop = levelDb (after, sr, 0.020) - levelDb (before, sr, 0.48);
    std::printf ("      ring: %.2f cents off the open string, %.1f dB\n", cents (hz, kOpenHz[4]), drop);
    CHECK_MSG (std::abs (cents (hz, kOpenHz[4])) <= 5.0, "the ring is not the open string");
    CHECK_MSG (std::abs (drop + 10.5) <= 2.0, "the ring is " + juce::String (drop, 1) + " dB");
}

//==============================================================================
// SUS-11
LUTHIER_TEST (SustainDecay, boundedAtTheLimits)
{
    const auto shape = shapeWith ([] (auto& s)
    {
        s.tensionMod = 6.0; s.attackTransient = 3.0; s.fastShare = 0.99; s.fastRatio = 0.01;
        s.releaseSeconds = 0.3; s.releaseSagMm = 20.0; s.releaseRing = 1.0; s.advanced = true;
    });

    LuthierEngine engine;
    engine.prepare (48000.0, 128);
    engine.setGuitarType (GuitarType::TwelveString);
    engine.setSustainShape (shape);
    engine.reset();

    juce::AudioBuffer<float> buffer (2, 128);
    bool finite = true;
    double worstCents = 0.0, worstGain = 0.0;

    for (int b = 0; b < (int) (60.0 * 48000.0 / 128); ++b)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (b % 100 == 0)
            for (int s = 0; s < engine.getNumStrings(); ++s)
            {
                NoteOnEvent e;
                e.stringIndex = s;
                e.fretPosition = (double) ((b / 100 + s) % 12);
                e.pitchHz = engine.getTuningEngine().computeFrequency (s, e.fretPosition);
                e.velocity = 1.0;
                engine.triggerNoteNow (e);
            }

        engine.processBlock (buffer, midi);

        for (int i = 0; i < 128; ++i)
            finite = finite && std::isfinite (buffer.getSample (0, i));

        for (int s = 0; s < engine.getNumStrings(); ++s)
        {
            worstCents = juce::jmax (worstCents, engine.getString (s).getTensionCents());
            worstGain = juce::jmax (worstGain, engine.getString (s).getLoopGain());
        }
    }

    CHECK (finite);
    CHECK_MSG (worstCents <= 50.0 + 1.0e-6, "pitch offset " + juce::String (worstCents, 2));
    CHECK_MSG (worstGain <= 0.9995, "loop gain " + juce::String (worstGain, 6));
}

//==============================================================================
// SUS-12
LUTHIER_TEST (SustainDecay, styles)
{
    LuthierAudioProcessor processor;
    auto& state = processor.getState();

    auto get = [&state] (const char* id)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id));
        return (double) p->convertFrom0to1 (p->getValue());
    };

    applySustainStyle (processor, 1);
    const auto& row = RealismStyles::getSustainStyle (1);
    CHECK_NEAR (get (ParamIDs::sustainAttackTransient), row.transient, 1.0e-4);
    CHECK_NEAR (get (ParamIDs::sustainAttackTime), row.attackMs, 1.0e-3);
    CHECK_NEAR (get (ParamIDs::sustainFastShare), row.fastShare, 1.0e-4);
    CHECK_NEAR (get (ParamIDs::sustainFastRatio), row.fastRatio, 1.0e-4);
    CHECK_NEAR (get (ParamIDs::sustainTensionMod), row.tension, 1.0e-4);
    CHECK_NEAR (get (ParamIDs::sustainReleaseTime), row.releaseMs, 1.0e-3);
    CHECK_NEAR (get (ParamIDs::sustainReleaseSag), row.sagMm, 1.0e-4);
    CHECK_NEAR (get (ParamIDs::sustainReleaseRing), row.ring, 1.0e-4);
    CHECK (describeSustainStyle (processor) == "Electric, natural");

    if (auto* p = state.getParameter (ParamIDs::sustainReleaseSag))
        p->setValueNotifyingHost (0.9f);

    CHECK (describeSustainStyle (processor) == "Electric, natural (modified)");

    applySustainStyle (processor, 0);

    for (auto* id : { ParamIDs::sustainAttackTransient, ParamIDs::sustainAttackTime, ParamIDs::sustainFastShare,
                      ParamIDs::sustainFastRatio, ParamIDs::sustainTensionMod, ParamIDs::sustainReleaseTime,
                      ParamIDs::sustainReleaseSag, ParamIDs::sustainReleaseRing })
    {
        auto* p = state.getParameter (id);
        CHECK_MSG (std::abs (p->getValue() - p->getDefaultValue()) < 1.0e-6, juce::String (id) + " is not back at its default");
    }

    // ... so SUS-01 holds: the bridge hands the strings a neutral shape.
    processor.prepareToPlay (48000.0, 128);
    processor.getParameterBridge().applyToEngine();
    CHECK (processor.getEngine().getSustainShape().isNeutral());
}

//==============================================================================
// SUS-13
LUTHIER_TEST (SustainDecay, lettingRingSkipsTheRelease)
{
    const double sr = 48000.0;
    auto render = [sr] (bool withRelease)
    {
        auto shape = shapeWith ([withRelease] (auto& s)
        {
            s.fastShare = 0.5;
            if (withRelease) { s.releaseSeconds = 0.04; s.releaseSagMm = 5.0; s.releaseRing = 0.3; }
        });

        TestString t (4, sr, shape);
        t.pluck (0.9, 5.0);
        auto out = t.run (0.3);
        t.str->release (true, 5.0);   // the sustain pedal, or an E-Bow holding it
        const auto after = t.run (0.3);
        out.insert (out.end(), after.begin(), after.end());
        return out;
    };

    CHECK (render (true) == render (false));
}

//==============================================================================
// SUS-14
LUTHIER_TEST (SustainDecay, costAndSafety)
{
    auto run = [] (bool shaped, bool countAllocations)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 128);
        engine.setGuitarType (GuitarType::TwelveString);

        if (shaped)
            engine.setSustainShape (shapeWith ([] (auto& s)
            {
                s.attackTransient = 1.0; s.fastShare = 0.7; s.tensionMod = 1.0;
                s.releaseSeconds = 0.02; s.releaseSagMm = 5.0; s.releaseRing = 0.05;
            }));

        engine.reset();
        juce::AudioBuffer<float> buffer (2, 128);
        juce::MidiBuffer midi;
        const long before = realismCAllocationCount();
        const auto t0 = juce::Time::getMillisecondCounterHiRes();

        for (int b = 0; b < (int) (10.0 * 48000.0 / 128); ++b)
        {
            if (b % 150 == 0)
                for (int s = 0; s < engine.getNumStrings(); ++s)
                {
                    NoteOnEvent e;
                    e.stringIndex = s;
                    e.fretPosition = 3.0;
                    e.pitchHz = engine.getTuningEngine().computeFrequency (s, 3.0);
                    e.velocity = 1.0;
                    engine.triggerNoteNow (e);
                }

            buffer.clear();
            engine.processBlock (buffer, midi);
        }

        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        return countAllocations ? (double) (realismCAllocationCount() - before) : ms;
    };

    CHECK_MSG (run (true, true) == 0.0, "the shape allocated on the audio path");

    // Everything below is a machine-relative CPU budget (measured, then asserted): run under
    // LUTHIER_PERF=1 (the nightly) only. The allocation check above runs everywhere.
    if (! luthier::tests::perfRunRequested())
        return;

    // A unit is one real-time core at 48 kHz: 10 s of audio is 10000 ms of it.
    const double units = (run (true, false) - run (false, false)) / 10000.0;
    std::printf ("      shape cost %.3f units over the legacy strings\n", units);
    CHECK_MSG (units <= 0.3, "the shape costs " + juce::String (units, 3) + " units");
}

//==============================================================================
// SUS-15: the knee time and SUS-04's offset at 44.1 and 96 kHz.
LUTHIER_TEST (SustainDecay, sampleRateIndependence)
{
    auto kneeTime = [] (double sr)
    {
        TestString legacy (4, sr);
        TestString shaped (4, sr, shapeWith ([] (auto& s) { s.fastShare = 0.5; }));
        legacy.pluck (0.8);
        shaped.pluck (0.8);
        const auto l = legacy.run (3.2), s = shaped.run (3.2);
        const auto late = fitLine (s, sr, 2.0, 3.0);

        // The knee: the first t at which the envelope is within 1 dB of the late line.
        for (double t = 0.02; t < 2.0; t += 0.01)
            if (levelDb (s, sr, t) - (late.second + late.first * (t + 0.01)) < 1.0)
                return t;

        return 2.0;
    };

    const double k48 = kneeTime (48000.0);
    const double off48 = tensionOffset (5, 1.0);

    for (double sr : { 44100.0, 96000.0 })
    {
        const double k = kneeTime (sr);
        CHECK_MSG (std::abs (k / k48 - 1.0) <= 0.05, juce::String (sr) + ": knee at " + juce::String (k, 3) + " s, "
                                                       + juce::String (k48, 3) + " s at 48 kHz");
        CHECK_MSG (std::abs (tensionOffset (5, 1.0, sr) - off48) <= 1.0, juce::String (sr) + ": the tension offset moved");
    }
}

