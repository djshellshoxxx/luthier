/*  harmonic-realism.md 9 (REALISM-B): a harmonic is a touch, not a filter.

    Measured on the string's own output (the per-string taps, routing-io 3),
    so the body, the amp and the pickups play no part: the partials seen are
    the string's. Character is off, so no dead spot or drift sits on a
    partial; everything here is deterministic.
*/

#include "TestFramework.h"

#include "../DSP/String/Harmonics.h"
#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../PhysicalRange.h"

using namespace luthier;
using namespace luthier::tests;

long luthierAllocationsOnThisThread() noexcept;   // CircuitTests.cpp

namespace
{
    constexpr int kBlock = 256;

    // Standard tuning, engine order.
    constexpr int kHighE = 0, kB = 1, kA = 4, kLowE = 5;

    struct Rig
    {
        explicit Rig (double sampleRate = 48000.0, GuitarType type = GuitarType::Stratocaster)
            : sr (sampleRate)
        {
            engine.prepare (sr, kBlock);
            engine.setGuitarType (type);
            engine.getCharacterEngine().setEnabled (false);
            engine.getTapBuffers().setPerStringWanted (true);
            engine.reset();
        }

        NoteOnEvent note (int s, double stoppedFret, double touchFret, Technique t, double velocity = 0.8)
        {
            NoteOnEvent e;
            e.stringIndex = s;
            e.midiNote = 40;
            e.fretPosition = stoppedFret;
            e.touchFret = touchFret;
            e.technique = t;
            e.velocity = velocity;
            e.pitchHz = engine.getTuningEngine().computeFrequency (s, stoppedFret, 0.0);

            if (touchFret >= 0.0)
                e.harmonicPartial = harmonics::findNode (harmonics::touchFractionFromBridge (touchFret, stoppedFret),
                                                         648.0, 2.5).partial;
            return e;
        }

        /** Renders, recording string `s`; `each (block)` runs before each block. */
        template <typename Each>
        void run (double seconds, int s, Each each)
        {
            juce::AudioBuffer<float> buffer (2, kBlock);
            juce::MidiBuffer midi;

            for (int b = 0; b < (int) (seconds * sr / kBlock); ++b)
            {
                each (b);
                buffer.clear();
                engine.processBlock (buffer, midi);

                const float* own = engine.getTapBuffers().stringRead (s);

                for (int i = 0; i < kBlock; ++i)
                    out.push_back ((double) own[i]);
            }
        }

        void run (double seconds, int s) { run (seconds, s, [] (int) {}); }

        double sr;
        LuthierEngine engine;
        std::vector<double> out;
    };

    /** Hann-windowed DFT magnitude at `hz`. */
    double dft (const std::vector<double>& x, size_t from, size_t n, double hz, double sr)
    {
        double re = 0.0, im = 0.0;

        for (size_t i = 0; i < n && from + i < x.size(); ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (double) i / (double) (n - 1));
            const double ph = juce::MathConstants<double>::twoPi * hz * (double) i / sr;
            re += x[from + i] * w * std::cos (ph);
            im -= x[from + i] * w * std::sin (ph);
        }

        return std::sqrt (re * re + im * im);
    }

    /** The strongest level within +/-1.5 % of `hz`, in dB, over [t, t + len). */
    double levelDb (const std::vector<double>& x, double t, double len, double hz, double sr)
    {
        const auto from = (size_t) (t * sr), n = (size_t) (len * sr);
        double best = 1.0e-30;

        for (double f = hz * 0.985; f <= hz * 1.015; f += hz * 0.0015)
            best = juce::jmax (best, dft (x, from, n, f, sr));

        return juce::Decibels::gainToDecibels (best, -400.0);
    }

    /** The frequency of the peak near `hz`, refined to a hundredth of a Hz. */
    double pitchNear (const std::vector<double>& x, double t, double len, double hz, double sr)
    {
        const auto from = (size_t) (t * sr), n = (size_t) (len * sr);
        double best = hz, bestMag = -1.0;

        for (double step : { hz * 0.002, 0.2, 0.02, 0.002 })
        {
            const double centre = best;
            const double span = (step == hz * 0.002) ? hz * 0.04 : step * 12.0;

            for (double f = centre - span; f <= centre + span; f += step)
            {
                const double m = dft (x, from, n, f, sr);

                if (m > bestMag)
                {
                    bestMag = m;
                    best = f;
                }
            }
        }

        return best;
    }

    double cents (double a, double b) { return 1200.0 * std::log2 (a / b); }

    double rmsOf (const std::vector<double>& x, double t0, double t1, double sr)
    {
        double sum = 0.0;
        const auto a = (size_t) (t0 * sr), b = juce::jmin (x.size(), (size_t) (t1 * sr));

        for (size_t i = a; i < b; ++i)
            sum += x[i] * x[i];

        return std::sqrt (sum / (double) juce::jmax ((size_t) 1, b - a));
    }

    /** The open string's frequency and B as the engine has them. */
    double openHz (Rig& rig, int s)  { return rig.engine.getTuningEngine().computeFrequency (s, 0.0, 0.0); }
    double stringB (Rig& rig, int s) { return rig.engine.getString (s).getPhysical().inharmonicityB; }

    const double kFret7 = 12.0 * std::log2 (1.5);    // 7.02: the 3rd partial's node
    const double kFret386 = 12.0 * std::log2 (1.25); // 3.86: the 5th's
}

//==============================================================================
LUTHIER_TEST (HarmonicRealism, HR01_naturalHarmonicPitch)
{
    Rig rig;
    rig.run (0.8, kLowE, [&] (int b) { if (b == 0) rig.engine.triggerNoteNow (rig.note (kLowE, 0.0, 12.0, Technique::NaturalHarmonic)); });

    const double f0 = rig.engine.getString (kLowE).getTargetFrequency();
    const double expected = 2.0 * f0 * std::sqrt (1.0 + 4.0 * stringB (rig, kLowE));
    const double measured = pitchNear (rig.out, 0.3, 0.5, expected, rig.sr);

    CHECK_MSG (std::abs (cents (measured, expected)) <= 2.0,
               "12th-fret harmonic of the low E at " + juce::String (measured, 3) + " Hz, expected "
                 + juce::String (expected, 3) + " (" + juce::String (cents (measured, expected), 2) + " cents)");

    // The bug this replaced: an octave higher than that.
    CHECK (std::abs (cents (measured, 2.0 * expected)) > 600.0);
}

LUTHIER_TEST (HarmonicRealism, HR02_HR03_fundamentalSuppressedAndNotASine)
{
    Rig rig;
    rig.run (0.6, kLowE, [&] (int b) { if (b == 0) rig.engine.triggerNoteNow (rig.note (kLowE, 0.0, 12.0, Technique::NaturalHarmonic)); });

    const double f0 = rig.engine.getString (kLowE).getTargetFrequency();
    const double B = stringB (rig, kLowE);
    auto partial = [&] (int k) { return levelDb (rig.out, 0.3, 0.25, k * f0 * std::sqrt (1.0 + B * k * k), rig.sr); };

    const double p1 = partial (1), p2 = partial (2), p3 = partial (3), p4 = partial (4), p5 = partial (5);

    CHECK_MSG (p2 - p1 >= 35.0, "partial 1 only " + juce::String (p2 - p1, 1) + " dB under partial 2");
    CHECK_MSG (p2 - p3 >= 30.0, "partial 3 only " + juce::String (p2 - p3, 1) + " dB under partial 2");
    CHECK_MSG (p2 - p5 >= 30.0, "partial 5 only " + juce::String (p2 - p5, 1) + " dB under partial 2");

    // HR-03: the harmonic has its own upper partials.
    CHECK_MSG (p2 - p4 <= 30.0, "partial 4 is " + juce::String (p2 - p4, 1) + " dB under partial 2: a sine");
}

LUTHIER_TEST (HarmonicRealism, HR04_fret7)
{
    Rig rig;
    rig.run (0.8, kA, [&] (int b) { if (b == 0) rig.engine.triggerNoteNow (rig.note (kA, 0.0, kFret7, Technique::NaturalHarmonic)); });

    const double f0 = rig.engine.getString (kA).getTargetFrequency();
    const double B = stringB (rig, kA);
    const double expected = 3.0 * f0 * std::sqrt (1.0 + 9.0 * B);
    const double measured = pitchNear (rig.out, 0.3, 0.5, expected, rig.sr);

    CHECK_MSG (std::abs (cents (measured, expected)) <= 3.0,
               "7.02 harmonic at " + juce::String (cents (measured, expected), 2) + " cents");

    auto partial = [&] (int k) { return levelDb (rig.out, 0.3, 0.25, k * f0 * std::sqrt (1.0 + B * k * k), rig.sr); };
    const double p3 = partial (3);
    CHECK_MSG (p3 - partial (1) >= 30.0, "partial 1 " + juce::String (p3 - partial (1), 1) + " dB under 3");
    CHECK_MSG (p3 - partial (2) >= 30.0, "partial 2 " + juce::String (p3 - partial (2), 1) + " dB under 3");
}

LUTHIER_TEST (HarmonicRealism, HR05_justIntonationIsKept)
{
    Rig rig;
    auto physical = rig.engine.getString (kLowE).getPhysical();
    physical.inharmonicityB = 0.0;
    rig.engine.getString (kLowE).setPhysical (physical);

    rig.run (0.8, kLowE, [&] (int b) { if (b == 0) rig.engine.triggerNoteNow (rig.note (kLowE, 0.0, kFret386, Technique::NaturalHarmonic)); });

    const double f0 = rig.engine.getString (kLowE).getTargetFrequency();
    const double measured = pitchNear (rig.out, 0.2, 0.5, 5.0 * f0, rig.sr);

    // G#4 equal-tempered from this E: 28 semitones above.
    const double equal = f0 * std::pow (2.0, 28.0 / 12.0);
    CHECK_MSG (std::abs (cents (measured, equal) + 13.686) <= 1.0,
               "3.86 harmonic is " + juce::String (cents (measured, equal), 2) + " cents from ET G#4, expected -13.7");
}

LUTHIER_TEST (HarmonicRealism, HR06_aMissedTouchIsADeadThud)
{
    auto render = [] (double touch)
    {
        Rig rig;
        rig.run (0.5, kA, [&] (int b) { if (b == 0) rig.engine.triggerNoteNow (rig.note (kA, 0.0, touch, Technique::NaturalHarmonic)); });
        return rmsOf (rig.out, 0.2, 0.4, rig.sr);
    };

    CHECK (harmonics::partialForFret (6.0) == 0);

    const double missed = render (6.0), hit = render (kFret7);
    CHECK_MSG (juce::Decibels::gainToDecibels (hit / juce::jmax (1.0e-12, missed)) >= 18.0,
               "the miss is only " + juce::String (juce::Decibels::gainToDecibels (hit / missed), 1) + " dB under the harmonic");
}

LUTHIER_TEST (HarmonicRealism, HR07_fingerWidthIsTheTolerance)
{
    // 3 mm off the 12th-fret node of the low E, toward the bridge.
    const double offNode = -12.0 * std::log2 (0.5 - 3.0 / 648.0);

    auto partial2 = [] (double touch, double width)
    {
        Rig rig;
        HarmonicTouchSettings t;
        t.fingerWidthMm = width;
        rig.engine.setHarmonicTouch (t);
        rig.run (0.6, kLowE, [&] (int b) { if (b == 0) rig.engine.triggerNoteNow (rig.note (kLowE, 0.0, touch, Technique::NaturalHarmonic)); });
        const double f0 = rig.engine.getString (kLowE).getTargetFrequency();
        return levelDb (rig.out, 0.3, 0.25, 2.0 * f0 * std::sqrt (1.0 + 4.0 * stringB (rig, kLowE)), rig.sr);
    };

    const double narrowLoss = partial2 (12.0, 2.5) - partial2 (offNode, 2.5);
    const double wideLoss = partial2 (12.0, 6.0) - partial2 (offNode, 6.0);

    CHECK_MSG (narrowLoss >= 3.0 && narrowLoss <= 15.0, "at 2.5 mm the harmonic lost " + juce::String (narrowLoss, 1) + " dB");
    CHECK_MSG (wideLoss < 3.0, "at 6 mm the harmonic lost " + juce::String (wideLoss, 1) + " dB");
}

LUTHIER_TEST (HarmonicRealism, HR08_pluckingTheNodeKillsIt)
{
    auto partial2 = [] (double position)
    {
        Rig rig;
        rig.engine.setPluckPosition (position);
        rig.run (0.6, kLowE, [&] (int b) { if (b == 0) rig.engine.triggerNoteNow (rig.note (kLowE, 0.0, 12.0, Technique::NaturalHarmonic)); });
        const double f0 = rig.engine.getString (kLowE).getTargetFrequency();
        return levelDb (rig.out, 0.2, 0.25, 2.0 * f0 * std::sqrt (1.0 + 4.0 * stringB (rig, kLowE)), rig.sr);
    };

    const double difference = partial2 (0.16) - partial2 (0.5);
    CHECK_MSG (difference >= 20.0, "plucking the node was only " + juce::String (difference, 1) + " dB quieter");
}

LUTHIER_TEST (HarmonicRealism, HR09_artificialHarmonics)
{
    auto measure = [] (double offset, int n)
    {
        Rig rig;
        rig.run (0.8, kA, [&] (int b) { if (b == 0) rig.engine.triggerNoteNow (rig.note (kA, 5.0, 5.0 + offset, Technique::ArtificialHarmonic)); });
        const double f0 = rig.engine.getString (kA).getTargetFrequency();   // D3, fretted
        const double B = stringB (rig, kA) * std::pow (2.0, 10.0 / 12.0);    // B scales as 1/L^2 on the fretted length
        const double expected = n * f0 * std::sqrt (1.0 + B * n * n);
        return cents (pitchNear (rig.out, 0.3, 0.5, expected, rig.sr), expected);
    };

    const double octave = measure (12.0, 2);
    const double twelfth = measure (kFret7, 3);
    CHECK_MSG (std::abs (octave) <= 3.0, "offset 12: " + juce::String (octave, 2) + " cents from D4");
    CHECK_MSG (std::abs (twelfth) <= 3.0, "offset 7: " + juce::String (twelfth, 2) + " cents from 3 x D3");
}

LUTHIER_TEST (HarmonicRealism, HR10_pinchHarmonic)
{
    // The partial the pinch selects, from the engine's own geometry.
    Rig probe;
    probe.engine.triggerNoteNow (probe.note (kB, 5.0, -1.0, Technique::PinchHarmonic));
    int n = 0;

    for (int slot = 0; slot < StringEngine::kMaxContacts; ++slot)
        n = juce::jmax (n, probe.engine.getString (kB).getContactPartial (slot));

    CHECK_MSG (n >= 2, "the pinch selected no partial at the default pick position");

    auto relative = [n] (Technique t)
    {
        Rig rig;
        rig.run (0.45, kB, [&] (int b) { if (b == 0) rig.engine.triggerNoteNow (rig.note (kB, 5.0, -1.0, t)); });
        const double f0 = rig.engine.getString (kB).getTargetFrequency();
        const double B = stringB (rig, kB) * std::pow (2.0, 5.0 / 6.0);
        return levelDb (rig.out, 0.2, 0.2, n * f0 * std::sqrt (1.0 + B * n * n), rig.sr)
               - levelDb (rig.out, 0.2, 0.2, f0 * std::sqrt (1.0 + B), rig.sr);
    };

    const double pinch = relative (Technique::PinchHarmonic);
    const double plain = relative (Technique::Pluck);

    CHECK_MSG (pinch - plain >= 8.0, "the pinch lifted partial " + juce::String (n) + " by only "
                                       + juce::String (pinch - plain, 1) + " dB");
    CHECK_MSG (pinch < 0.0, "a clean pinch harmonic is louder than the fundamental ("
                              + juce::String (pinch, 1) + " dB): ground rule 4");

    // Moving the pick moves the partial.
    auto selected = [] (double position)
    {
        Rig rig;
        rig.engine.setPluckPosition (position);
        rig.engine.triggerNoteNow (rig.note (kB, 5.0, -1.0, Technique::PinchHarmonic));
        int p = 0;

        for (int slot = 0; slot < StringEngine::kMaxContacts; ++slot)
            p = juce::jmax (p, rig.engine.getString (kB).getContactPartial (slot));

        return p;
    };

    CHECK_MSG (selected (0.12) != selected (0.20), "pick position 0.12 and 0.20 selected the same partial");
}

LUTHIER_TEST (HarmonicRealism, HR11_tappedHarmonicConvertsTheRingingString)
{
    Rig rig;
    bool stole = false;

    /*  The A string's own vibration. A fret 5 is D3, exactly the open D's
        pitch: with the bridge coupled the open D rings in sympathy for the
        half second before the tap and feeds its fundamental back in after
        the touch lifts (15 dB under, decaying at the D's own rate). That is
        real, and it is the D string's, so the measurement leaves it out. */
    rig.engine.getCouplingMatrix().setAmount (0.0);

    rig.run (0.65, kA, [&] (int b)
    {
        if (b == 0)
            rig.engine.triggerNoteNow (rig.note (kA, 5.0, -1.0, Technique::Pluck));

        if (b == (int) (0.5 * rig.sr / kBlock))
        {
            rig.engine.triggerNoteNow (rig.note (kA, 5.0, 17.0, Technique::Tap));
            stole = stole || rig.engine.getString (kA).isStealPending();
        }

        stole = stole || (b > (int) (0.5 * rig.sr / kBlock) && rig.engine.getString (kA).isStealPending());
    });

    const double f0 = rig.engine.getString (kA).getTargetFrequency();
    const double tapAt = (double) ((int) (0.5 * rig.sr / kBlock) * kBlock) / rig.sr;
    const double B = stringB (rig, kA) * std::pow (2.0, 5.0 / 6.0);
    const double p1 = levelDb (rig.out, tapAt + 0.05, 0.1, f0 * std::sqrt (1.0 + B), rig.sr);
    const double p2 = levelDb (rig.out, tapAt + 0.05, 0.1, 2.0 * f0 * std::sqrt (1.0 + 4.0 * B), rig.sr);

    CHECK_MSG (p2 - p1 >= 20.0, "150 ms after the tap partial 2 leads partial 1 by only " + juce::String (p2 - p1, 1) + " dB");
    CHECK_MSG (! stole, "the tapped harmonic voice-stole the ringing note");
}

LUTHIER_TEST (HarmonicRealism, HR12_randomContactsAreStable)
{
    for (const double sr : { 44100.0, 96000.0 })
    {
        Rig rig (sr, GuitarType::TwelveString);
        RtRandom r { 0x12C0417ull };
        const int strings = rig.engine.getNumStrings();
        CHECK (strings == 12);

        double worst = 0.0;
        bool finite = true;
        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;

        for (int s = 0; s < strings; ++s)
            rig.engine.triggerNoteNow (rig.note (s, (double) (s % 7), -1.0, Technique::Pluck, 1.0));

        for (int b = 0; b < (int) (10.0 * sr / kBlock); ++b)
        {
            if (b % 8 == 0)
            {
                for (int s = 0; s < strings; ++s)
                {
                    StringEngine::Contact c;
                    c.positionFromBridge = r.nextDouble();
                    c.strength = 1.0;
                    c.widthMm = 0.1 + r.nextDouble() * 19.9;
                    c.seconds = 0.001 + r.nextDouble() * 0.2;
                    rig.engine.getString (s).addContact (c);
                }
            }

            if (b % 400 == 0)
                for (int s = 0; s < strings; ++s)
                    rig.engine.triggerNoteNow (rig.note (s, (double) ((s + b) % 9), -1.0, Technique::Pluck, 1.0));

            buffer.clear();
            rig.engine.processBlock (buffer, midi);

            for (int s = 0; s < strings; ++s)
            {
                const float* own = rig.engine.getTapBuffers().stringRead (s);

                for (int i = 0; i < kBlock; ++i)
                {
                    finite = finite && std::isfinite (own[i]);
                    worst = juce::jmax (worst, (double) std::abs (own[i]));
                }
            }
        }

        CHECK_MSG (finite, "a non-finite sample at " + juce::String (sr));
        CHECK_MSG (worst < 4.0, "peak " + juce::String (worst, 3) + " at " + juce::String (sr));
    }

    /*  With no excitation the string's energy never grows. DECISION
        (REALISM-B): measured over 100 ms windows. The comb's taps are
        themselves in the loop, so energy moves between a period's worth of
        samples as a touch lands (one period's window grew 26x while the
        peak, the loop's real bound, never did). Over a window of many
        periods that redistribution averages out and passivity shows. */
    StringEngine str;
    str.prepare (48000.0, kBlock);
    str.snapToFrequency (110.0);
    Excitation::Params p;
    p.velocity = 1.0;
    str.excite (p);

    RtRandom r { 0xE4E4ull };
    double previous = 1.0e30, worstGrowth = 0.0, peakBefore = 0.0, peakAfter = 0.0;

    for (int w = 0; w < 40; ++w)
    {
        double energy = 0.0;

        for (int i = 0; i < 4800; ++i)
        {
            if (i % 300 == 0)
            {
                StringEngine::Contact c;
                c.positionFromBridge = r.nextDouble();
                c.strength = r.nextDouble();
                c.widthMm = 0.1 + r.nextDouble() * 19.9;
                c.seconds = 0.002 + r.nextDouble() * 0.02;
                str.addContact (c);
            }

            const double y = str.processSample (0.0);
            energy += y * y;
            (w == 0 ? peakBefore : peakAfter) = juce::jmax (w == 0 ? peakBefore : peakAfter, std::abs (y));
        }

        if (w > 0 && previous > 1.0e-24)
            worstGrowth = juce::jmax (worstGrowth, energy / previous);

        previous = energy;
    }

    CHECK_MSG (worstGrowth <= 1.05, "100 ms of touches grew the string's energy x" + juce::String (worstGrowth, 4));
    CHECK_MSG (peakAfter <= peakBefore * 1.05, "the peak grew under the touches");
}

LUTHIER_TEST (HarmonicRealism, HR13_landingAndLiftingAreClickFree)
{
    StringEngine free, touched;

    for (auto* s : { &free, &touched })
    {
        s->prepare (48000.0, kBlock);
        s->setIndex (3);
        s->snapToFrequency (146.8);
        Excitation::Params p;
        s->excite (p);
    }

    std::vector<double> a, b;
    const int land = 24000, lift = land + (int) (0.07 * 48000.0);

    for (int i = 0; i < 48000; ++i)
    {
        if (i == land)
        {
            StringEngine::Contact c;
            c.positionFromBridge = 0.5;
            c.seconds = 0.07;
            touched.addContact (c);
        }

        a.push_back (free.processSample (0.0));
        b.push_back (touched.processSample (0.0));
    }

    auto maxDiff = [] (const std::vector<double>& x, int centre)
    {
        double m = 0.0;

        for (int i = centre - 48; i < centre + 48; ++i)
            m = juce::jmax (m, std::abs (x[(size_t) i] - x[(size_t) i - 1]));

        return m;
    };

    for (const int edge : { land, lift })
        CHECK_MSG (maxDiff (b, edge) <= 1.5 * maxDiff (a, edge),
                   "edge at " + juce::String (edge) + ": " + juce::String (maxDiff (b, edge), 5)
                     + " against the free string's " + juce::String (maxDiff (a, edge), 5));
}

LUTHIER_TEST (HarmonicRealism, HR14_soundingPitchLocator)
{
    Rig rig;
    double open[kMaxStrings] {}, b[kMaxStrings] {};

    for (int s = 0; s < 6; ++s)
    {
        open[s] = openHz (rig, s);
        b[s] = stringB (rig, s);
    }

    const auto found = harmonics::locate (329.628, open, b, 6, 22, 5.0);
    CHECK (found.found);
    CHECK (found.stringIndex == kA);
    CHECK (found.partial == 3);
    CHECK_NEAR (found.touchFret, 7.02, 0.05);

    // Through the interpreter: the note names the pitch, and the string stays open.
    rig.engine.getTechniqueEngine().setNaturalHarmonicTrigger (true);
    MidiInterpreter::HarmonicSettings mapping;
    mapping.soundingPitch = true;
    rig.engine.getMidiInterpreter().setHarmonicSettings (mapping);
    rig.engine.getMidiInterpreter().setPlayingMode (PlayingMode::Mono);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
    PlayEventQueue q;
    rig.engine.getMidiInterpreter().processBlock (midi, kBlock, 0, q);

    CHECK (q.getNumNoteOns() == 1);

    if (q.getNumNoteOns() == 1)
    {
        const auto& e = q.getNoteOn (0);
        CHECK (e.stringIndex == kA);
        CHECK (e.technique == Technique::NaturalHarmonic);
        CHECK_NEAR (e.fretPosition, 0.0, 1.0e-9);
        CHECK_NEAR (e.touchFret, 7.02, 0.05);
        CHECK (std::abs (cents (e.pitchHz, open[kA])) < 20.0);
    }
}

LUTHIER_TEST (HarmonicRealism, HR15_fallbackIsCounted)
{
    Rig rig (44100.0);
    const int before = rig.engine.getValidator().getHarmonicFallbackCount();

    // High E fret 20, touched at the 8th partial's node 1/8 from the fret.
    const double touch = 20.0 + 12.0 * std::log2 (8.0 / 7.0);
    rig.engine.triggerNoteNow (rig.note (kHighE, 20.0, touch, Technique::ArtificialHarmonic));

    bool realised = false;

    for (int slot = 0; slot < StringEngine::kMaxContacts; ++slot)
        realised = realised || rig.engine.getString (kHighE).getContactPartial (slot) == 8;

    const bool fellBack = rig.engine.getValidator().getHarmonicFallbackCount() > before;
    CHECK_MSG (realised != fellBack, "the n = 8 contact was neither realised nor counted as a fallback (or both)");
    CHECK (rig.engine.getLastExcitation (kHighE).isolateHarmonic == fellBack);

    // A loop far too short for its comb always falls back: fret 22, n = 8, 22.05 kHz.
    Rig tiny (22050.0);
    const int before2 = tiny.engine.getValidator().getHarmonicFallbackCount();
    tiny.engine.triggerNoteNow (tiny.note (kHighE, 22.0, 22.0 + 12.0 * std::log2 (8.0 / 7.0), Technique::ArtificialHarmonic));
    CHECK (tiny.engine.getValidator().getHarmonicFallbackCount() == before2 + 1);
}

LUTHIER_TEST (HarmonicRealism, HR16_contactFreeIsBitIdentical)
{
    /*  The contact path and the direct input are inert when unused: a string
        whose contact has come and gone renders sample for sample what a
        string that never had one does, and a zero direct input is nothing. */
    StringEngine a, b;

    for (auto* s : { &a, &b })
    {
        s->prepare (48000.0, kBlock);
        s->snapToFrequency (196.0);
    }

    StringEngine::Contact c;
    c.seconds = 0.001;
    b.addContact (c);

    for (int i = 0; i < 480; ++i)
        b.processSample (0.0, 0.0);

    CHECK (! b.hasActiveContact());
    b.reset();
    b.snapToFrequency (196.0);
    a.reset();
    a.snapToFrequency (196.0);

    Excitation::Params p;
    a.excite (p);
    b.excite (p);

    bool identical = true;

    for (int i = 0; i < 48000; ++i)
        identical = identical && a.processSample (1.0e-4 * std::sin (i * 0.01)) == b.processSample (1.0e-4 * std::sin (i * 0.01), 0.0);

    CHECK_MSG (identical, "an unused contact path changed the string");
}

LUTHIER_TEST (HarmonicRealism, HR17_rangesAreRegistered)
{
    juce::ignoreUnused (Parameters::createLayout());

    for (const char* id : { ParamIDs::harmonicTouchPressure, ParamIDs::harmonicFingerWidth, ParamIDs::harmonicTouchTime,
                            ParamIDs::harmonicBriefTouch, ParamIDs::pinchThumbOffsetMm })
    {
        const auto* range = RangeRegistry::find (id);
        CHECK_MSG (range != nullptr && range->isValid() && range->family == RangeFamily::pick,
                   juce::String (id) + " has no valid pick-family PhysicalRange");
    }

    // 3.3: above 1 equation (3) is not convex.
    CHECK (RangeRegistry::find (ParamIDs::harmonicTouchPressure)->advancedMax <= 1.0f);
    CHECK_MSG (RangeRegistry::findDeclarationMismatches().isEmpty(),
               RangeRegistry::findDeclarationMismatches().joinIntoString (", "));
}

LUTHIER_TEST (HarmonicRealism, HR18_realtime)
{
    // No allocation across a harmonic performance, after the first block.
    {
        Rig rig;
        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;
        rig.engine.processBlock (buffer, midi);

        const long before = luthierAllocationsOnThisThread();

        for (int b = 0; b < 400; ++b)
        {
            if (b % 40 == 0)
            {
                rig.engine.triggerNoteNow (rig.note (kLowE, 0.0, 12.0, Technique::NaturalHarmonic));
                rig.engine.triggerNoteNow (rig.note (kA, 5.0, 17.0, Technique::ArtificialHarmonic));
                rig.engine.triggerNoteNow (rig.note (kB, 5.0, -1.0, Technique::PinchHarmonic));
                rig.engine.triggerNoteNow (rig.note (kA, 5.0, 17.0, Technique::Tap));
            }

            buffer.clear();
            rig.engine.processBlock (buffer, midi);
        }

        CHECK_MSG (luthierAllocationsOnThisThread() == before,
                   "harmonics allocated " + juce::String (luthierAllocationsOnThisThread() - before) + " times");
    }

    // Cost: six strings each touched at n = 8, against the same strings free.
    auto timeOneSecond = [] (bool touch)
    {
        std::array<StringEngine, 6> strings;
        double best = 1.0e9;

        for (int run = 0; run < 5; ++run)
        {
            for (int s = 0; s < 6; ++s)
            {
                strings[(size_t) s].prepare (48000.0, kBlock);
                strings[(size_t) s].snapToFrequency (82.4 * std::pow (2.0, s * 5.0 / 12.0));

                if (touch)
                {
                    StringEngine::Contact c;
                    c.positionFromBridge = 1.0 / 8.0;
                    c.seconds = 0.0;
                    strings[(size_t) s].addContact (c);
                }
            }

            double sink = 0.0;
            const auto start = juce::Time::getHighResolutionTicks();

            for (int i = 0; i < 48000; ++i)
                for (auto& s : strings)
                    sink += s.processSample (1.0e-6);

            best = juce::jmin (best, juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start));
            juce::ignoreUnused (sink);
        }

        return best;
    };

    const double units = 100.0 * juce::jmax (0.0, timeOneSecond (true) - timeOneSecond (false));

    /*  performance-budget.md: a unit is 1 % of a core. The spec draft's 0.05
        was a tenth of its own arithmetic (7 taps x 6 strings x 48 kHz is two
        million interpolated reads a second). Measured 0.4-0.75 with linear
        taps; harmonic-realism.md 8 now budgets 1.0 for this worst case (six
        strings held at n = 8) and about 0.1 for a single harmonic. */
    CHECK_MSG (units <= 1.0, "six n = 8 contacts cost " + juce::String (units, 3) + " units");
}

//==============================================================================
#include "../Export/MidiPerformance.h"
#include "../Export/MidiProfiles.h"

LUTHIER_TEST (HarmonicRealism, HR19_FA17_luthierExportRoundTrip)
{
    /*  harmonic-realism.md 9 HR-19 and fingerstyle-attack.md 9 FA-17: a
        passage with all four harmonic kinds (CC 73, 72, 103, 104) and the
        right hand's live controls (CC 102 tools, CC 105 rest strokes),
        exported in the Luthier profile, re-imported, and rendered again.
        The triggers are ordinary CCs around the touch-fret notes, which is
        also what the Generic profile carries (7). */
    MidiPerformance source (48000.0);
    source.setTempo (120.0);

    auto cc = [&source] (juce::int64 at, int n, int v) { source.addMessage (at, juce::MidiMessage::controllerEvent (1, n, v)); };
    auto hit = [&source] (juce::int64 at, int key, juce::int64 length)
    {
        source.addMessage (at, juce::MidiMessage::noteOn (1, key, (juce::uint8) 100));
        source.addMessage (at + length, juce::MidiMessage::noteOff (1, key));
    };

    cc (0, 73, 127);    hit (100, 52, 9000);  cc (9500, 73, 0);      // natural, 12th fret of the low E
    cc (10000, 72, 127); hit (10100, 64, 9000); cc (19500, 72, 0);   // pinch
    cc (20000, 103, 127); hit (20100, 57, 9000); cc (29500, 103, 0); // artificial
    hit (30000, 50, 20000);                                          // a note to tap on
    cc (39000, 104, 127); hit (40000, 62, 6000); cc (47000, 104, 0); // tapped harmonic
    cc (50000, 102, 40); hit (50100, 60, 6000);                      // Finger tool (band 2)
    cc (57000, 105, 127); hit (57100, 64, 6000); cc (64000, 105, 0); // a rest stroke
    cc (65000, 102, 0);

    MidiPerformance imported;
    const auto bytes = MidiProfiles::exportToMemory (source, MidiExportOptions {});
    const auto result = MidiProfiles::importFromMemory (bytes.getData(), bytes.getSize(), imported, 48000.0);
    CHECK_MSG (result.ok, result.error);

    auto render = [] (const MidiPerformance& performance)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, kBlock);
        engine.setGuitarType (GuitarType::Stratocaster);
        engine.getCharacterEngine().setEnabled (false);
        engine.getMidiInterpreter().setHumanisation ({ 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 });
        engine.getMidiInterpreter().setPlayingMode (PlayingMode::Mono);
        engine.reset();

        std::vector<double> out;
        juce::AudioBuffer<float> block (2, kBlock);
        size_t cursor = 0;

        for (int position = 0; position < 72000; position += kBlock)
        {
            block.clear();
            juce::MidiBuffer midi;
            performance.renderBlock (midi, position, kBlock, cursor);
            engine.processBlock (block, midi);

            for (int i = 0; i < kBlock; ++i)
                out.push_back (0.5 * (block.getSample (0, i) + block.getSample (1, i)));
        }

        return out;
    };

    const auto a = render (source), b = render (imported);
    double diff = 0.0, level = 0.0;

    for (size_t i = 0; i < juce::jmin (a.size(), b.size()); ++i)
    {
        diff += (a[i] - b[i]) * (a[i] - b[i]);
        level += a[i] * a[i];
    }

    const double nullDbfs = juce::Decibels::gainToDecibels (std::sqrt (diff / (double) a.size()), -400.0);
    CHECK_MSG (level > 1.0e-6, "the passage was silent");
    CHECK_MSG (nullDbfs <= -60.0, "the Luthier round trip nulls at only " + juce::String (nullDbfs, 1) + " dBFS RMS");
}
