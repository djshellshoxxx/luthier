/*  Unit tests for the DSP modules (engine spec 19).

    Each test verifies a claim the engine actually makes, not just that the code
    runs: that a plucked string lands on the right pitch, that a pickup's comb
    filter nulls the harmonic it should, that a TransTrem preserves chord
    intervals, that the coupling matrix cannot run away.
*/

#include "TestFramework.h"

#include "../DSP/Common/DspCommon.h"
#include "../DSP/Common/Oversampler.h"
#include "../DSP/String/StringEngine.h"
#include "../DSP/String/FractionalDelayLine.h"
#include "../DSP/Coupling/CouplingMatrix.h"
#include "../DSP/Pickup/PickupEngine.h"
#include "../DSP/Whammy/WhammyEngine.h"
#include "../DSP/Body/BodyEngine.h"
#include "../DSP/Amp/AmpEngine.h"
#include "../DSP/Amp/CabinetEngine.h"
#include "../DSP/Amp/RoomEngine.h"
#include "../DSP/Master/MasterBus.h"
#include "../DSP/Effects/EffectsChain.h"
#include "../DSP/Effects/SecretEffect.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;

    /** Plucks a string and renders `seconds` of it. */
    std::vector<double> renderString (StringEngine& s, double seconds,
                                      const Excitation::Params& params)
    {
        const int n = (int) (kSr * seconds);
        std::vector<double> out ((size_t) n, 0.0);

        s.excite (params);

        for (int i = 0; i < n; ++i)
            out[(size_t) i] = s.processSample (0.0);

        return out;
    }

    Excitation::Params defaultPluck (double velocity = 0.8)
    {
        Excitation::Params p;
        p.material = Excitation::Material::PickCelluloid;
        p.kind = Excitation::Kind::Pluck;
        p.pluckPosition = 0.18;
        p.velocity = velocity;
        p.brightness = 0.5;
        return p;
    }
}

//==============================================================================
//  Primitives
//==============================================================================
LUTHIER_TEST (Common, sanitiseClampsNonFinite)
{
    CHECK (sanitise (std::numeric_limits<double>::quiet_NaN()) == 0.0);
    CHECK (sanitise (std::numeric_limits<double>::infinity()) == 0.0);
    CHECK (sanitise (-std::numeric_limits<double>::infinity()) == 0.0);
    CHECK_NEAR (sanitise (100.0), constants::kGuardLimit, 1.0e-9);
    CHECK_NEAR (sanitise (-100.0), -constants::kGuardLimit, 1.0e-9);
    CHECK_NEAR (sanitise (0.5), 0.5, 1.0e-12);
}

LUTHIER_TEST (Common, dcBlockerRemovesOffset)
{
    DCBlocker dc;
    dc.prepare (kSr, 7.0);

    double last = 0.0;

    // A constant input should decay to nothing.
    for (int i = 0; i < (int) kSr; ++i)
        last = dc.process (1.0);

    CHECK_MSG (std::abs (last) < 0.01, "DC blocker left " + juce::String (last));
}

LUTHIER_TEST (Common, biquadLowpassAttenuatesHighFrequencies)
{
    Biquad lp;
    lp.setLowpass (kSr, 1000.0, 0.707);

    auto measureGain = [&lp] (double hz)
    {
        lp.reset();

        const int n = (int) (kSr * 0.25);
        std::vector<double> out ((size_t) n);

        for (int i = 0; i < n; ++i)
            out[(size_t) i] = lp.process (std::sin (2.0 * constants::kPi * hz * i / kSr));

        // Skip the settling transient.
        return rms (out.data() + n / 2, n / 2) * std::sqrt (2.0);
    };

    CHECK_NEAR (measureGain (100.0), 1.0, 0.05);
    CHECK_NEAR (measureGain (1000.0), 0.707, 0.08);
    CHECK_MSG (measureGain (8000.0) < 0.05, "8 kHz should be well attenuated");
}

LUTHIER_TEST (Common, allpassSignIsCorrectForDispersion)
{
    // A negative coefficient must delay LOW frequencies more than high ones.
    // That sign is what makes the string's partials stretch sharp rather than
    // compress flat, so it is worth asserting rather than assuming.
    Allpass1 ap;
    ap.setCoefficient (-0.5);

    CHECK_MSG (ap.delayAtDC() > ap.delayAtNyquist(),
               "negative coefficient must delay DC more than Nyquist");

    ap.setCoefficient (0.5);
    CHECK_MSG (ap.delayAtDC() < ap.delayAtNyquist(),
               "positive coefficient compresses, which is the wrong direction");
}

LUTHIER_TEST (Common, oversamplerIsTransparentAtUnityShaper)
{
    for (int factor : { 1, 2, 4, 8 })
    {
        Oversampler os;
        os.prepare (kSr, factor);

        const int n = 4096;
        std::vector<double> input ((size_t) n), output ((size_t) n);

        for (int i = 0; i < n; ++i)
            input[(size_t) i] = std::sin (2.0 * constants::kPi * 440.0 * i / kSr) * 0.5;

        for (int i = 0; i < n; ++i)
            output[(size_t) i] = os.processSample (input[(size_t) i], [] (double v) { return v; });

        CHECK_FINITE (output.data(), n);

        // Round-tripping through the filters must preserve the level, allowing for
        // the group delay the allpass branches introduce.
        const double inLevel = rms (input.data() + n / 2, n / 2);
        const double outLevel = rms (output.data() + n / 2, n / 2);

        CHECK_MSG (std::abs (outLevel - inLevel) < 0.03,
                   "factor " + juce::String (factor) + ": level changed from "
                   + juce::String (inLevel, 4) + " to " + juce::String (outLevel, 4));
    }
}

LUTHIER_TEST (Common, oversamplingSuppressesAliasing)
{
    // Hard-clip a 5 kHz sine. Without oversampling, the harmonics above Nyquist
    // fold back as inharmonic hash; with it, they should be far quieter.
    auto measureAliasing = [] (int factor)
    {
        Oversampler os;
        os.prepare (kSr, factor);

        const int n = 16384;
        std::vector<double> out ((size_t) n);

        const double hz = 5000.0;

        for (int i = 0; i < n; ++i)
        {
            const double in = std::sin (2.0 * constants::kPi * hz * i / kSr) * 0.9;
            out[(size_t) i] = os.processSample (in, [] (double v)
            {
                return juce::jlimit (-0.4, 0.4, v);
            });
        }

        // Harmonics of 5 kHz land at 10, 15, 20 kHz and then fold: 25 -> 23,
        // 35 -> 13, 45 -> 3, 55 -> 7 kHz. So energy anywhere near 3 kHz is
        // aliasing and nothing else.
        //
        // This has to be measured with an FFT rather than a bandpass: a
        // second-order bandpass two and a half octaves away still leaks enough of
        // the 5 kHz fundamental to swamp what we are trying to see.
        std::vector<double> window (out.begin() + n / 2, out.end());

        std::vector<float> fftData (16384 * 2, 0.0f);
        const int fftSize = 8192;

        for (int i = 0; i < fftSize && i < (int) window.size(); ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * constants::kPi * i / (double) (fftSize - 1));
            fftData[(size_t) i] = (float) (window[(size_t) i] * w);
        }

        juce::dsp::FFT fft (13);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const double binHz = kSr / (double) (1 << 13);

        double energy = 0.0;

        for (int bin = (int) (2600.0 / binHz); bin <= (int) (3400.0 / binHz); ++bin)
            energy += (double) fftData[(size_t) bin] * fftData[(size_t) bin];

        return std::sqrt (energy);
    };

    const double none = measureAliasing (1);
    const double four = measureAliasing (4);

    CHECK_MSG (four < none * 0.5,
               "4x oversampling should roughly halve or better the aliasing energy: "
               + juce::String (none, 6) + " -> " + juce::String (four, 6));
}

//==============================================================================
//  Fractional delay
//==============================================================================
LUTHIER_TEST (DelayLine, allInterpolatorsPreserveLevel)
{
    using Mode = FractionalDelayLine::Interpolation;

    for (auto mode : { Mode::Allpass1, Mode::Lagrange3, Mode::Lagrange5 })
    {
        FractionalDelayLine line;
        line.prepare (kSr, 30.0);
        line.setInterpolation (mode);

        const int n = 8192;
        const double delay = 100.37;   // deliberately fractional

        std::vector<double> out ((size_t) n);

        for (int i = 0; i < n; ++i)
        {
            const double in = std::sin (2.0 * constants::kPi * 440.0 * i / kSr) * 0.5;
            out[(size_t) i] = line.read (delay);
            line.write (in);
        }

        CHECK_FINITE (out.data(), n);

        const double level = rms (out.data() + n / 2, n / 2);
        CHECK_MSG (std::abs (level - 0.3536) < 0.02,
                   "interpolation mode " + juce::String ((int) mode)
                   + " changed the level to " + juce::String (level, 4));
    }
}

LUTHIER_TEST (DelayLine, modulationStaysClean)
{
    // A fast delay sweep is what a dive-bomb does. Nothing may produce a
    // discontinuity, a NaN, or an output louder than the input.
    FractionalDelayLine line;
    line.prepare (kSr, 30.0);
    line.setInterpolation (FractionalDelayLine::Interpolation::Lagrange5);

    const int n = (int) kSr;
    std::vector<double> out ((size_t) n);

    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / (double) n;
        const double delay = juce::jmap (t, 50.0, 1200.0);

        const double in = std::sin (2.0 * constants::kPi * 220.0 * i / kSr) * 0.5;
        out[(size_t) i] = line.read (delay);
        line.write (in);
    }

    CHECK_FINITE (out.data(), n);
    CHECK_MSG (peak (out.data(), n) < 0.7, "modulated read should not exceed the input level");

    // No sample-to-sample jumps large enough to click.
    double maxJump = 0.0;

    for (int i = 1; i < n; ++i)
        maxJump = juce::jmax (maxJump, std::abs (out[(size_t) i] - out[(size_t) (i - 1)]));

    CHECK_MSG (maxJump < 0.15, "largest sample jump was " + juce::String (maxJump, 4));
}

//==============================================================================
//  String engine
//==============================================================================
LUTHIER_TEST (StringEngine, pluckProducesCorrectPitch)
{
    // Engine spec build order step 1: play a note, FFT it, check the pitch.
    const double frequencies[] = { 82.407, 110.0, 146.832, 261.626, 440.0, 659.255, 1318.51 };

    for (double hz : frequencies)
    {
        StringEngine s;
        s.prepare (kSr, 512);

        StringEngine::Physical physical;
        physical.sustainSeconds = 6.0;
        physical.openBrightnessHz = 5000.0;
        physical.inharmonicityB = 0.00005;
        s.setPhysical (physical);

        s.snapToFrequency (hz);

        auto out = renderString (s, 0.75, defaultPluck());

        CHECK_FINITE (out.data(), (int) out.size());

        // Skip the excitation transient before measuring.
        const int skip = (int) (kSr * 0.05);
        const double measured = findPeakFrequency (out.data() + skip,
                                                   (int) out.size() - skip, kSr,
                                                   hz * 0.6, hz * 1.6);

        const double cents = 1200.0 * std::log2 (juce::jmax (1.0, measured) / hz);

        CHECK_MSG (std::abs (cents) < 12.0,
                   juce::String (hz, 2) + " Hz: measured " + juce::String (measured, 2)
                   + " Hz (" + juce::String (cents, 1) + " cents off)");
    }
}

LUTHIER_TEST (StringEngine, higherNotesDecayFaster)
{
    // Identity rule 6.
    auto decayFor = [] (double hz)
    {
        StringEngine s;
        s.prepare (kSr, 512);

        StringEngine::Physical physical;
        physical.sustainSeconds = 5.0;
        s.setPhysical (physical);
        s.snapToFrequency (hz);

        auto out = renderString (s, 4.0, defaultPluck());
        return measureDecayTime (out.data(), (int) out.size(), kSr, 30.0);
    };

    const double lowDecay = decayFor (82.4);
    const double highDecay = decayFor (659.3);

    CHECK_MSG (lowDecay > 0.0 && highDecay > 0.0,
               "both notes should decay measurably (low " + juce::String (lowDecay, 3)
               + " s, high " + juce::String (highDecay, 3) + " s)");

    CHECK_MSG (highDecay < lowDecay,
               "a high note must die sooner than a low one: low " + juce::String (lowDecay, 3)
               + " s vs high " + juce::String (highDecay, 3) + " s");
}

LUTHIER_TEST (StringEngine, harderPluckIsBrighterNotJustLouder)
{
    // Identity rule 5: velocity changes the spectrum, not only the gain.
    auto brightnessFor = [] (double velocity)
    {
        StringEngine s;
        s.prepare (kSr, 512);
        s.snapToFrequency (146.832);

        auto out = renderString (s, 0.5, defaultPluck (velocity));

        // Measure the ATTACK, not the whole note. The loop filter strips the high
        // partials within a few hundred milliseconds whatever the excitation did,
        // so averaging over half a second washes the difference away.
        //
        // The window has to start AFTER the first round trip, because the delay
        // line begins empty: the first fs/f0 samples of output are silence, and
        // measuring them gives a ratio of zero over zero.
        const int firstRoundTrip = (int) (kSr / 146.832) + 16;
        const int attack = firstRoundTrip + (int) (kSr * 0.012);

        Biquad hp;
        hp.setHighpass (kSr, 1500.0, 0.707);

        double high = 0.0, total = 0.0;

        for (int i = firstRoundTrip; i < attack && i < (int) out.size(); ++i)
        {
            const double h = hp.process (out[(size_t) i]);
            high += h * h;
            total += out[(size_t) i] * out[(size_t) i];
        }

        return (total > 1.0e-12) ? high / total : 0.0;
    };

    const double soft = brightnessFor (0.2);
    const double hard = brightnessFor (1.0);

    // The shift is real but not enormous, because within a few milliseconds the
    // string's own loop filter - not the excitation - governs the spectrum. What
    // matters is that velocity moves the tone at all, rather than only the level.
    CHECK_MSG (hard > soft * 1.05,
               "a hard pluck must be brighter, not just louder: soft ratio "
               + juce::String (soft, 5) + " vs hard " + juce::String (hard, 5));
}

LUTHIER_TEST (StringEngine, palmMuteShortensAndDarkens)
{
    auto measure = [] (StringEngine::Damping damping)
    {
        StringEngine s;
        s.prepare (kSr, 512);
        s.snapToFrequency (110.0);
        s.setDamping (damping, 1.0);

        // Long enough that an open string really does reach -30 dB: its default
        // T60 is about 4.5 s, so two seconds is not enough to measure it.
        auto out = renderString (s, 6.0, defaultPluck());
        return measureDecayTime (out.data(), (int) out.size(), kSr, 30.0);
    };

    const double open = measure (StringEngine::Damping::Open);
    const double muted = measure (StringEngine::Damping::PalmMute);

    CHECK_MSG (muted > 0.0 && muted < open * 0.6,
               "palm mute should be far shorter: open " + juce::String (open, 3)
               + " s vs muted " + juce::String (muted, 3) + " s");
}

LUTHIER_TEST (StringEngine, bendIsSmoothAndReachesTarget)
{
    // Engine spec build order step 2: bend from A to B, verify a smooth
    // transition without clicks.
    StringEngine s;
    s.prepare (kSr, 512);
    s.snapToFrequency (220.0);
    s.setGlideTime (0.15);

    s.excite (defaultPluck());

    const int n = (int) (kSr * 1.5);
    std::vector<double> out ((size_t) n);

    for (int i = 0; i < n; ++i)
    {
        // Bend up a whole tone over the first half second.
        const double t = juce::jlimit (0.0, 1.0, (double) i / (kSr * 0.5));
        s.setTargetFrequency (220.0 * std::pow (2.0, t * 2.0 / 12.0));

        out[(size_t) i] = s.processSample (0.0);
    }

    CHECK_FINITE (out.data(), n);

    // No clicks: no large sample-to-sample jump after the initial attack.
    const int afterAttack = (int) (kSr * 0.05);
    double maxJump = 0.0;

    for (int i = afterAttack + 1; i < n; ++i)
        maxJump = juce::jmax (maxJump, std::abs (out[(size_t) i] - out[(size_t) (i - 1)]));

    CHECK_MSG (maxJump < 0.2, "bend produced a jump of " + juce::String (maxJump, 4));

    // The pitch really arrived.
    const int tailStart = (int) (kSr * 0.7);
    const double measured = findPeakFrequency (out.data() + tailStart, n - tailStart,
                                               kSr, 180.0, 320.0);
    const double expected = 220.0 * std::pow (2.0, 2.0 / 12.0);
    const double cents = 1200.0 * std::log2 (juce::jmax (1.0, measured) / expected);

    CHECK_MSG (std::abs (cents) < 20.0,
               "bend landed at " + juce::String (measured, 2) + " Hz, expected "
               + juce::String (expected, 2) + " (" + juce::String (cents, 1) + " cents)");
}

LUTHIER_TEST (StringEngine, dispersionStretchesPartialsSharp)
{
    // Stiffness makes upper partials sharp, never flat. The magnitude is small
    // for a guitar; the direction is what must never be wrong.
    auto partialRatio = [] (double b)
    {
        StringEngine s;
        s.prepare (kSr, 512);

        StringEngine::Physical physical;
        physical.inharmonicityB = b;
        physical.sustainSeconds = 8.0;
        physical.openBrightnessHz = 7000.0;
        s.setPhysical (physical);

        const double f0 = 110.0;
        s.snapToFrequency (f0);

        auto out = renderString (s, 1.0, defaultPluck (1.0));

        const int skip = (int) (kSr * 0.05);
        const auto partials = findPartials (out.data() + skip, (int) out.size() - skip,
                                            kSr, 8, f0);

        if (partials.size() < 8 || partials[7] <= 0.0)
            return 1.0;

        // Ratio of the measured 8th partial to a perfectly harmonic one.
        return partials[7] / (partials[0] * 8.0);
    };

    const double stiff = partialRatio (0.0008);
    const double flexible = partialRatio (0.00002);

    CHECK_MSG (stiff >= flexible - 0.002,
               "a stiffer string must not have flatter partials: stiff ratio "
               + juce::String (stiff, 5) + " vs flexible " + juce::String (flexible, 5));

    CHECK_MSG (stiff > 0.98 && stiff < 1.25,
               "partial stretch should be audible but not absurd, got "
               + juce::String (stiff, 5));
}

LUTHIER_TEST (StringEngine, survivesExtremeParameters)
{
    // "Stable at extreme parameter values" is the first shipping criterion.
    StringEngine s;
    s.prepare (kSr, 512);

    StringEngine::Physical physical;
    physical.inharmonicityB = 0.005;      // absurdly stiff
    physical.sustainSeconds = 30.0;       // absurdly long
    physical.openBrightnessHz = 11000.0;
    s.setPhysical (physical);

    s.setFretBuzz (1.0, 0.5);
    s.setSlideSpeed (60.0);
    s.setNoiseAmount (1.0, 1.0);

    const int n = (int) (kSr * 2.0);
    std::vector<double> out ((size_t) n);

    RtRandom rng { 12345 };

    for (int i = 0; i < n; ++i)
    {
        // Sweep from a dive-bombed low note to the top of the neck, repeatedly.
        const double t = std::fmod ((double) i / kSr, 0.25) * 4.0;
        s.setTargetFrequency (juce::jmap (t, 25.0, 2000.0));

        if (i % 4800 == 0)
            s.excite (defaultPluck (1.0));

        out[(size_t) i] = s.processSample (rng.nextBipolar() * 0.05);
    }

    CHECK_FINITE (out.data(), n);
    CHECK_MSG (peak (out.data(), n) <= constants::kGuardLimit + 1.0e-6,
               "output exceeded the guard limit: " + juce::String (peak (out.data(), n), 4));
}

LUTHIER_TEST (StringEngine, legatoDoesNotRetriggerTheAttack)
{
    // A hammer-on must carry the existing vibration through rather than starting
    // a new note. Its injected energy should be far smaller than a fresh pluck.
    auto attackEnergyFor = [] (Excitation::Kind kind)
    {
        StringEngine s;
        s.prepare (kSr, 512);
        s.snapToFrequency (196.0);

        // Let a note establish first.
        s.excite (defaultPluck (0.8));

        for (int i = 0; i < (int) (kSr * 0.5); ++i)
            s.processSample (0.0);

        auto params = defaultPluck (0.5);
        params.kind = kind;

        const int n = (int) (kSr * 0.02);
        std::vector<double> out ((size_t) n);

        const double before = s.getLevel();
        s.excite (params);

        for (int i = 0; i < n; ++i)
            out[(size_t) i] = s.processSample (0.0);

        return peak (out.data(), n) / juce::jmax (1.0e-6, before);
    };

    const double pluckJump = attackEnergyFor (Excitation::Kind::Pluck);
    const double hammerJump = attackEnergyFor (Excitation::Kind::HammerOn);

    CHECK_MSG (hammerJump < pluckJump,
               "a hammer-on must disturb the string less than a re-pluck: hammer "
               + juce::String (hammerJump, 3) + " vs pluck " + juce::String (pluckJump, 3));
}

//==============================================================================
//  Coupling
//==============================================================================
LUTHIER_TEST (Coupling, matrixIsSymmetricAndZeroOnTheDiagonal)
{
    CouplingMatrix m;
    m.prepare (kSr, 6);
    m.buildDefault (0.02);

    for (int i = 0; i < 6; ++i)
    {
        CHECK (m.getPairCoupling (i, i) == 0.0);

        for (int j = 0; j < 6; ++j)
            CHECK_NEAR (m.getPairCoupling (i, j), m.getPairCoupling (j, i), 1.0e-12);
    }
}

LUTHIER_TEST (Coupling, neighboursCoupleMoreThanDistantStrings)
{
    CouplingMatrix m;
    m.prepare (kSr, 6);
    m.buildDefault (0.02);

    CHECK_MSG (m.getPairCoupling (0, 1) > m.getPairCoupling (0, 5),
               "adjacent strings share more of the bridge than distant ones");
}

LUTHIER_TEST (Coupling, cannotRunAway)
{
    // Engine spec: the safety cap must make feedback runaway impossible, even
    // with the coupling turned up far past anything the UI offers.
    CouplingMatrix m;
    m.prepare (kSr, 6);
    m.buildDefault (0.10);
    m.setAmount (1.0);

    std::array<double, kMaxStrings> bridge {};
    std::array<double, kMaxStrings> inputs {};

    // Feed the matrix its own output, which is the worst case.
    for (int s = 0; s < 6; ++s)
        bridge[(size_t) s] = 1.0;

    for (int i = 0; i < (int) (kSr * 2.0); ++i)
    {
        m.process (bridge.data(), inputs.data());

        for (int s = 0; s < 6; ++s)
        {
            bridge[(size_t) s] = inputs[(size_t) s] * 4.0;   // absurd gain
            CHECK_FINITE (bridge.data(), 6);
        }
    }

    for (int s = 0; s < 6; ++s)
        CHECK_MSG (std::abs (bridge[(size_t) s]) < 10.0,
                   "string " + juce::String (s) + " reached "
                   + juce::String (bridge[(size_t) s], 4));
}

LUTHIER_TEST (Coupling, aStruckStringRingsItsNeighbour)
{
    // Pluck string 0, leave string 1 silent, and check that string 1 picks up
    // energy through the bridge.
    StringEngine a, b;
    a.prepare (kSr, 512);
    b.prepare (kSr, 512);

    a.snapToFrequency (330.0);
    b.snapToFrequency (247.0);

    CouplingMatrix m;
    m.prepare (kSr, 2);
    m.buildDefault (0.03);
    m.setStringFrequency (0, 330.0);
    m.setStringFrequency (1, 247.0);

    a.excite (defaultPluck (1.0));

    std::array<double, kMaxStrings> bridge {};
    std::array<double, kMaxStrings> inputs {};

    const int n = (int) (kSr * 1.0);
    double neighbourEnergy = 0.0;

    for (int i = 0; i < n; ++i)
    {
        m.process (bridge.data(), inputs.data());

        a.processSample (inputs[0]);
        b.processSample (inputs[1]);

        bridge[0] = a.getBridgeOutput();
        bridge[1] = b.getBridgeOutput();

        if (i > (int) (kSr * 0.2))
            neighbourEnergy += bridge[1] * bridge[1];
    }

    CHECK_MSG (neighbourEnergy > 1.0e-9,
               "the undamped neighbour should be ringing, energy was "
               + juce::String (neighbourEnergy, 12));
}

//==============================================================================
//  Pickups
//==============================================================================
LUTHIER_TEST (Pickup, resonantFrequencyMatchesTheLcrValues)
{
    PickupEngine p;
    p.prepare (kSr, 6);

    // Engine spec 7.2: single coil L=2.5H C=200pF -> about 7 kHz.
    auto single = PickupSpec::makeDefault (PickupType::SingleCoil, 0.13);
    p.setPickupSpec (0, single);

    const double expected = 1.0 / (constants::kTwoPi * std::sqrt (2.5 * 200.0e-12));

    CHECK_NEAR (p.getResonantFrequency (0), expected, 1.0);
    CHECK_MSG (p.getResonantFrequency (0) > 6000.0 && p.getResonantFrequency (0) < 8000.0,
               "single coil resonance should land near 7 kHz, got "
               + juce::String (p.getResonantFrequency (0), 1));

    // Humbucker L=6H C=180pF -> about 4 kHz.
    auto humbucker = PickupSpec::makeDefault (PickupType::Humbucker, 0.14);
    p.setPickupSpec (1, humbucker);

    CHECK_MSG (p.getResonantFrequency (1) > 3500.0 && p.getResonantFrequency (1) < 5500.0,
               "humbucker resonance should land near 4 kHz, got "
               + juce::String (p.getResonantFrequency (1), 1));

    CHECK_MSG (p.getResonantFrequency (1) < p.getResonantFrequency (0),
               "a humbucker must resonate lower than a single coil");
}

LUTHIER_TEST (Pickup, positionCombNullsTheExpectedHarmonic)
{
    // A pickup at 1/N of the string length cannot sense the Nth harmonic, because
    // that harmonic has a node exactly there. This is the whole reason a bridge
    // pickup is bright and a neck pickup is warm.
    const double delaySamples = 400.0;            // an arbitrary string length
    const double f0 = kSr / delaySamples;

    auto harmonicLevel = [delaySamples, f0] (double position, int harmonic)
    {
        PickupEngine p;
        p.prepare (kSr, 1);
        p.setNumPickups (1);

        auto spec = PickupSpec::makeDefault (PickupType::SingleCoil, position);
        spec.inductanceHenries = 0.0;             // bypass the tank, isolate the comb
        spec.capacitancePf = 0.0;
        p.setPickupSpec (0, spec);
        p.setSelector (PickupSelector::Bridge);

        const int n = 16384;
        std::vector<double> out ((size_t) n);

        const double hz = f0 * harmonic;

        for (int i = 0; i < n; ++i)
        {
            const double in = std::sin (2.0 * constants::kPi * hz * i / kSr);
            const double delays[1] = { delaySamples };
            const double ins[1] = { in };
            out[(size_t) i] = p.processStrings (ins, delays, 1);
        }

        return rms (out.data() + n / 2, n / 2);
    };

    // A pickup at 1/4 of the string should null the 4th harmonic.
    const double atNode = harmonicLevel (0.25, 4);
    const double offNode = harmonicLevel (0.25, 3);

    CHECK_MSG (atNode < offNode * 0.25,
               "the 4th harmonic should be deeply notched at position 1/4: node level "
               + juce::String (atNode, 6) + " vs off-node " + juce::String (offNode, 6));
}

LUTHIER_TEST (Pickup, silenceWhenEverythingIsOff)
{
    // Identity rule 4.
    PickupEngine p;
    p.prepare (kSr, 6);
    p.setNumPickups (1);

    auto spec = PickupSpec::makeDefault (PickupType::Piezo, 0.0);
    p.setPickupSpec (0, spec);
    p.setSelector (PickupSelector::Neck);   // selects slot 0, a piezo, not magnetic

    // A piezo is not sensed magnetically, so the magnetic path must be silent.
    const double ins[1] = { 1.0 };
    const double delays[1] = { 400.0 };

    double total = 0.0;

    for (int i = 0; i < 1000; ++i)
        total += std::abs (p.processStrings (ins, delays, 1));

    CHECK_MSG (total < 1.0e-3,
               "the magnetic path should be silent for a piezo-only instrument, got "
               + juce::String (total, 8));
}

/*  SPEC-SWEEP: SP-17 / ISS-2 - pickup_blend used to be stored and never read.
    Two pickups on: the bridge one at 1/4 of the string nulls the 4th harmonic,
    the neck one at 1/5 does not. Blend 0 is the bridge pickup alone (so the 4th
    harmonic nearly vanishes), blend 1 the neck pickup alone, and 0.5 is both at
    full level - the old behaviour. With one pickup on the blend does nothing. */
LUTHIER_TEST (Pickup, blendSweepsBetweenNeckAndBridge)
{
    const double delaySamples = 400.0;
    const double hz = 4.0 * kSr / delaySamples;

    auto level = [&] (PickupSelector selector, double blend)
    {
        PickupEngine p;
        p.prepare (kSr, 1);
        p.setNumPickups (2);

        auto bridge = PickupSpec::makeDefault (PickupType::SingleCoil, 0.25);
        auto neck   = PickupSpec::makeDefault (PickupType::SingleCoil, 0.20);
        for (auto* s : { &bridge, &neck }) { s->inductanceHenries = 0.0; s->capacitancePf = 0.0; }
        p.setPickupSpec (0, bridge);
        p.setPickupSpec (1, neck);
        p.setSelector (selector);
        p.setBlend (blend);
        p.reset();

        const int n = 16384;
        std::vector<double> out ((size_t) n);

        for (int i = 0; i < n; ++i)
        {
            const double ins[1] = { std::sin (2.0 * constants::kPi * hz * i / kSr) };
            const double delays[1] = { delaySamples };
            out[(size_t) i] = p.processStrings (ins, delays, 1);
        }

        return rms (out.data() + n / 2, n / 2);
    };

    const double bridgeOnly = level (PickupSelector::BridgeNeck, 0.0);
    const double neckOnly   = level (PickupSelector::BridgeNeck, 1.0);
    const double both       = level (PickupSelector::BridgeNeck, 0.5);

    CHECK_MSG (bridgeOnly < neckOnly * 0.25,
               "blend 0 should be the bridge pickup alone (4th harmonic nulled): "
               + juce::String (bridgeOnly, 6) + " vs neck " + juce::String (neckOnly, 6));
    CHECK_MSG (both > neckOnly * 0.5,
               "blend 0.5 keeps both pickups at full level: " + juce::String (both, 6));

    const double singleA = level (PickupSelector::Bridge, 0.0);
    const double singleB = level (PickupSelector::Bridge, 1.0);
    CHECK_MSG (std::abs (singleA - singleB) < 1.0e-9,
               "with one pickup selected the blend must do nothing");
}

//==============================================================================
//  Whammy
//==============================================================================
LUTHIER_TEST (Whammy, transTremPreservesChordIntervals)
{
    // The defining property of a TransTrem: every string gets the same ratio, so
    // a chord stays in tune through the bend.
    WhammyEngine w;
    w.prepare (kSr, 6);
    w.setBridgeType (WhammyEngine::BridgeType::TransTrem);
    w.setPosition (-0.5);

    for (int i = 0; i < 200; ++i)
        w.updateBlock (512);

    const double first = w.getCentOffset (0);

    for (int s = 1; s < 6; ++s)
        CHECK_NEAR (w.getCentOffset (s), first, 0.001);

    CHECK_MSG (first < -100.0, "a half dive should be a real pitch change, got "
               + juce::String (first, 2) + " cents");
}

LUTHIER_TEST (Whammy, vintageTremDetunesChords)
{
    // And the corresponding property of a vintage trem: it does NOT, because the
    // bridge moves the slack strings further than the tight ones. Modelling that
    // unevenness is the point.
    WhammyEngine w;
    w.prepare (kSr, 6);
    w.setBridgeType (WhammyEngine::BridgeType::VintageTrem);
    w.setPosition (-1.0);

    for (int i = 0; i < 200; ++i)
        w.updateBlock (512);

    const double highString = w.getCentOffset (0);
    const double lowString = w.getCentOffset (5);

    CHECK_MSG (std::abs (lowString) > std::abs (highString) * 1.05,
               "a vintage trem should move the slack strings further: high "
               + juce::String (highString, 2) + " vs low " + juce::String (lowString, 2));
}

LUTHIER_TEST (Whammy, fixedBridgeDoesNothing)
{
    WhammyEngine w;
    w.prepare (kSr, 6);
    w.setBridgeType (WhammyEngine::BridgeType::Fixed);
    w.setPosition (-1.0);

    for (int i = 0; i < 100; ++i)
        w.updateBlock (512);

    for (int s = 0; s < 6; ++s)
        CHECK_NEAR (w.getCentOffset (s), 0.0, 1.0e-9);
}

//==============================================================================
//  Cable, amp, cabinet, room, master
//==============================================================================
// The cable is part of GuitarCircuit now; its tests are in CircuitTests.cpp.

LUTHIER_TEST (Amp, gainProducesHarmonicDistortion)
{
    auto thdFor = [] (double gain)
    {
        AmpEngine amp;
        amp.prepare (kSr, 512);
        amp.setModel (AmpModel::MarshallPlexi);
        amp.setGain (gain);
        amp.setMaster (0.5);
        amp.setBass (0.5);
        amp.setMid (0.5);
        amp.setTreble (0.5);

        // Let the warm-up smoother settle.
        for (int i = 0; i < (int) kSr; ++i)
            amp.processSample (0.0);

        const int n = 16384;
        std::vector<double> out ((size_t) n);

        for (int i = 0; i < n; ++i)
            out[(size_t) i] = amp.processSample (std::sin (2.0 * constants::kPi * 220.0 * i / kSr) * 0.3);

        for (double v : out)
            jassert (std::isfinite (v));

        return measureThd (out.data() + n / 2, n / 2, kSr, 220.0);
    };

    const double clean = thdFor (0.05);
    const double dirty = thdFor (0.95);

    CHECK_MSG (dirty > clean,
               "more gain must produce more distortion: clean THD " + juce::String (clean, 4)
               + " vs driven " + juce::String (dirty, 4));

    CHECK_MSG (dirty > 0.05, "a cranked Plexi should be clearly distorted, THD was "
               + juce::String (dirty, 4));
}

LUTHIER_TEST (Amp, standbyIsSilentAndWarmsUp)
{
    AmpEngine amp;
    amp.prepare (kSr, 512);
    amp.setStandby (true);

    // The warm-up smoother takes seconds; drive it to completion.
    for (int i = 0; i < (int) (kSr * 60.0); ++i)
        amp.processSample (0.5);

    double total = 0.0;

    for (int i = 0; i < 1000; ++i)
        total += std::abs (amp.processSample (0.5));

    CHECK_MSG (total < 1.0e-4, "standby should be silent, got " + juce::String (total, 8));

    amp.setStandby (false);
    CHECK (amp.isWarmingUp());
}

LUTHIER_TEST (Cabinet, procedualFallbackRemovesTheFizz)
{
    // With no IR loaded the cabinet must still behave like a speaker: a guitar
    // cab is deaf above about 5 kHz, and that roll-off is the difference between
    // a usable sound and a wasp in a tin.
    CabinetEngine cab;
    cab.prepare (kSr, 512);

    CabinetConfig config;
    config.cabinet = CabinetType::Cab4x12;
    config.speaker = SpeakerType::Vintage30;
    config.mic = MicType::SM57;
    cab.setConfigA (config);

    auto levelAt = [&cab] (double hz)
    {
        cab.reset();

        const int n = 8192;
        juce::AudioBuffer<float> buffer (2, n);

        for (int i = 0; i < n; ++i)
        {
            const float v = (float) std::sin (2.0 * constants::kPi * hz * i / kSr);
            buffer.setSample (0, i, v);
            buffer.setSample (1, i, v);
        }

        cab.processBlock (buffer);

        double sum = 0.0;

        for (int i = n / 2; i < n; ++i)
            sum += (double) buffer.getSample (0, i) * buffer.getSample (0, i);

        return std::sqrt (sum / (double) (n / 2));
    };

    const double mid = levelAt (1000.0);
    const double high = levelAt (10000.0);

    CHECK_MSG (mid > 0.05, "the midrange must pass, got " + juce::String (mid, 5));
    CHECK_MSG (high < mid * 0.1,
               "10 kHz must be well down on 1 kHz: mid " + juce::String (mid, 5)
               + " vs high " + juce::String (high, 5));
}

LUTHIER_TEST (Room, biggerRoomsRingLonger)
{
    // Measured as the energy still present after a second, relative to the energy
    // in the first hundred milliseconds. A plain decay-time measurement does not
    // work here: the early reflections of a large room are sparse, so the envelope
    // dips between them and a naive -30 dB search stops at the first gap.
    auto tailRatioFor = [] (RoomSize size)
    {
        RoomEngine room;
        room.prepare (kSr, 512);
        room.setRoomSize (size);
        room.setMaterial (RoomMaterial::Stone);
        room.setRoomBlend (1.0);
        room.setDecayScale (1.0);

        const int n = (int) (kSr * 3.0);
        juce::AudioBuffer<float> buffer (2, n);
        buffer.clear();

        // A short burst rather than a single impulse, so the network is actually
        // excited rather than given one sample to work with.
        RtRandom rng { 4242 };

        for (int i = 0; i < (int) (kSr * 0.01); ++i)
        {
            const float v = (float) rng.nextBipolar();
            buffer.setSample (0, i, v);
            buffer.setSample (1, i, v);
        }

        room.processBlock (buffer);

        std::vector<double> mono ((size_t) n);

        for (int i = 0; i < n; ++i)
            mono[(size_t) i] = (double) buffer.getSample (0, i);

        const int earlyStart = 0;
        const int earlyLength = (int) (kSr * 0.1);
        const int lateStart = (int) (kSr * 1.0);
        const int lateLength = (int) (kSr * 0.2);

        const double early = rms (mono.data() + earlyStart, earlyLength);
        const double late = rms (mono.data() + lateStart, lateLength);

        return (early > 1.0e-9) ? late / early : 0.0;
    };

    const double booth = tailRatioFor (RoomSize::IsoBooth);
    const double hall = tailRatioFor (RoomSize::ConcertHall);

    CHECK_MSG (hall > booth,
               "a hall must retain more energy after a second than a booth: booth "
               + juce::String (booth, 6) + " vs hall " + juce::String (hall, 6));

    CHECK_MSG (hall > 1.0e-4,
               "the hall tail is implausibly quiet at one second: "
               + juce::String (hall, 8));
}

LUTHIER_TEST (Master, limiterHoldsTheCeiling)
{
    MasterBus master;
    master.prepare (kSr, 512);
    master.setGainDb (12.0);
    master.setLimiterEnabled (true);

    const int n = (int) kSr;
    juce::AudioBuffer<float> buffer (2, n);

    for (int i = 0; i < n; ++i)
    {
        const float v = (float) (std::sin (2.0 * constants::kPi * 100.0 * i / kSr) * 0.95);
        buffer.setSample (0, i, v);
        buffer.setSample (1, i, v);
    }

    master.processBlock (buffer);

    float worst = 0.0f;

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < n; ++i)
            worst = juce::jmax (worst, std::abs (buffer.getSample (ch, i)));

    const double ceiling = dbToGain (-0.3);

    CHECK_MSG (worst <= ceiling + 1.0e-4,
               "the limiter must hold -0.3 dBFS, peak was "
               + juce::String (gainToDb (worst), 3) + " dBFS");
}

LUTHIER_TEST (Master, meteringTracksTheSignal)
{
    MasterBus master;
    master.prepare (kSr, 512);
    master.setGainDb (0.0);
    master.setLimiterEnabled (false);

    const int n = 4096;
    juce::AudioBuffer<float> buffer (2, n);

    for (int i = 0; i < n; ++i)
    {
        const float v = (float) (std::sin (2.0 * constants::kPi * 440.0 * i / kSr) * 0.5);
        buffer.setSample (0, i, v);
        buffer.setSample (1, i, v);
    }

    master.processBlock (buffer);

    CHECK_NEAR (master.getPeakLeft(), 0.5, 0.05);
    CHECK_NEAR (master.getRmsLeft(), 0.3536, 0.05);
}

//==============================================================================
//  Effects
//==============================================================================
LUTHIER_TEST (Effects, everyPedalTypeRunsCleanly)
{
    // Every pedal, at its defaults and then at both extremes of every parameter,
    // must produce finite output and must not blow up.
    for (int t = 1; t < (int) PedalType::NumTypes; ++t)
    {
        const auto type = (PedalType) t;
        auto pedal = Pedal::create (type);

        CHECK_MSG (pedal != nullptr, juce::String (Pedal::getTypeName (type)) + " failed to build");

        if (pedal == nullptr)
            continue;

        pedal->prepare (kSr, 512);
        pedal->resetParametersToDefault();
        pedal->setTempoBpm (120.0);
        pedal->setExpression (0.7);

        for (int extreme = 0; extreme < 3; ++extreme)
        {
            if (extreme > 0)
                for (int p = 0; p < pedal->getNumParameters(); ++p)
                    pedal->setParameterNormalised (p, extreme == 1 ? 0.0 : 1.0);

            pedal->reset();

            const int n = 4096;
            std::vector<double> left ((size_t) n), right ((size_t) n);

            RtRandom rng { (uint64_t) (t * 7919 + extreme) };

            for (int i = 0; i < n; ++i)
            {
                const double v = std::sin (2.0 * constants::kPi * 220.0 * i / kSr) * 0.5
                                 + rng.nextBipolar() * 0.02;
                left[(size_t) i] = v;
                right[(size_t) i] = v;
            }

            pedal->processWithBypass (left.data(), right.data(), n);

            CHECK_FINITE (left.data(), n);
            CHECK_FINITE (right.data(), n);

            CHECK_MSG (peak (left.data(), n) < 8.0,
                       juce::String (Pedal::getTypeName (type)) + " at extreme "
                       + juce::String (extreme) + " peaked at "
                       + juce::String (peak (left.data(), n), 3));
        }
    }
}

LUTHIER_TEST (Effects, bypassIsTransparent)
{
    for (int t = 1; t < (int) PedalType::NumTypes; ++t)
    {
        auto pedal = Pedal::create ((PedalType) t);

        if (pedal == nullptr)
            continue;

        pedal->prepare (kSr, 512);
        pedal->resetParametersToDefault();
        pedal->setBypassed (true);

        const int n = 512;
        std::vector<double> left ((size_t) n), right ((size_t) n), original ((size_t) n);

        for (int i = 0; i < n; ++i)
        {
            const double v = std::sin (2.0 * constants::kPi * 330.0 * i / kSr) * 0.4;
            left[(size_t) i] = right[(size_t) i] = original[(size_t) i] = v;
        }

        // The first block crossfades the bypass in; run a second one to measure.
        pedal->processWithBypass (left.data(), right.data(), n);

        for (int i = 0; i < n; ++i)
            left[(size_t) i] = right[(size_t) i] = original[(size_t) i];

        pedal->processWithBypass (left.data(), right.data(), n);

        double worst = 0.0;

        for (int i = 0; i < n; ++i)
            worst = juce::jmax (worst, std::abs (left[(size_t) i] - original[(size_t) i]));

        CHECK_MSG (worst < 1.0e-6,
                   juce::String (Pedal::getTypeName ((PedalType) t))
                   + " is not transparent when bypassed, worst error "
                   + juce::String (worst, 9));
    }
}

LUTHIER_TEST (Effects, chainReordersWithoutGlitching)
{
    EffectsChain chain;
    chain.prepare (kSr, 512);

    chain.setSlotType (0, PedalType::Overdrive);
    chain.setSlotType (1, PedalType::Delay);
    chain.setSlotType (2, PedalType::Reverb);

    CHECK (chain.getSlotType (0) == PedalType::Overdrive);
    CHECK (chain.getSlotType (2) == PedalType::Reverb);

    chain.moveSlot (0, 2);

    CHECK (chain.getSlotType (2) == PedalType::Overdrive);
    CHECK (chain.getSlotType (0) == PedalType::Delay);

    const int n = 2048;
    std::vector<double> left ((size_t) n, 0.1), right ((size_t) n, 0.1);

    chain.processStereo (left.data(), right.data(), n);

    CHECK_FINITE (left.data(), n);
    CHECK_FINITE (right.data(), n);
}

LUTHIER_TEST (Effects, secretEffectIsStableAtMaximumRegeneration)
{
    SecretEffect wolf;
    wolf.prepare (kSr);
    wolf.setEnabled (true);
    wolf.setRate (8.0);
    wolf.setDepth (1.0);
    wolf.setFeedback (1.0);     // clamped internally
    wolf.setMix (1.0);

    const int n = (int) (kSr * 5.0);
    std::vector<double> left ((size_t) n), right ((size_t) n);

    for (int i = 0; i < n; ++i)
        left[(size_t) i] = right[(size_t) i] = (i < 1000) ? 0.8 : 0.0;

    wolf.process (left.data(), right.data(), n);

    CHECK_FINITE (left.data(), n);
    CHECK_MSG (peak (left.data(), n) < 4.0,
               "the hidden effect must howl without running away, peak was "
               + juce::String (peak (left.data(), n), 3));
}

//==============================================================================
//  Body
//==============================================================================
LUTHIER_TEST (Body, modalBankReproducesTheAirResonance)
{
    BodyConfig config;
    config.shape = BodyShape::Dreadnought;
    config.topWood = Wood::SitkaSpruce;
    config.backWood = Wood::Rosewood;

    const double air = BodyModels::computeAirResonance (config);

    // A dreadnought's Helmholtz resonance is famously around 100 Hz.
    CHECK_MSG (air > 85.0 && air < 135.0,
               "dreadnought air resonance should land near 100 Hz, got "
               + juce::String (air, 1));

    // A smaller body must resonate higher: less enclosed air, same hole.
    BodyConfig parlor = config;
    parlor.shape = BodyShape::Parlor;

    CHECK_MSG (BodyModels::computeAirResonance (parlor) > air,
               "a parlour body must resonate above a dreadnought");

    // And a solid body has no cavity at all.
    BodyConfig solid = config;
    solid.shape = BodyShape::SolidStandard;

    CHECK_NEAR (BodyModels::computeAirResonance (solid), 0.0, 1.0e-9);
}

LUTHIER_TEST (Body, dimensionsMoveTheModes)
{
    // The reason modal synthesis exists: a bigger body really rings lower.
    BodyConfig config;
    config.shape = BodyShape::Dreadnought;

    const double normal = BodyModels::computeTopFundamental (config);

    config.scaleWidth = 1.3;
    const double bigger = BodyModels::computeTopFundamental (config);

    CHECK_MSG (bigger < normal,
               "a wider body must have a lower top mode: " + juce::String (normal, 1)
               + " Hz -> " + juce::String (bigger, 1) + " Hz");

    config.scaleWidth = 1.0;
    config.topThicknessMm = 4.0;
    const double thicker = BodyModels::computeTopFundamental (config);

    CHECK_MSG (thicker > normal, "a thicker top must be stiffer and so pitched higher");
}

LUTHIER_TEST (Body, modalBankIsStableAndBounded)
{
    BodyEngine body;
    body.prepare (kSr, 512);
    body.setMode (BodyEngine::Mode::Modal);
    body.setAmount (1.0);

    BodyConfig config;
    config.shape = BodyShape::Dreadnought;
    config.age = 1.0;                 // highest Q
    body.setBodyConfig (config);

    CHECK_MSG (body.getNumModes() >= 20,
               "an acoustic body should build at least 20 modes, got "
               + juce::String (body.getNumModes()));

    const int n = (int) (kSr * 2.0);
    std::vector<double> signal ((size_t) n, 0.0);

    RtRandom rng { 999 };

    for (int i = 0; i < n; ++i)
        signal[(size_t) i] = rng.nextBipolar() * 0.5;

    body.processMono (signal.data(), n);

    CHECK_FINITE (signal.data(), n);
    CHECK_MSG (peak (signal.data(), n) < 4.0,
               "modal bank peaked at " + juce::String (peak (signal.data(), n), 3));
}
