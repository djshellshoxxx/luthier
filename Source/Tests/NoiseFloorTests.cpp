/*  Noise floor (noise-floor.md 8), group NoiseFloor.

    The DSP tests drive a LuthierEngine directly - a Stratocaster on its bridge
    single coil, volume and tone at 10, 3 m standard cable, which is the
    reference rig of 0.4 - and read the DI (Aux 1), what entered the amp, and
    the main output. The preset and style tests run the real processor.

    Levels are in dB re the reference-pluck peak measured at the DI in the same
    run, so the thresholds are the spec's own and not tied to a constant.
*/

#include "TestFramework.h"

#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../Presets/FactoryPresets.h"
#include "../Presets/RealismStyles.h"
#include "../Presets/RealismStyleActions.h"

#include <cstdio>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr int kBlock = 128;

    struct Render
    {
        std::vector<double> di, ampIn, main, aux8;
    };

    struct Rig
    {
        explicit Rig (double sampleRate = 48000.0, GuitarType type = GuitarType::Stratocaster)
            : sr (sampleRate)
        {
            engine = std::make_unique<LuthierEngine>();
            engine->prepare (sr, kBlock);
            engine->setGuitarType (type);
            engine->getPickupEngine().setSelector (PickupSelector::Bridge);
            engine->setAmpBuzzAmount (0.0);
            engine->setCircuitControls (CircuitComponents {});
            engine->getTapBuffers().setAuxWanted ((int) AuxBus::di, true);
            engine->reset();
        }

        void settings (const NoiseFloorSettings& s) { engine->setNoiseFloorSettings (s); }

        void pluckLowE (double velocity = 100.0 / 127.0)
        {
            NoteOnEvent e;
            e.stringIndex = 5;
            e.midiNote = 40;
            e.fretPosition = 0.0;
            e.pitchHz = engine->getTuningEngine().computeFrequency (5, 0.0);
            e.velocity = velocity;
            engine->triggerNoteNow (e);
        }

        template <typename PerBlock>
        Render render (double seconds, PerBlock perBlock)
        {
            Render r;
            const int blocks = (int) std::ceil (seconds * sr / kBlock);
            juce::AudioBuffer<float> buffer (2, kBlock);

            for (int b = 0; b < blocks; ++b)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                perBlock (b);
                engine->processBlock (buffer, midi);

                const float* di = engine->getTapBuffers().auxRead ((int) AuxBus::di, 0);
                const double* amp = engine->getNoiseFloor().getAmpInputRecord();
                const bool nfOn = ! engine->getNoiseFloor().isIdle();
                const double* aux8 = engine->getNoiseBusData();

                for (int i = 0; i < kBlock; ++i)
                {
                    r.di.push_back (di[i]);
                    r.ampIn.push_back (nfOn ? amp[i] : (double) di[i]);
                    r.main.push_back (buffer.getSample (0, i));
                    r.aux8.push_back (aux8[i]);
                }
            }

            return r;
        }

        Render render (double seconds) { return render (seconds, [] (int) {}); }

        double sr;
        std::unique_ptr<LuthierEngine> engine;
    };

    double rmsFrom (const std::vector<double>& x, double sr, double from, double to = 1.0e9)
    {
        const int a = juce::jlimit (0, (int) x.size(), (int) (from * sr));
        const int b = (int) juce::jlimit ((double) a, (double) x.size(), to * sr);
        return rms (x.data() + a, juce::jmax (1, b - a));
    }

    double db (double x) { return gainToDb (juce::jmax (1.0e-15, x)); }

    /** The reference pluck's DI peak, measured (0.4). */
    double referencePeak (double sr = 48000.0)
    {
        Rig rig (sr);
        rig.pluckLowE();
        const auto r = rig.render (1.0);
        return peak (r.di.data(), (int) r.di.size());
    }

    /** Magnitude of one DFT bin at `hz`, Hann-windowed. */
    double binLevel (const std::vector<double>& x, int start, int n, double sr, double hz)
    {
        double re = 0.0, im = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (constants::kTwoPi * i / (n - 1));
            const double ph = constants::kTwoPi * hz * i / sr;
            re += x[(size_t) (start + i)] * w * std::cos (ph);
            im -= x[(size_t) (start + i)] * w * std::sin (ph);
        }

        return std::sqrt (re * re + im * im) / n;
    }

    /** Energy in a band, by a sweep of DFT bins. */
    double bandEnergy (const std::vector<double>& x, int start, int n, double sr, double lo, double hi)
    {
        double sum = 0.0;
        const double step = sr / n;

        for (double f = lo; f <= hi; f += step)
        {
            const double m = binLevel (x, start, n, sr, f);
            sum += m * m;
        }

        return sum;
    }
}

//==============================================================================
LUTHIER_TEST (NoiseFloor, calibrationIsLogged)
{
    // The reference the targets are stated against, and the shipped hum
    // constant at 1.0 and at its 0.12 default (NF-02 logs both).
    const double ref = referencePeak();
    std::printf ("      reference pluck DI peak %.4f (constant %.4f)\n", ref, NoiseFloor::kReferencePluckPeak);
    CHECK_MSG (std::abs (db (ref / NoiseFloor::kReferencePluckPeak)) < 3.0,
               "the reference pluck moved: " + juce::String (ref, 4));
}

//==============================================================================
// NF-02
LUTHIER_TEST (NoiseFloor, humCalibration)
{
    const double ref = referencePeak();

    for (double amount : { 1.0, 0.12 })
    {
        Rig rig;
        rig.engine->setAmpBuzzAmount (amount);
        const auto r = rig.render (1.5);
        const double level = db (rmsFrom (r.di, rig.sr, 0.5) / ref);
        std::printf ("      hum %.2f: %.1f dB re the reference pluck\n", amount, level);

        /*  2.1's target is -40 +/-6 dB, but the shipped constant (0.0022) must
            not change, because that would re-voice every preset. Against the
            measured reference pluck it is -58 dB at 1.0, which is recorded in
            docs/coverage/REALISM-C.md and noise-floor.md 2.1; this pins it so
            a change to either is noticed. */
        if (amount == 1.0)
            CHECK_MSG (std::abs (level + 57.7) <= 2.0, "hum at 1.0 is " + juce::String (level, 1) + " dB");
    }
}

//==============================================================================
// NF-03 (and NF-17 at 44.1 and 96 kHz)
LUTHIER_TEST (NoiseFloor, regionSetsTheHumFrequency)
{
    for (double sr : { 48000.0, 44100.0, 96000.0 })
    {
        for (double mains : { 50.0, 60.0 })
        {
            Rig rig (sr);
            rig.engine->setAmpBuzzAmount (1.0);
            rig.engine->getPickupEngine().setMainsFrequency (mains);
            const auto r = rig.render (2.5);

            const int start = (int) (0.5 * sr);
            const int n = (int) (2.0 * sr);

            // The largest bin in 20-400 Hz, at 0.05 Hz resolution around the line.
            double best = 0.0, bestHz = 0.0;

            for (double f = 20.0; f <= 400.0; f += 2.0)
            {
                const double m = binLevel (r.di, start, n, sr, f);
                if (m > best) { best = m; bestHz = f; }
            }

            for (double f = bestHz - 2.0; f <= bestHz + 2.0; f += 0.05)
            {
                const double m = binLevel (r.di, start, n, sr, f);
                if (m > best) { best = m; bestHz = f; }
            }

            CHECK_MSG (std::abs (bestHz - mains) <= 0.5,
                       juce::String (sr) + " Hz, " + juce::String (mains) + " Hz mains: peak at " + juce::String (bestHz, 2));

            const double third = binLevel (r.di, start, n, sr, 3.0 * mains);
            CHECK_MSG (db (third / best) > -20.0, "the third harmonic is " + juce::String (db (third / best), 1) + " dB");
        }
    }
}

//==============================================================================
// NF-04
LUTHIER_TEST (NoiseFloor, aHumbuckerCancelsHumButNotAGroundLoop)
{
    auto noiseAt = [] (bool humbucker, bool groundLoop, bool atAmp)
    {
        Rig rig;

        if (humbucker)
        {
            auto spec = PickupSpec::makeDefault (PickupType::Humbucker, rig.engine->getGuitarSpec().pickupPositions[0]);
            rig.engine->getPickupEngine().setPickupSpec (0, spec);
        }

        NoiseFloorSettings s;

        if (groundLoop)
            s.groundLoop = 1.0;
        else
        {
            s.fluorescent = 1.0;
            rig.engine->setAmpBuzzAmount (1.0);
        }

        rig.settings (s);
        const auto r = rig.render (1.5);
        return rmsFrom (atAmp ? r.ampIn : r.di, rig.sr, 0.5);
    };

    const double single = noiseAt (false, false, false);
    const double bucker = noiseAt (true, false, false);
    CHECK_MSG (db (single / bucker) >= 40.0, "humbucker only " + juce::String (db (single / bucker), 1) + " dB quieter");

    const double loopSingle = noiseAt (false, true, true);
    const double loopBucker = noiseAt (true, true, true);
    CHECK_MSG (std::abs (db (loopSingle / loopBucker)) < 0.1,
               "a ground loop moved " + juce::String (db (loopSingle / loopBucker), 2) + " dB with the pickup");
}

//==============================================================================
// NF-05
LUTHIER_TEST (NoiseFloor, theVolumeKnobActsOnHumOnly)
{
    auto measure = [] (double volume, int source)
    {
        Rig rig;
        CircuitComponents c;
        c.volume = volume;
        rig.engine->setCircuitControls (c);

        NoiseFloorSettings s;
        if (source == 0) rig.engine->setAmpBuzzAmount (1.0);
        if (source == 1) s.groundLoop = 1.0;
        if (source == 2) s.ampHiss = 1.0;

        // A source at the amp input needs the module running; the hum alone is
        // measured at the DI, which is the amp input with no pedals.
        rig.settings (s);
        const auto r = rig.render (1.5);
        return rmsFrom (source == 0 ? r.di : r.ampIn, rig.sr, 0.5);
    };

    CHECK_MSG (db (measure (1.0, 0) / measure (0.0, 0)) >= 40.0, "hum does not follow the volume knob");
    CHECK (std::abs (db (measure (1.0, 1) / measure (0.0, 1))) < 0.5);
    CHECK (std::abs (db (measure (1.0, 2) / measure (0.0, 2))) < 0.5);
}

//==============================================================================
// NF-06 (and NF-17)
LUTHIER_TEST (NoiseFloor, positionScalesTheHum)
{
    for (double sr : { 48000.0, 44100.0, 96000.0 })
    {
        auto hum = [sr] (double angle, double distance)
        {
            Rig rig (sr);
            rig.engine->setAmpBuzzAmount (1.0);
            NoiseFloorSettings s;
            s.angleDegrees = angle;
            s.distanceMetres = distance;
            rig.settings (s);
            const auto r = rig.render (1.0);
            return rmsFrom (r.di, sr, 0.4);
        };

        const double neutral = hum (0.0, 1.0);
        const double turned = db (hum (90.0, 1.0) / neutral);
        const double far = db (hum (0.0, 5.0) / neutral);
        const double near = db (hum (0.0, 0.3) / neutral);

        CHECK_MSG (turned <= -15.0 && turned >= -21.0, "90 degrees: " + juce::String (turned, 2));
        CHECK_MSG (far <= -6.5 && far >= -9.0, "5 m: " + juce::String (far, 2));
        CHECK_MSG (near >= 15.0 && near <= 19.0, "0.3 m: " + juce::String (near, 2));
    }

    // At (0, 1 m) the hum is the legacy path, sample for sample.
    Rig a, b;
    a.engine->setAmpBuzzAmount (1.0);
    b.engine->setAmpBuzzAmount (1.0);
    b.engine->setNoiseFloorBypassedForTest (true);
    const auto ra = a.render (0.5), rb = b.render (0.5);
    CHECK (ra.di == rb.di);
    CHECK (NoiseFloor::positionGain (0.0, 1.0) == 1.0);
}

//==============================================================================
// NF-07 (and NF-17)
LUTHIER_TEST (NoiseFloor, ampGainRaisesHiss)
{
    for (double sr : { 48000.0, 44100.0, 96000.0 })
    {
        auto hissAt = [sr] (double gain, bool output)
        {
            Rig rig (sr);
            rig.engine->getAmpEngine().setModel (AmpModel::MesaRectifier);
            rig.engine->getAmpEngine().setGain (gain);
            NoiseFloorSettings s;
            s.ampHiss = 0.5;
            rig.settings (s);
            const auto r = rig.render (2.0);
            return rmsFrom (output ? r.main : r.ampIn, sr, 1.0);
        };

        const double rise = db (hissAt (0.9, true) / hissAt (0.2, true));
        CHECK_MSG (rise >= 15.0, juce::String (sr) + ": gain 0.9 over 0.2 raises the hiss only "
                                   + juce::String (rise, 1) + " dB");

        const double inputReferred = db (hissAt (0.5, false) / NoiseFloor::kReferencePluckPeak);
        CHECK_MSG (std::abs (inputReferred + 100.0) <= 2.0, "input-referred hiss " + juce::String (inputReferred, 2) + " dB");
    }
}

//==============================================================================
// NF-08
LUTHIER_TEST (NoiseFloor, microphonicsIsBounded)
{
    /*  Stock max, around a linear stand-in for the amp at a clean (x4) and a
        high (x200) gain: one impulse in, and the microphonic component (the
        module's amp-input output) decays 60 dB within 500 ms. The loop gain is
        the parameter's (G_m = 0.5), whatever the amp's own gain, because the
        module divides the measured gain at the resonance out (2.8).

        In the engine a choked note keeps feeding the resonator through the
        body's tail, so the component there tracks its source rather than
        decaying on its own; this isolates the loop, which is what "bounded by
        construction" is about. */
    for (double ampGain : { 4.0, 200.0 })
    {
        NoiseFloor nf;
        nf.prepare (48000.0, kBlock);
        NoiseFloorSettings s;
        s.microphonics = 1.0;
        nf.setSettings (s);
        CircuitComponents c;

        std::vector<double> component;
        std::array<double, kBlock> out {};

        for (int b = 0; b < (int) (1.0 * 48000.0 / kBlock); ++b)
        {
            nf.beginBlock (kBlock, 1.0, c, false, false);

            for (int i = 0; i < kBlock; ++i)
            {
                const double drive = (b == 0 && i == 0) ? 0.1 : 0.0;
                const double in = drive + nf.ampInSample (i);
                nf.recordAmpInput (i, in);
                out[(size_t) i] = std::tanh (ampGain * in);
                component.push_back (nf.ampInSample (i));
            }

            nf.pushAmpOutput (out.data(), kBlock);
        }

        const double sr = 48000.0;
        const double early = peak (component.data(), (int) (0.05 * sr));
        const double late = peak (component.data() + (int) (0.5 * sr), (int) (0.05 * sr));
        std::printf ("      amp x%g: ping %.1f dB re the reference, %.1f dB after 500 ms\n", ampGain,
                     db (early / NoiseFloor::kReferencePluckPeak), db (late / juce::jmax (1.0e-15, early)));
        CHECK_MSG (early > 0.0, "the microphonic path made nothing");
        CHECK_MSG (db (late / juce::jmax (1.0e-15, early)) <= -60.0,
                   "at x" + juce::String (ampGain) + " the component did not decay: " + juce::String (db (late / early), 1) + " dB");
    }

    // Advanced max: each second's RMS over 10 s never rises, every sample finite.
    Rig rig;
    NoiseFloorSettings s;
    s.microphonics = 1.9;
    rig.settings (s);
    rig.pluckLowE (1.0);
    const auto r = rig.render (10.0);
    CHECK_FINITE (r.main.data(), (int) r.main.size());

    double previous = 1.0e9;
    for (int second = 0; second < 10; ++second)
    {
        const double level = rmsFrom (r.main, rig.sr, second, second + 1);
        CHECK_MSG (level <= previous * 1.0001, "second " + juce::String (second) + " rose");
        previous = level;
    }
}

//==============================================================================
// NF-09
LUTHIER_TEST (NoiseFloor, rendersAreDeterministic)
{
    auto run = [] (uint64_t seed, bool everySource)
    {
        Rig rig;
        rig.engine->getCharacterEngine().setSeed (seed);
        NoiseFloorSettings s;
        s.cableMovement = s.radio = 1.0;

        if (everySource)
            s.fluorescent = s.passiveHiss = s.groundLoop = s.ampHiss = s.microphonics = 1.0;

        rig.settings (s);
        rig.engine->reset();

        const auto r = rig.render (3.0, [&rig] (int block)
        {
            if (block % 40 == 0)
                for (int k = 0; k < 20; ++k)
                    rig.engine->getNoiseFloor().onNoteOn (1.0);   // many cable rolls
        });

        // Cable (at the DI) and radio (at the amp input), together.
        std::vector<double> cableAndRadio (r.ampIn.size());
        for (size_t i = 0; i < cableAndRadio.size(); ++i)
            cableAndRadio[i] = r.ampIn[i];

        return std::make_pair (r, cableAndRadio);
    };

    const auto a = run (1234, true), b = run (1234, true);
    CHECK (a.first.main == b.first.main);
    CHECK (a.first.ampIn == b.first.ampIn);

    auto correlation = [] (const std::vector<double>& x, const std::vector<double>& y)
    {
        double xy = 0.0, xx = 0.0, yy = 0.0;
        for (size_t i = 0; i < x.size(); ++i) { xy += x[i] * y[i]; xx += x[i] * x[i]; yy += y[i] * y[i]; }
        return xy / std::sqrt (juce::jmax (1.0e-30, xx * yy));
    };

    const auto c = run (1234, false), d = run (98765, false);
    const double r = correlation (c.second, d.second);
    CHECK_MSG (std::abs (r) < 0.5, "two seeds correlate: " + juce::String (r, 3));
}

//==============================================================================
// NF-10
LUTHIER_TEST (NoiseFloor, cableMovementRollsAndScales)
{

    // Off: zero events, measured as zero cable output over the whole run.
    {
        NoiseFloor nf;
        nf.prepare (48000.0, kBlock);
        NoiseFloorSettings s;
        s.cableMovement = 1.0;
        nf.setSettings (s);
        CircuitComponents c;
        c.cableOn = false;
        double energy = 0.0;

        for (int i = 0; i < 1000; ++i)
        {
            nf.onNoteOn (1.0);
            nf.beginBlock (kBlock, 1.0, c, false, false);
            for (int k = 0; k < kBlock; ++k) energy += nf.diSample (k) * nf.diSample (k);
        }

        CHECK_MSG (energy == 0.0, "a cable that is off made noise");
    }

    // On: 1000 note-ons at v = 1 give 10-32 events.
    {
        NoiseFloor nf;
        nf.prepare (48000.0, kBlock);
        NoiseFloorSettings s;
        s.cableMovement = 1.0;
        nf.setSettings (s);
        for (int i = 0; i < 1000; ++i) nf.onNoteOn (1.0);
        const int count = nf.getCableEventCount();
        CHECK_MSG (count >= 10 && count <= 32, "events: " + juce::String (count));
    }

    // Length and quality scale the level: mean event peak.
    auto meanPeak = [] (double length, CableQuality quality)
    {
        NoiseFloor nf;
        nf.prepare (48000.0, kBlock);
        NoiseFloorSettings s;
        s.cableMovement = 4.0;
        nf.setSettings (s);
        CircuitComponents c;
        c.cableLength = length;
        c.cableQuality = quality;

        double sum = 0.0;
        int n = 0;

        for (int note = 0; note < 400; ++note)
        {
            const int before = nf.getCableEventCount();
            nf.onNoteOn (1.0);
            double p = 0.0;
            for (int b = 0; b < 200; ++b)
            {
                nf.beginBlock (kBlock, 1.0, c, false, false);
                for (int k = 0; k < kBlock; ++k) p = juce::jmax (p, std::abs (nf.diSample (k)));
            }
            if (nf.getCableEventCount() > before) { sum += p; ++n; }
        }

        return n > 0 ? sum / n : 0.0;
    };

    const double standard3 = meanPeak (3.0, CableQuality::standard);
    CHECK_NEAR (db (meanPeak (6.0, CableQuality::standard) / standard3), 6.0, 1.0);
    CHECK_NEAR (db (meanPeak (3.0, CableQuality::cheap) / meanPeak (3.0, CableQuality::studio)), 12.0, 1.0);

    // Target at 1.0 (x4 here): peaks -35 to -50 dB re the reference pluck.
    const double at1 = db (standard3 / 4.0 / NoiseFloor::kReferencePluckPeak);
    std::printf ("      cable mean peak at 1.0: %.1f dB\n", at1);
    CHECK_MSG (at1 <= -35.0 && at1 >= -50.0, "cable peaks " + juce::String (at1, 1) + " dB");
}

//==============================================================================
// NF-11
LUTHIER_TEST (NoiseFloor, passiveHissIsPhysical)
{
    Rig rig;
    NoiseFloorSettings s;
    s.passiveHiss = 1.0;
    rig.settings (s);
    const auto r = rig.render (2.5);

    const auto parts = rig.engine->getLiveCircuitComponents();

    /*  The physical figure: Johnson noise has a flat density of 4kTR V^2/Hz,
        so over the audio band it is sqrt (4kTRB). Through the circuit, the
        expected DI power is that density times the integral of |H|^2. The DI
        is the discrete (bilinear) circuit, whose response at f is exactly
        GuitarCircuit::response at the pre-warped analogue frequency, so the
        integral is taken over 0..sr/2 through the warp. */
    const double density = std::pow (NoiseFloor::johnsonVoltsRms (NoiseFloor::hissResistance (parts), 1.0)
                                       / NoiseFloor::kEmfVoltsPerUnit, 2.0);
    double integral = 0.0;
    const double df = 5.0;

    for (double f = df * 0.5; f < rig.sr * 0.5; f += df)
    {
        const double analogue = rig.sr / constants::kPi * std::tan (constants::kPi * f / rig.sr);
        integral += std::norm (GuitarCircuit::response (parts, analogue)) * df;
    }

    const double expected = std::sqrt (density * integral);
    const double measured = rmsFrom (r.di, rig.sr, 0.5);
    CHECK_MSG (std::abs (db (measured / expected)) <= 2.0,
               "passive hiss " + juce::String (db (measured / expected), 2) + " dB off the physical figure");

    // Its colour: the spectral peak at the circuit's resonance, +/-15%.
    const int start = (int) (0.5 * rig.sr), n = (int) (2.0 * rig.sr);
    double best = 0.0, bestHz = 0.0;
    for (double f = 1000.0; f <= 12000.0; f += 50.0)
    {
        // A little averaging, because one bin of noise is noise.
        double m = 0.0;
        for (double g = f - 100.0; g <= f + 100.0; g += 25.0) m += binLevel (r.di, start, n, rig.sr, g);
        if (m > best) { best = m; bestHz = f; }
    }

    const double resonance = GuitarCircuit::findResonantPeakHz (parts);
    CHECK_MSG (std::abs (bestHz / resonance - 1.0) <= 0.15,
               "hiss peaks at " + juce::String (bestHz) + " Hz, circuit at " + juce::String (resonance));

    std::printf ("      passive hiss at 1.0: %.1f dB re the reference pluck\n", db (measured / NoiseFloor::kReferencePluckPeak));
}

//==============================================================================
// NF-12
LUTHIER_TEST (NoiseFloor, fluorescentSpectrum)
{
    Rig rig;
    NoiseFloorSettings s;
    s.fluorescent = 1.0;
    rig.settings (s);
    const auto r = rig.render (2.5);

    const int start = (int) (0.5 * rig.sr), n = (int) (2.0 * rig.sr);
    const double line = binLevel (r.di, start, n, rig.sr, 120.0);
    const double beside = binLevel (r.di, start, n, rig.sr, 90.0);
    CHECK_MSG (line > 4.0 * beside, "no line at twice the mains");

    const int shortN = (int) (0.25 * rig.sr);
    const double high = bandEnergy (r.di, start, shortN, rig.sr, 2000.0, 6000.0);
    const double low = bandEnergy (r.di, start, shortN, rig.sr, 300.0, 1000.0);
    CHECK_MSG (10.0 * std::log10 (high / low) >= 6.0, "2-6 kHz only " + juce::String (10.0 * std::log10 (high / low), 1) + " dB over 300-1000 Hz");

    const double level = db (rmsFrom (r.di, rig.sr, 0.5) / referencePeak());
    std::printf ("      fluorescent at 1.0: %.1f dB\n", level);
    CHECK_MSG (std::abs (level + 45.0) <= 6.0, "fluorescent " + juce::String (level, 1) + " dB");
}

//==============================================================================
// Levels of the amp-input sources (2.5, 2.6)
LUTHIER_TEST (NoiseFloor, ampInputSourcesHitTheirTargets)
{
    auto level = [] (bool ground)
    {
        Rig rig;
        NoiseFloorSettings s;
        (ground ? s.groundLoop : s.radio) = 1.0;
        rig.settings (s);
        const auto r = rig.render (12.0);
        return db (rmsFrom (r.ampIn, rig.sr, 0.5) / NoiseFloor::kReferencePluckPeak);
    };

    const double ground = level (true), radio = level (false);
    std::printf ("      ground loop %.1f dB, radio %.1f dB\n", ground, radio);
    CHECK (std::abs (ground + 45.0) <= 6.0);
    CHECK (std::abs (radio + 55.0) <= 6.0);
}

//==============================================================================
// NF-13
LUTHIER_TEST (NoiseFloor, aux8IsOptIn)
{
    Rig off, legacy;
    legacy.engine->setNoiseFloorBypassedForTest (true);
    off.pluckLowE();
    legacy.pluckLowE();
    CHECK (off.render (0.5).aux8 == legacy.render (0.5).aux8);

    Rig on;
    NoiseFloorSettings s;
    s.groundLoop = 1.0;
    s.toAux8 = true;
    on.settings (s);
    CHECK (rmsFrom (on.render (0.5).aux8, on.sr, 0.1) > 0.0);
}

//==============================================================================
// NF-15
LUTHIER_TEST (NoiseFloor, idleIsFree)
{
    NoiseFloor nf;
    nf.prepare (48000.0, kBlock);
    CHECK (nf.isIdle());

    // Everything below is a machine-relative CPU budget (measured, then asserted): run under
    // LUTHIER_PERF=1 (the nightly) only. The idle-state check above runs everywhere.
    if (! luthier::tests::perfRunRequested())
        return;

    // Cost over the baseline: 20 s with the module idle against bypassed, on
    // the thread's CPU clock and interleaved best-of-two, so a shared machine's
    // scheduling noise does not land on one side of the difference.
    auto time = [] (bool bypass)
    {
        Rig rig;
        rig.engine->setNoiseFloorBypassedForTest (bypass);
        const double t0 = threadCpuTimeSeconds();
        rig.render (20.0);
        return 1000.0 * (threadCpuTimeSeconds() - t0);
    };

    double idle = 1.0e9, bypassed = 1.0e9;

    for (int round = 0; round < 2; ++round)
    {
        idle = juce::jmin (idle, time (false));
        bypassed = juce::jmin (bypassed, time (true));
    }
    // A unit is one real-time core at 48 kHz (performance-budget.md 1); 20 s of
    // audio is 20000 ms of it. Timing noise on a shared machine is larger than
    // the budget, so the check is generous and the figure is logged.
    const double units = (idle - bypassed) / 20000.0;
    std::printf ("      idle cost %.4f units\n", units);
    CHECK_MSG (units < 0.02, "idle noise floor costs " + juce::String (units, 4) + " units");
}

//==============================================================================
// NF-01
LUTHIER_TEST (NoiseFloor, defaultsAreBitIdentical)
{
    for (int index : { 0, 3, 7 })
    {
        auto run = [index] (bool bypass)
        {
            LuthierAudioProcessor processor;
            processor.prepareToPlay (48000.0, kBlock);
            FactoryPresets::setProcessorForRanges (&processor);
            processor.getPresetManager().fromVar (FactoryPresets::toVar (FactoryPresets::getPreset (index), processor));
            processor.getParameterBridge().applyAllNow();
            processor.getEngine().setNoiseFloorBypassedForTest (bypass);
            processor.getEngine().reset();

            std::vector<float> out;
            juce::AudioBuffer<float> buffer (2, kBlock);

            for (int b = 0; b < (int) (10.0 * 48000.0 / kBlock); ++b)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                const int step = b % 375;
                if (step == 0)   midi.addEvent (juce::MidiMessage::noteOn (1, 40 + (b / 375) % 12, (juce::uint8) 100), 0);
                if (step == 10)  midi.addEvent (juce::MidiMessage::noteOn (1, 47 + (b / 375) % 12, (juce::uint8) 70), 5);
                if (step == 200) midi.addEvent (juce::MidiMessage::noteOff (1, 40 + (b / 375) % 12), 0);
                if (step == 300) midi.addEvent (juce::MidiMessage::noteOff (1, 47 + (b / 375) % 12), 0);
                processor.processBlock (buffer, midi);

                for (int i = 0; i < kBlock; ++i)
                    out.push_back (buffer.getSample (0, i));
            }

            FactoryPresets::setProcessorForRanges (nullptr);
            return out;
        };

        CHECK_MSG (run (false) == run (true), "preset " + juce::String (index) + " changed with the noise floor at its defaults");
    }
}

//==============================================================================
// NF-14
LUTHIER_TEST (NoiseFloor, styleOffIsInert)
{
    LuthierAudioProcessor processor;
    auto& state = processor.getState();

    auto set = [&state] (const char* id, double plain)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id)))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) plain));
    };

    auto get = [&state] (const char* id)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id));
        return (double) p->convertFrom0to1 (p->getValue());
    };

    set (ParamIDs::ampBuzz, 0.33);
    applyNoiseFloorStyle (processor, 2);
    CHECK_NEAR (get (ParamIDs::noiseFluorescent), 0.15, 1.0e-4);
    applyNoiseFloorStyle (processor, 0);

    for (auto* id : { ParamIDs::noiseFluorescent, ParamIDs::noisePassiveHiss, ParamIDs::noiseCableMovement,
                      ParamIDs::noiseRadio, ParamIDs::noiseGroundLoop, ParamIDs::noiseAmpHiss, ParamIDs::noiseMicrophonics })
        CHECK_MSG (get (id) == 0.0, juce::String (id) + " is not 0 after Off");

    CHECK_NEAR (get (ParamIDs::ampBuzz), 0.25, 1.0e-4);   // Home desk's hum stays: Off leaves it unchanged
}




//==============================================================================
namespace luthier::tests { long realismCAllocationCount() noexcept; }

// NF-16
LUTHIER_TEST (NoiseFloor, noAllocationOnTheAudioPath)
{
    Rig rig;
    NoiseFloorSettings s;
    s.fluorescent = s.passiveHiss = s.cableMovement = s.radio = s.groundLoop = s.ampHiss = s.microphonics = 1.0;
    rig.settings (s);
    rig.pluckLowE();
    rig.render (0.5);   // warm-up: anything lazily sized is sized now

    const long before = realismCAllocationCount();

    rig.render (60.0, [&rig, s] (int block) mutable
    {
        if (block % 200 == 0)
            rig.pluckLowE();

        // A region change and a style change, as the bridge would apply them.
        if (block == 5000)
        {
            s.mainsHz = 50.0;
            rig.engine->getPickupEngine().setMainsFrequency (50.0);
            rig.settings (s);
        }

        if (block == 10000)
        {
            const auto& style = RealismStyles::getNoiseFloorStyle (3);
            s.fluorescent = style.fluorescent; s.cableMovement = style.cable; s.radio = style.radio;
            s.groundLoop = style.groundLoop; s.ampHiss = style.ampHiss; s.microphonics = style.microphonics;
            rig.settings (s);
        }
    });

    // The render's own vectors grew, which shows the counter counts; the
    // engine alone is measured below, into preallocated storage.
    CHECK_MSG (realismCAllocationCount() > before, "the allocation counter is not counting");

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer midi;
    midi.ensureSize (1024);
    const long start = realismCAllocationCount();

    for (int b = 0; b < (int) (60.0 * 48000.0 / kBlock); ++b)
    {
        buffer.clear();
        if (b == 3000)
        {
            s.mainsHz = 60.0;
            rig.engine->getPickupEngine().setMainsFrequency (60.0);
            rig.settings (s);
        }
        rig.engine->processBlock (buffer, midi);
    }

    CHECK_MSG (realismCAllocationCount() == start,
               juce::String (realismCAllocationCount() - start) + " allocations on the audio path");
}
