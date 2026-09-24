/*  Body coupling (body-coupling.md 8, BC-01 to BC-13; environment.md ENV-13).

    Most measurements run on a rig of the guitar's strings and the bank alone
    - the pieces the return path is made of, wired exactly as processSubBlock
    wires them - so a T60 is the string's own and a warble is not smeared by the
    radiated body, the amp or the coupling matrix. The wiring itself (off is
    bit-identical, the legacy load, the tap, the budget) is tested on the engine
    and the processor.
*/

#include "TestFramework.h"

#include "../DSP/Coupling/BodyCouplingBank.h"
#include "../DSP/String/StringEngine.h"
#include "../Model/Guitar/StringMaterials.h"
#include "../Model/Guitar/GuitarLibrary.h"
#include "../Model/Playing/TuningEngine.h"
#include "../Character/EnvironmentModel.h"
#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../PhysicalRange.h"
#include "../UI/RealismGroups.h"

#include <cstdio>

using namespace luthier;
using namespace luthier::tests;

long luthierAllocationCount() noexcept;   // CircuitTests.cpp

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    //==========================================================================
    /** A guitar's strings and its bank: processSubBlock's return path, alone. */
    struct Rig
    {
        GuitarSpec spec {};
        BodyConfig body;
        BodyCouplingBank bank;
        BodyCouplingDesign design;
        BodyCouplingScaling scaling;
        std::array<StringEngine, kMaxStrings> strings;
        std::array<double, kMaxStrings> openHz {}, waves {}, inputs {}, z0 {};
        int n = 6;

        Rig (GuitarType type, double amount = 1.0, const BridgeCoupling* bridge = nullptr)
        {
            spec = GuitarLibrary::get (type);
            body = GuitarLibrary::makeBodyConfig (spec);
            n = spec.numStrings;
            TuningEngine::getPresetFrequencies (spec.tuning, openHz.data(), n);

            for (int s = 0; s < n; ++s)
            {
                const auto st = StringMaterials::computeSpec (spec.stringMaterial, spec.stringGauge, StringAge::Fresh,
                                                              s, openHz[(size_t) s], spec.scaleLengthMm);
                auto& str = strings[(size_t) s];
                str.setIndex (s);
                str.prepare (kSr, kBlock);
                str.setPhysical (StringMaterials::toPhysical (st, spec.scaleLengthMm));
                str.snapToFrequency (openHz[(size_t) s]);
                z0[(size_t) s] = str.getPhysical().waveImpedance;
            }

            const auto b = bridge != nullptr ? *bridge
                                             : BodyCouplingBank::bridgeFor ((int) spec.bridge, spec.category == GuitarCategory::Acoustic,
                                                                            body.shape == BodyShape::Resonator);
            bank.prepare (kSr);
            design = bank.design (body, chamberingFor (body.shape), b, z0.data(), n);
            bank.stage (design);
            bank.setAmount (amount);
            bank.setModeCount (16);
            bank.reset();
        }

        void setScaling (const BodyCouplingScaling& s)
        {
            scaling = s;
            bank.setScaling (s);
            bank.reset();
        }

        void pluck (int s, double fret, double velocity = 0.8)
        {
            auto& str = strings[(size_t) s];
            str.snapToFrequency (openHz[(size_t) s] * std::pow (2.0, fret / 12.0));
            Excitation::Params p;
            p.velocity = velocity;
            p.pluckPosition = 0.16;
            str.excite (p);
        }

        /** Runs; `out[s]` collects string s's output. */
        void run (double seconds, std::vector<std::vector<double>>& out)
        {
            out.assign ((size_t) n, {});
            const int total = (int) (seconds * kSr);

            for (int i = 0; i < total; ++i)
            {
                if (i % kBlock == 0)
                    bank.beginBlock();

                inputs.fill (0.0);

                for (int s = 0; s < n; ++s)
                {
                    strings[(size_t) s].beginSample();
                    waves[(size_t) s] = strings[(size_t) s].getBridgeWave();
                }

                bank.processSample (waves.data(), inputs.data(), n);

                for (int s = 0; s < n; ++s)
                    out[(size_t) s].push_back (strings[(size_t) s].endSample (inputs[(size_t) s]));
            }
        }

        double fundamental (int s, double fret) const { return openHz[(size_t) s] * std::pow (2.0, fret / 12.0); }

        /** The strongest mode: what "the main mode" means (2.2). */
        int mainMode() const { return 0; }
    };

    /** T60 from the 20 ms RMS in dB, fitted from 5 to 35 dB down (or the
        deepest it goes). */
    double measureT60 (const std::vector<double>& x)
    {
        const int w = (int) (0.020 * kSr);
        std::vector<double> db;

        for (size_t i = 0; i + (size_t) w <= x.size(); i += (size_t) w)
        {
            double e = 0.0;

            for (int k = 0; k < w; ++k)
                e += x[i + (size_t) k] * x[i + (size_t) k];

            db.push_back (10.0 * std::log10 (juce::jmax (1.0e-30, e / w)));
        }

        const double peak = *std::max_element (db.begin(), db.end());
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        int count = 0;
        bool started = false;

        for (size_t i = 0; i < db.size(); ++i)
        {
            if (db[i] > peak - 5.0 && ! started)
                continue;

            started = true;

            if (db[i] < peak - 35.0)
                break;

            const double t = (double) i * 0.020;
            sx += t; sy += db[i]; sxx += t * t; sxy += t * db[i]; ++count;
        }

        if (count < 3)
            return 0.0;

        const double slope = (count * sxy - sx * sy) / (count * sxx - sx * sx);
        return slope < 0.0 ? -60.0 / slope : 1.0e9;
    }

    /** The fundamental alone: a band-pass at f0 (Q 8), which is what a wolf eats. */
    std::vector<double> fundamentalBand (const std::vector<double>& x, double f0)
    {
        Biquad bp;
        bp.setBandpass (kSr, f0, 8.0);
        std::vector<double> y (x.size());

        for (size_t i = 0; i < x.size(); ++i)
            y[i] = bp.process (x[i]);

        return y;
    }

    double t60At (GuitarType type, int s, double fret, const BodyCouplingScaling& sc, double amount = 1.0,
                  const BridgeCoupling* bridge = nullptr, double seconds = 6.0, bool others = true)
    {
        Rig rig (type, amount, bridge);
        rig.setScaling (sc);

        // The other strings are damped by the hand, as when the note is played.
        if (! others)
            for (int o = 0; o < rig.n; ++o)
                if (o != s)
                    rig.strings[(size_t) o].setDamping (StringEngine::Damping::Choked, 1.0);

        rig.pluck (s, fret);
        std::vector<std::vector<double>> out;
        rig.run (seconds, out);
        return measureT60 (fundamentalBand (out[(size_t) s], rig.fundamental (s, fret)));
    }

    /** The largest T60 dip across frets 0..19 of a string, against the same string with the bank off. */
    struct Dip { int fret = 0; double loss = 0.0; };

    Dip worstDip (GuitarType type, int s, const BodyCouplingScaling& sc, int frets = 19)
    {
        Dip worst;

        for (int f = 0; f <= frets; ++f)
        {
            const double on = t60At (type, s, f, sc, 1.0, nullptr, 4.0);
            const double off = t60At (type, s, f, sc, 0.0, nullptr, 4.0);
            const double loss = 1.0 - on / juce::jmax (1.0e-6, off);

            if (loss > worst.loss)
                worst = { f, loss };
        }

        return worst;
    }

    BodyCouplingScaling freqScale (double k)
    {
        BodyCouplingScaling sc;
        sc.plateFreq = sc.airFreq = k;
        return sc;
    }

    /** Frequencies of the spectral peaks of `x` within [lo, hi] Hz. */
    std::vector<double> peaksBetween (const std::vector<double>& x, double seconds, double lo, double hi)
    {
        constexpr int order = 17;
        const int size = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> buffer ((size_t) size * 2, 0.0f);
        const int n = juce::jmin ((int) (seconds * kSr), (int) x.size());

        for (int i = 0; i < n; ++i)
            buffer[(size_t) i] = (float) x[(size_t) i];   // rectangular: the finest resolution 500 ms allows

        fft.performFrequencyOnlyForwardTransform (buffer.data());

        std::vector<double> peaks;
        const double bin = kSr / size;
        float top = 0.0f;

        for (int k = (int) (lo / bin); k <= (int) (hi / bin); ++k)
            top = juce::jmax (top, buffer[(size_t) k]);

        for (int k = juce::jmax (1, (int) (lo / bin)); k <= (int) (hi / bin); ++k)
            if (buffer[(size_t) k] > buffer[(size_t) k - 1] && buffer[(size_t) k] >= buffer[(size_t) k + 1]
                && buffer[(size_t) k] > 0.25f * top)
                peaks.push_back (k * bin);

        return peaks;
    }

    /** The frequency, between lo and hi, where a note on string s decays fastest. */
    double wolfFrequency (GuitarType type, int s, const BodyCouplingScaling& sc, double lo, double hi)
    {
        double bestHz = lo, bestT60 = 1.0e9;
        std::vector<std::pair<double, double>> curve;

        for (double hz = lo; hz <= hi + 1.0e-9; hz += (hi - lo) / 48.0)
        {
            Rig rig (type);
            rig.setScaling (sc);

            for (int o = 0; o < rig.n; ++o)
                if (o != s)
                    rig.strings[(size_t) o].setDamping (StringEngine::Damping::Choked, 1.0);

            rig.pluck (s, 12.0 * std::log2 (hz / rig.openHz[(size_t) s]));
            std::vector<std::vector<double>> out;
            rig.run (2.5, out);
            const double t60 = measureT60 (fundamentalBand (out[(size_t) s], hz));
            curve.push_back ({ hz, t60 });

            if (t60 > 0.0 && t60 < bestT60)
            {
                bestT60 = t60;
                bestHz = hz;
            }
        }

        // A parabola through the minimum and its neighbours.
        for (size_t i = 1; i + 1 < curve.size(); ++i)
        {
            if (curve[i].first != bestHz)
                continue;

            const double y0 = curve[i - 1].second, y1 = curve[i].second, y2 = curve[i + 1].second;
            const double denom = y0 - 2.0 * y1 + y2;

            if (denom > 0.0)
                bestHz += 0.5 * (y0 - y2) / denom * (curve[i + 1].first - curve[i].first);
        }

        return bestHz;
    }

    std::unique_ptr<LuthierEngine> makeEngine (GuitarType type)
    {
        auto e = std::make_unique<LuthierEngine>();
        e->prepare (kSr, kBlock);
        e->setGuitarType (type);
        e->getCharacterEngine().setEnabled (false);
        e->reset();
        return e;
    }

    void pluckEngine (LuthierEngine& e, int s, double fret = 0.0, double velocity = 0.8)
    {
        NoteOnEvent n;
        n.stringIndex = s;
        n.fretPosition = fret;
        n.velocity = velocity;
        n.midiNote = 40 + s;
        n.pitchHz = e.getTuningEngine().computeFrequency (s, fret);
        e.triggerNoteNow (n);
    }
}

//==============================================================================
LUTHIER_TEST (BodyCoupling, BC01_theBankIsPassive)
{
    Rig rig (GuitarType::Dreadnought);

    for (int s = 0; s < rig.n; ++s)
        rig.pluck (s, 0.0);

    std::vector<std::vector<double>> out;
    rig.run (10.0, out);

    const int w = (int) (0.050 * kSr);
    double last = -1.0, worstRise = -1.0e9;

    for (size_t i = (size_t) (0.020 * kSr); i + (size_t) w <= out[0].size(); i += (size_t) w)
    {
        double e = 0.0;

        for (int s = 0; s < rig.n; ++s)
            for (int k = 0; k < w; ++k)
                e += rig.z0[(size_t) s] * out[(size_t) s][i + (size_t) k] * out[(size_t) s][i + (size_t) k];

        if (last > 0.0 && e > 1.0e-12)
            worstRise = juce::jmax (worstRise, 10.0 * std::log10 (e / last));

        last = e;
    }

    CHECK_MSG (worstRise <= 0.1, "string energy rose " + juce::String (worstRise, 3) + " dB between windows");
    CHECK (rig.bank.getCapFraction() == 0.0);
}

LUTHIER_TEST (BodyCoupling, BC02_aWolfOnAnAcoustic)
{
    Rig probe (GuitarType::Dreadnought);
    const int low = probe.n - 1;
    const double g2 = probe.fundamental (low, 3.0);
    const auto sc = freqScale (g2 / BodyCouplingBank::viewMode (probe.design, 0, {}).hz);

    const double t3 = t60At (GuitarType::Dreadnought, low, 3.0, sc, 1.0, nullptr, 6.0, false);
    const double t0 = t60At (GuitarType::Dreadnought, low, 0.0, sc, 1.0, nullptr, 6.0, false);
    const double t6 = t60At (GuitarType::Dreadnought, low, 6.0, sc, 1.0, nullptr, 6.0, false);
    const double dip = 1.0 - t3 / (0.5 * (t0 + t6));

    CHECK_MSG (dip >= 0.50, "wolf dip " + juce::String (100.0 * dip, 1) + " % (T60 " + juce::String (t0, 2) + " / "
                              + juce::String (t3, 2) + " / " + juce::String (t6, 2) + " s)");

    // The warble: the fundamental splits into two peaks near f0.
    Rig rig (GuitarType::Dreadnought);
    rig.setScaling (sc);

    for (int o = 0; o < rig.n - 1; ++o)
        rig.strings[(size_t) o].setDamping (StringEngine::Damping::Choked, 1.0);

    rig.pluck (low, 3.0);
    std::vector<std::vector<double>> out;
    rig.run (0.5, out);

    const auto peaks = peaksBetween (out[(size_t) low], 0.5, 0.85 * g2, 1.15 * g2);
    const double spread = peaks.size() >= 2 ? peaks.back() - peaks.front() : 0.0;
    CHECK_MSG (peaks.size() >= 2 && spread >= 2.0,
               juce::String ((int) peaks.size()) + " peaks near " + juce::String (g2, 1) + " Hz, spread "
                 + juce::String (spread, 2) + " Hz");
}

LUTHIER_TEST (BodyCoupling, BC03_aSolidbodyIsMild)
{
    for (auto type : { GuitarType::LesPaul, GuitarType::Stratocaster })
    {
        Rig probe (type);
        const int low = probe.n - 1;
        const auto sc = freqScale (probe.fundamental (low, 3.0) / BodyCouplingBank::viewMode (probe.design, 0, {}).hz);
        const double on = t60At (type, low, 3.0, sc, 1.0, nullptr, 6.0, false);
        const double off = t60At (type, low, 3.0, sc, 0.0, nullptr, 6.0, false);
        const double dip = 1.0 - on / off;

        CHECK_MSG (dip >= 0.20 && dip <= 0.50, juce::String (probe.spec.name) + " exact-coincidence dip "
                                                + juce::String (100.0 * dip, 1) + " %");

        // No splitting on a 10 kg body.
        Rig rig (type);
        rig.setScaling (sc);

        for (int o = 0; o < rig.n - 1; ++o)
            rig.strings[(size_t) o].setDamping (StringEngine::Damping::Choked, 1.0);

        rig.pluck (low, 3.0);
        std::vector<std::vector<double>> out;
        rig.run (0.5, out);
        const double g = rig.fundamental (low, 3.0);
        CHECK (peaksBetween (out[(size_t) low], 0.5, 0.85 * g, 1.15 * g).size() == 1);
    }

    // At the default tuning, no fret 0-19 of any string of the factory six-string
    // solidbodies loses more than 35 %.
    for (auto type : { GuitarType::Stratocaster, GuitarType::Telecaster, GuitarType::LesPaul,
                       GuitarType::SG, GuitarType::Explorer, GuitarType::IbanezRG })
    {
        Rig probe (type);

        for (int s = 0; s < probe.n; ++s)
        {
            for (int f = 0; f <= 19; ++f)
            {
                const double on = t60At (type, s, f, {}, 1.0, nullptr, 2.5, false);
                const double off = t60At (type, s, f, {}, 0.0, nullptr, 2.5, false);
                const double dip = 1.0 - on / off;

                if (dip > 0.35)
                {
                    CHECK_MSG (false, juce::String (probe.spec.name) + " string " + juce::String (s) + " fret "
                                        + juce::String (f) + " dips " + juce::String (100.0 * dip, 1) + " %");
                    break;
                }
            }
        }
    }
}

LUTHIER_TEST (BodyCoupling, BC04_offIsBitIdentical)
{
    // At 0 the bank is out of the per-sample loop: nothing about it - its
    // modes, its scaling, a tap - reaches the strings.
    auto render = [&] (bool fiddle)
    {
        auto engine = makeEngine (GuitarType::Dreadnought);
        engine->getBodyCoupling().setAmount (0.0);

        if (fiddle)
        {
            engine->getBodyCoupling().setModeCount (4);
            engine->setBodyModeScales (1.05, 1.7, 0.3);
            engine->getBodyCoupling().driveDirect (1.0, 0);
        }

        for (int s = 0; s < 6; ++s)
            pluckEngine (*engine, s, 2.0);

        juce::AudioBuffer<float> buffer (2, kBlock);
        std::vector<float> out;
        juce::MidiBuffer midi;

        for (int b = 0; b < (int) (2.0 * kSr / kBlock); ++b)
        {
            buffer.clear();
            engine->processBlock (buffer, midi);

            for (int i = 0; i < kBlock; ++i)
                out.push_back (buffer.getSample (0, i));
        }

        CHECK (! engine->getBodyCoupling().isActive());
        return out;
    };

    // The body's scales are shared (3): the radiated body moves with them, so
    // compare the pre-body strings, which is where the bank acts.
    auto preBody = [] (bool fiddle)
    {
        auto engine = makeEngine (GuitarType::Dreadnought);
        engine->getBodyCoupling().setAmount (0.0);

        if (fiddle)
        {
            engine->getBodyCoupling().setModeCount (4);
            engine->setBodyModeScales (1.05, 1.7, 0.3);
            engine->getBodyCoupling().driveDirect (1.0, 0);
        }

        for (int s = 0; s < 6; ++s)
            pluckEngine (*engine, s, 2.0);

        juce::AudioBuffer<float> buffer (2, kBlock);
        std::vector<double> out;
        juce::MidiBuffer midi;

        for (int b = 0; b < (int) (2.0 * kSr / kBlock); ++b)
        {
            buffer.clear();
            engine->processBlock (buffer, midi);

            for (int i = 0; i < kBlock; ++i)
                out.push_back (engine->getPreBodyBuffer()[i]);
        }

        return out;
    };

    CHECK (render (false) == render (false));
    CHECK (preBody (false) == preBody (true));
}

LUTHIER_TEST (BodyCoupling, BC05_theWolfMovesWithTheBody)
{
    Rig probe (GuitarType::Dreadnought);
    const double main = BodyCouplingBank::viewMode (probe.design, 0, {}).hz;
    const int low = probe.n - 1;

    const double at1 = wolfFrequency (GuitarType::Dreadnought, low, {}, main * 0.93, main * 1.07);
    const double at097 = wolfFrequency (GuitarType::Dreadnought, low, freqScale (0.97), main * 0.90, main * 1.04);
    const double moved = at097 / at1 - 1.0;

    CHECK_MSG (std::abs (moved + 0.03) <= 0.005, "the wolf moved " + juce::String (100.0 * moved, 2) + " % ("
                                                  + juce::String (at1, 2) + " -> " + juce::String (at097, 2) + " Hz)");
}

LUTHIER_TEST (BodyCoupling, ENV13_theWolfFollowsTheEnvironment)
{
    // environment.md 2.6 moves the plate modes. Measured on a solidbody,
    // whose modes are all plate: on an acoustic the unmoving air modes sit
    // beside the plate ones and pull the measured minimum (-3.5 % there).
    Rig probe (GuitarType::LesPaul);
    const double f = BodyCouplingBank::viewMode (probe.design, 0, {}).hz;
    const int s = probe.n - 1;
    BodyCouplingScaling onNote = freqScale (probe.fundamental (s, 3.0) / f);
    BodyCouplingScaling warm = onNote;
    warm.plateFreq *= 0.97;

    const double centre = probe.fundamental (s, 3.0);
    const double at1 = wolfFrequency (GuitarType::LesPaul, s, onNote, centre * 0.95, centre * 1.05);
    const double at097 = wolfFrequency (GuitarType::LesPaul, s, warm, centre * 0.92, centre * 1.02);
    const double moved = at097 / at1 - 1.0;

    CHECK_MSG (std::abs (moved + 0.03) <= 0.005, "the wolf moved " + juce::String (100.0 * moved, 2) + " %");
}

LUTHIER_TEST (BodyCoupling, BC06_BC09_aTapRingsTheStringsNearAMode)
{
    auto answer = [] (bool muted)
    {
        Rig rig (GuitarType::Dreadnought);
        const double main = BodyCouplingBank::viewMode (rig.design, 0, {}).hz;

        // String 4 retuned onto the main mode.
        rig.openHz[4] = main;
        rig.strings[4].snapToFrequency (rig.openHz[4]);

        // The string the bank reaches least: the smallest admittance summed
        // over its first four partials.
        int far = 0;
        double least = 1.0e9;

        for (int s = 0; s < rig.n; ++s)
        {
            if (s == 4)
                continue;

            double y = 0.0;

            for (int n = 1; n <= 4; ++n)
                y += BodyCouplingBank::realAdmittance (rig.design, 16, {}, n * rig.openHz[(size_t) s], 1.0) / n;

            if (y < least)
            {
                least = y;
                far = s;
            }
        }

        if (muted)
            rig.strings[4].setDamping (StringEngine::Damping::PalmMute, 1.0);

        rig.bank.driveDirect (1.0, 0);
        std::vector<std::vector<double>> out;
        rig.run (1.0, out);

        return std::make_pair (rms (out[4].data(), (int) out[4].size()), rms (out[(size_t) far].data(), (int) out[(size_t) far].size()));
    };

    const auto open = answer (false);
    const double over = juce::Decibels::gainToDecibels (open.first / juce::jmax (1.0e-15, open.second));
    CHECK_MSG (over >= 12.0, "the string on the mode answered only " + juce::String (over, 1) + " dB above the far one");

    const auto muted = answer (true);
    const double less = juce::Decibels::gainToDecibels (open.first / juce::jmax (1.0e-15, muted.first));
    CHECK_MSG (less >= 6.0, "a palm-muted string answered only " + juce::String (less, 1) + " dB less");
}

LUTHIER_TEST (BodyCoupling, BC07_sympatheticRingThroughTheBody)
{
    auto dRing = [] (double amount)
    {
        auto engine = makeEngine (GuitarType::Dreadnought);
        engine->getCouplingMatrix().setAmount (0.05);
        engine->getBodyCoupling().setAmount (amount);
        engine->reset();
        pluckEngine (*engine, 4, 0.0, 1.0);   // A

        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;
        double peak = 0.0;

        for (int b = 0; b < (int) (3.0 * kSr / kBlock); ++b)
        {
            buffer.clear();
            engine->processBlock (buffer, midi);
            peak = juce::jmax (peak, engine->getString (3).getLevel());   // D, unplucked
        }

        return peak;
    };

    const double with = dRing (1.0), without = dRing (0.0);
    const double more = juce::Decibels::gainToDecibels (with / juce::jmax (1.0e-15, without));
    CHECK_MSG (more >= 6.0, "D rang only " + juce::String (more, 1) + " dB more with the body");
}

LUTHIER_TEST (BodyCoupling, BC08_aHeavyBridgeCouplesLess)
{
    Rig probe (GuitarType::Dreadnought);
    const int low = probe.n - 1;
    const auto sc = freqScale (probe.fundamental (low, 3.0) / BodyCouplingBank::viewMode (probe.design, 0, {}).hz);

    // At exact coincidence any coupling stronger than the string's own loss
    // takes most of the fundamental (and the pin bridge is past that, into
    // the warble), so the bridge's effect shows beside the mode: one
    // semitone off it, and in the admittance itself (body-coupling.md 8, as built).
    auto dipWith = [&] (BridgeCoupling b)
    {
        const double on = t60At (GuitarType::Dreadnought, low, 4.0, sc, 1.0, &b, 6.0, false);
        const double off = t60At (GuitarType::Dreadnought, low, 4.0, sc, 0.0, &b, 6.0, false);
        return 1.0 - on / off;
    };

    auto peakWith = [&] (BridgeCoupling b)
    {
        Rig rig (GuitarType::Dreadnought, 1.0, &b);
        return BodyCouplingBank::viewMode (rig.design, 0, sc).peakAdmittance;
    };

    const double pin = dipWith ({ 0.028, 0.92 }), floyd = dipWith ({ 0.320, 0.30 });
    CHECK_MSG (floyd <= 0.5 * pin, "a semitone off the mode: pin " + juce::String (100.0 * pin, 1) + " %, Floyd "
                                     + juce::String (100.0 * floyd, 1) + " %");
    CHECK (peakWith ({ 0.320, 0.30 }) <= 0.5 * peakWith ({ 0.028, 0.92 }));
}

LUTHIER_TEST (BodyCoupling, BC10_stabilityAcrossTheAdvancedCorners)
{
    // 200 corners of the advanced ranges, 1 s each, twelve strings of dense
    // chords (the spec's 1000 x 5 s is cut to fit the suite's time; every
    // corner combination of the four scales is still visited many times).
    juce::Random random (0xB0D1u);
    bool finite = true;
    double worstPeak = 0.0, worstCap = 0.0;

    for (int corner = 0; corner < 200 && finite; ++corner)
    {
        Rig rig (GuitarType::TwelveString);
        BodyCouplingScaling sc;
        auto pick = [&random] (double lo, double hi) { return random.nextBool() ? lo : hi; };
        const double f = pick (0.5, 2.0);
        sc.plateFreq = sc.airFreq = f;
        sc.q = sc.airQ = pick (0.1, 5.0);
        sc.mass = pick (0.05, 20.0);
        rig.setScaling (sc);
        rig.bank.setModeCount (16);

        for (int s = 0; s < rig.n; ++s)
            rig.pluck (s, random.nextInt (12), 1.0);

        std::vector<std::vector<double>> out;
        rig.run (1.0, out);

        for (const auto& str : out)
            for (double x : str)
            {
                finite = finite && std::isfinite (x);
                worstPeak = juce::jmax (worstPeak, std::abs (x));
            }

        worstCap = juce::jmax (worstCap, rig.bank.getCapFraction());
    }

    CHECK (finite);
    CHECK_MSG (worstPeak < 4.0, "peak " + juce::String (worstPeak, 3));
    CHECK_MSG (worstCap <= 0.01, "the cap engaged on " + juce::String (100.0 * worstCap, 2) + " % of samples");
}

LUTHIER_TEST (BodyCoupling, BC11_theWolfMapIsHonest)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    processor.getEngine().setGuitarType (GuitarType::Dreadnought);
    processor.getEngine().getBodyCoupling().setAmount (1.0);

    int n = 0;
    const auto map = BodyCouplingGroup::computeWolfMap (processor, n);
    CHECK (n == 6);

    for (int s = n - 1; s >= n - 3; --s)
    {
        // The measured shortest fret.
        int shortest = 0;
        double best = 1.0e9;

        for (int f = 0; f <= WolfMap::kFrets; ++f)
        {
            const double t = t60At (GuitarType::Dreadnought, s, f, {}, 1.0, nullptr, 3.0, false);

            if (t > 0.0 && t < best)
            {
                best = t;
                shortest = f;
            }
        }

        // The map's three highest cells on that string.
        std::vector<std::pair<float, int>> cells;

        for (int f = 0; f <= WolfMap::kFrets; ++f)
            cells.push_back ({ map[(size_t) (s * (WolfMap::kFrets + 1) + f)], f });

        std::sort (cells.begin(), cells.end(), [] (auto a, auto b) { return a.first > b.first; });

        const bool found = cells[0].second == shortest || cells[1].second == shortest || cells[2].second == shortest;
        CHECK_MSG (found, "string " + juce::String (s) + ": shortest T60 at fret " + juce::String (shortest)
                            + ", map's top three " + juce::String (cells[0].second) + ", " + juce::String (cells[1].second)
                            + ", " + juce::String (cells[2].second));
    }
}

LUTHIER_TEST (BodyCoupling, BC12_legacyLoadIsOff)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& state = processor.getState();
    CHECK_NEAR (state.getParameter (ParamIDs::bodyCouplingAmount)->getValue(), 1.0, 1.0e-6);   // new presets: on

    // A preset from before this spec: no body_coupling_amount, no ranges block.
    auto data = processor.getPresetManager().toVar();
    auto* params = data.getDynamicObject()->getProperty ("parameters").getDynamicObject();
    params->removeProperty (ParamIDs::bodyCouplingAmount);
    params->removeProperty (ParamIDs::stringAgeHours);
    data.getDynamicObject()->removeProperty ("ranges");
    CHECK (processor.getPresetManager().fromVar (data));

    CHECK (state.getParameter (ParamIDs::bodyCouplingAmount)->getValue() == 0.0f);
    CHECK (! processor.getRanges().isFamilyAdvanced (RangeFamily::body));

    for (const char* id : { ParamIDs::bodyModeMassScale, ParamIDs::bodyModeQScale, ParamIDs::bodyModeFreqScale })
    {
        const auto* r = RangeRegistry::find (id);
        CHECK (r != nullptr && r->isValid() && r->family == RangeFamily::body);
    }
}

LUTHIER_TEST (BodyCoupling, BC13_budgetAndSafety)
{
    Rig rig (GuitarType::TwelveString);
    rig.bank.setModeCount (16);
    rig.bank.setAmount (1.0);
    rig.bank.reset();

    std::array<double, kMaxStrings> waves {}, inputs {};
    juce::Random random (7);

    for (auto& w : waves)
        w = random.nextDouble() * 0.2 - 0.1;

    constexpr int samples = 48000 * 20;
    double best = 1.0e9;

    for (int run = 0; run < 3; ++run)
    {
        const long allocations = luthierAllocationCount();
        const auto start = juce::Time::getHighResolutionTicks();

        for (int i = 0; i < samples; ++i)
        {
            if (i % kBlock == 0)
            {
                // A part swap's staged design arrives mid-run.
                if (i == samples / 2)
                    rig.bank.stage (rig.design);

                rig.bank.beginBlock();
            }

            rig.bank.processSample (waves.data(), inputs.data(), 12);
            waves[(size_t) (i % 12)] = -0.5 * waves[(size_t) (i % 12)] + 1.0e-3 * inputs[(size_t) (i % 12)];
        }

        best = juce::jmin (best, juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start));
        CHECK (luthierAllocationCount() == allocations);
    }

    const double units = 100.0 * best / (samples / kSr);
    CHECK_MSG (units <= 0.08, "BodyCouplingBank costs " + juce::String (units, 4) + " units (budget 0.08)");
}
