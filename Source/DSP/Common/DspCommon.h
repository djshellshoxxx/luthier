#pragma once

/*  Luthier - shared DSP primitives.

    Engine rule 1: every recursive stage in this plugin is double-precision.
    Engine rule 3: every recursive stage gets a DC blocker and a NaN/Inf guard.
    The helpers that enforce those rules live here so no module has to reinvent them.

    Everything in this header is real-time safe: no allocation, no locks, no I/O.
*/

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>

namespace luthier
{

/** Hard ceiling on string count: 12-string mode is the widest the engine supports. */
inline constexpr int kMaxStrings = 12;

//==============================================================================
namespace constants
{
    /** Hard ceiling applied by every NaN guard. Engine spec section 0.3. */
    inline constexpr double kGuardLimit = 4.0;

    /** Lowest fundamental the string delay lines must accommodate (low B, dive-bombed). */
    inline constexpr double kMinStringHz = 18.0;

    /** Standard smoothing time for continuous parameters (engine rule 4). */
    inline constexpr double kParamSmoothSeconds = 0.020;

    /** Filter cutoffs smooth a little slower to keep coefficient recalcs cheap. */
    inline constexpr double kCutoffSmoothSeconds = 0.030;

    /** Discrete switches (pickup selection, bypass) crossfade over this long. */
    inline constexpr double kSwitchCrossfadeSeconds = 0.005;

    inline constexpr double kTwoPi = 6.283185307179586476925286766559;
    inline constexpr double kPi    = 3.141592653589793238462643383279;

    /** Anything below this magnitude is flushed to zero in feedback loops. */
    inline constexpr double kDenormalFloor = 1.0e-18;
}

//==============================================================================
/** Clamps non-finite values to zero and finite values to the guard limit.
    Applied to the output of every recursive stage. */
inline double sanitise (double x) noexcept
{
    if (! std::isfinite (x))
        return 0.0;

    return juce::jlimit (-constants::kGuardLimit, constants::kGuardLimit, x);
}

/** Flushes values small enough to cost CPU on denormal-unfriendly paths.
    ScopedNoDenormals handles this in hardware, but waveguide feedback loops run
    long enough that an explicit flush is cheap insurance. */
inline double flushDenormal (double x) noexcept
{
    return (std::abs (x) < constants::kDenormalFloor) ? 0.0 : x;
}

inline double dbToGain (double db) noexcept        { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g) noexcept         { return g > 1.0e-12 ? 20.0 * std::log10 (g) : -240.0; }
inline double centsToRatio (double cents) noexcept { return std::pow (2.0, cents / 1200.0); }
inline double semitonesToRatio (double st) noexcept{ return std::pow (2.0, st / 12.0); }
inline double ratioToCents (double r) noexcept     { return 1200.0 * std::log2 (std::max (1.0e-12, r)); }

/** MIDI note number to Hz. */
inline double midiToHz (double note, double a4 = 440.0) noexcept
{
    return a4 * std::pow (2.0, (note - 69.0) / 12.0);
}

inline double hzToMidi (double hz, double a4 = 440.0) noexcept
{
    return 69.0 + 12.0 * std::log2 (std::max (1.0e-9, hz) / a4);
}

//==============================================================================
/** Removes DC from a recursive stage's output. Default corner is 7 Hz. */
class DCBlocker
{
public:
    void prepare (double sampleRate, double cutoffHz = 7.0) noexcept
    {
        r = 1.0 - (constants::kTwoPi * cutoffHz / sampleRate);
        r = juce::jlimit (0.9, 0.99999, r);
        reset();
    }

    void reset() noexcept { x1 = 0.0; y1 = 0.0; }

    inline double process (double x) noexcept
    {
        const double y = x - x1 + r * y1;
        x1 = x;
        y1 = flushDenormal (y);
        return y;
    }

private:
    double r = 0.999, x1 = 0.0, y1 = 0.0;
};

//==============================================================================
/** One-pole lowpass: loop filters, damping, cable sim, tone controls. */
class OnePoleLP
{
public:
    void prepare (double sampleRate) noexcept { sr = sampleRate; reset(); }
    void reset() noexcept { z = 0.0; }

    /** cpu-quality-modes 2.2: a new rate that keeps the filter's state. */
    void setSampleRateKeepingState (double sampleRate) noexcept { sr = sampleRate; }

    void setCutoff (double hz) noexcept
    {
        hz = juce::jlimit (1.0, sr * 0.49, hz);
        a = std::exp (-constants::kTwoPi * hz / sr);
        b = 1.0 - a;
    }

    /** Sets the pole directly, for the string loop filter where the coefficient
        (not the cutoff) is the physically meaningful quantity. */
    void setPole (double pole) noexcept
    {
        a = juce::jlimit (0.0, 0.9999, pole);
        b = 1.0 - a;
    }

    inline double process (double x) noexcept
    {
        z = flushDenormal (b * x + a * z);
        return z;
    }

    inline double current() const noexcept { return z; }

private:
    double sr = 44100.0, a = 0.0, b = 1.0, z = 0.0;
};

//==============================================================================
/** One-pole highpass, built as input minus its lowpass. */
class OnePoleHP
{
public:
    void prepare (double sampleRate) noexcept { lp.prepare (sampleRate); }
    void reset() noexcept { lp.reset(); }
    void setSampleRateKeepingState (double sampleRate) noexcept { lp.setSampleRateKeepingState (sampleRate); }
    void setCutoff (double hz) noexcept { lp.setCutoff (hz); }
    inline double process (double x) noexcept { return x - lp.process (x); }

private:
    OnePoleLP lp;
};

//==============================================================================
/** Direct-form-II transposed biquad in double precision, with RBJ designers. */
class Biquad
{
public:
    void reset() noexcept { z1 = 0.0; z2 = 0.0; }

    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = flushDenormal (b1 * x - a1 * y + z2);
        z2 = flushDenormal (b2 * x - a2 * y);
        return y;
    }

    void setCoefficients (double nb0, double nb1, double nb2, double na1, double na2) noexcept
    {
        b0 = nb0; b1 = nb1; b2 = nb2; a1 = na1; a2 = na2;
    }

    void setBypass() noexcept { setCoefficients (1.0, 0.0, 0.0, 0.0, 0.0); }

    void setLowpass (double sr, double freq, double q) noexcept
    {
        const double w0 = clampW0 (sr, freq), cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * std::max (0.05, q));
        const double a0 = 1.0 + alpha;
        setCoefficients ((1.0 - cw) * 0.5 / a0, (1.0 - cw) / a0, (1.0 - cw) * 0.5 / a0,
                         -2.0 * cw / a0, (1.0 - alpha) / a0);
    }

    void setHighpass (double sr, double freq, double q) noexcept
    {
        const double w0 = clampW0 (sr, freq), cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * std::max (0.05, q));
        const double a0 = 1.0 + alpha;
        setCoefficients ((1.0 + cw) * 0.5 / a0, -(1.0 + cw) / a0, (1.0 + cw) * 0.5 / a0,
                         -2.0 * cw / a0, (1.0 - alpha) / a0);
    }

    void setBandpass (double sr, double freq, double q) noexcept
    {
        const double w0 = clampW0 (sr, freq), cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * std::max (0.05, q));
        const double a0 = 1.0 + alpha;
        setCoefficients (alpha / a0, 0.0, -alpha / a0, -2.0 * cw / a0, (1.0 - alpha) / a0);
    }

    void setNotch (double sr, double freq, double q) noexcept
    {
        const double w0 = clampW0 (sr, freq), cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * std::max (0.05, q));
        const double a0 = 1.0 + alpha;
        setCoefficients (1.0 / a0, -2.0 * cw / a0, 1.0 / a0, -2.0 * cw / a0, (1.0 - alpha) / a0);
    }

    void setAllpass (double sr, double freq, double q) noexcept
    {
        const double w0 = clampW0 (sr, freq), cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * std::max (0.05, q));
        const double a0 = 1.0 + alpha;
        setCoefficients ((1.0 - alpha) / a0, -2.0 * cw / a0, 1.0, -2.0 * cw / a0, (1.0 - alpha) / a0);
    }

    void setPeaking (double sr, double freq, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = clampW0 (sr, freq), cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * std::max (0.05, q));
        const double a0 = 1.0 + alpha / A;
        setCoefficients ((1.0 + alpha * A) / a0, -2.0 * cw / a0, (1.0 - alpha * A) / a0,
                         -2.0 * cw / a0, (1.0 - alpha / A) / a0);
    }

    void setLowShelf (double sr, double freq, double slope, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = clampW0 (sr, freq), cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw * 0.5 * std::sqrt ((A + 1.0 / A) * (1.0 / std::max (0.05, slope) - 1.0) + 2.0);
        const double tsa = 2.0 * std::sqrt (A) * alpha;
        const double a0 = (A + 1.0) + (A - 1.0) * cw + tsa;
        setCoefficients (A * ((A + 1.0) - (A - 1.0) * cw + tsa) / a0,
                         2.0 * A * ((A - 1.0) - (A + 1.0) * cw) / a0,
                         A * ((A + 1.0) - (A - 1.0) * cw - tsa) / a0,
                         -2.0 * ((A - 1.0) + (A + 1.0) * cw) / a0,
                         ((A + 1.0) + (A - 1.0) * cw - tsa) / a0);
    }

    void setHighShelf (double sr, double freq, double slope, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = clampW0 (sr, freq), cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw * 0.5 * std::sqrt ((A + 1.0 / A) * (1.0 / std::max (0.05, slope) - 1.0) + 2.0);
        const double tsa = 2.0 * std::sqrt (A) * alpha;
        const double a0 = (A + 1.0) - (A - 1.0) * cw + tsa;
        setCoefficients (A * ((A + 1.0) + (A - 1.0) * cw + tsa) / a0,
                         -2.0 * A * ((A - 1.0) + (A + 1.0) * cw) / a0,
                         A * ((A + 1.0) + (A - 1.0) * cw - tsa) / a0,
                         2.0 * ((A - 1.0) - (A + 1.0) * cw) / a0,
                         ((A + 1.0) - (A - 1.0) * cw - tsa) / a0);
    }

private:
    static double clampW0 (double sr, double freq) noexcept
    {
        return constants::kTwoPi * juce::jlimit (5.0, sr * 0.495, freq) / sr;
    }

    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;
};

//==============================================================================
/** First-order allpass. Flat magnitude, frequency-dependent delay.

    Used as the dispersion element in the string loop. A NEGATIVE coefficient
    delays low frequencies more than high ones, which pushes the upper partials
    sharp - that is exactly the stiffness-driven inharmonicity of a real wound
    string. A positive coefficient would compress the partials instead, so the
    sign matters and the string engine always passes a negative value. */
class Allpass1
{
public:
    void reset() noexcept { x1 = 0.0; y1 = 0.0; }

    void setCoefficient (double c) noexcept { a = juce::jlimit (-0.97, 0.97, c); }
    double getCoefficient() const noexcept { return a; }

    inline double process (double x) noexcept
    {
        const double y = a * x + x1 - a * y1;
        x1 = x;
        y1 = flushDenormal (y);
        return y;
    }

    /** Group delay in samples at DC. */
    double delayAtDC() const noexcept { return (1.0 - a) / (1.0 + a); }

    /** Group delay in samples at Nyquist. */
    double delayAtNyquist() const noexcept { return (1.0 + a) / (1.0 - a); }

private:
    double a = 0.0, x1 = 0.0, y1 = 0.0;
};

//==============================================================================
/** Exponential parameter smoother. Time constant in seconds. */
class ExpSmoother
{
public:
    void prepare (double sampleRate, double timeSeconds) noexcept
    {
        sr = sampleRate;
        setTime (timeSeconds);
    }

    void setTime (double timeSeconds) noexcept
    {
        coeff = (timeSeconds <= 0.0) ? 0.0
                                     : std::exp (-1.0 / (std::max (1.0e-6, timeSeconds) * sr));
    }

    void setTarget (double t) noexcept { target = t; }
    void snapTo (double v) noexcept    { target = v; value = v; }

    /** Jumps to wherever the target already is. Called from reset(), so that
        resetting leaves no ramp in flight - two renders of the same state then
        produce the same audio, which offline rendering depends on. */
    void snapToTarget() noexcept       { value = target; }
    double getTarget() const noexcept  { return target; }
    double getCurrent() const noexcept { return value; }

    inline double next() noexcept
    {
        value = target + (value - target) * coeff;
        if (std::abs (value - target) < 1.0e-9)
            value = target;

        return value;
    }

    bool isSmoothing() const noexcept { return value != target; }

private:
    double sr = 44100.0, coeff = 0.0, value = 0.0, target = 0.0;
};

//==============================================================================
/** Linear ramp smoother, for parameters where a constant slew rate matters. */
class LinSmoother
{
public:
    void prepare (double sampleRate, double timeSeconds) noexcept
    {
        rampSamples = juce::jmax (1, (int) (sampleRate * timeSeconds));
        countdown = 0;
    }

    void setTarget (double t) noexcept
    {
        if (t == target)
            return;

        target = t;
        countdown = rampSamples;
        step = (target - value) / (double) rampSamples;
    }

    void snapTo (double v) noexcept     { value = target = v; countdown = 0; step = 0.0; }
    void snapToTarget() noexcept        { value = target; countdown = 0; step = 0.0; }
    double getCurrent() const noexcept  { return value; }
    double getTarget() const noexcept   { return target; }

    inline double next() noexcept
    {
        if (countdown > 0)
        {
            value += step;
            if (--countdown == 0)
                value = target;
        }
        return value;
    }

    bool isSmoothing() const noexcept { return countdown > 0; }

private:
    double value = 0.0, target = 0.0, step = 0.0;
    int rampSamples = 64, countdown = 0;
};

//==============================================================================
/** Real-time-safe xorshift128+ PRNG, for humanisation, noise excitation and
    string noise. Never allocates, never locks, deterministic from a seed. */
class RtRandom
{
public:
    explicit RtRandom (uint64_t seed = 0x9E3779B97F4A7C15ull) noexcept { setSeed (seed); }

    void setSeed (uint64_t seed) noexcept
    {
        s0 = seed ? seed : 0x9E3779B97F4A7C15ull;
        s1 = s0 ^ 0xBF58476D1CE4E5B9ull;

        for (int i = 0; i < 16; ++i)
            nextULong();
    }

    inline uint64_t nextULong() noexcept
    {
        uint64_t x = s0;
        const uint64_t y = s1;
        s0 = y;
        x ^= x << 23;
        s1 = x ^ y ^ (x >> 17) ^ (y >> 26);
        return s1 + y;
    }

    /** Uniform in [0, 1). */
    inline double nextDouble() noexcept
    {
        return (double) (nextULong() >> 11) * (1.0 / 9007199254740992.0);
    }

    /** Uniform in [-1, 1). */
    inline double nextBipolar() noexcept { return nextDouble() * 2.0 - 1.0; }

    /** Approximately Gaussian (sum of four uniforms), unit variance, cheap. */
    inline double nextGaussian() noexcept
    {
        return (nextDouble() + nextDouble() + nextDouble() + nextDouble() - 2.0) * 1.2247448713915890;
    }

    inline int nextInt (int maxExclusive) noexcept
    {
        return maxExclusive <= 0 ? 0 : (int) (nextULong() % (uint64_t) maxExclusive);
    }

    inline bool nextBool (double probability) noexcept { return nextDouble() < probability; }

private:
    uint64_t s0 = 1, s1 = 2;
};

//==============================================================================
/** Envelope follower with separate attack/release, used by the compressor,
    envelope filter, feedback simulator and amp sag. */
class EnvelopeFollower
{
public:
    void prepare (double sampleRate) noexcept { sr = sampleRate; reset(); }
    void reset() noexcept { env = 0.0; }

    /** cpu-quality-modes 2.2: a new rate that keeps the envelope. Call setTimes after. */
    void setSampleRateKeepingState (double sampleRate) noexcept { sr = sampleRate; }

    void setTimes (double attackSeconds, double releaseSeconds) noexcept
    {
        atk = attackSeconds  <= 0.0 ? 0.0 : std::exp (-1.0 / (attackSeconds  * sr));
        rel = releaseSeconds <= 0.0 ? 0.0 : std::exp (-1.0 / (releaseSeconds * sr));
    }

    inline double process (double x) noexcept
    {
        const double r = std::abs (x);
        const double c = (r > env) ? atk : rel;
        env = flushDenormal (r + (env - r) * c);
        return env;
    }

    double current() const noexcept { return env; }

private:
    double sr = 44100.0, atk = 0.0, rel = 0.0, env = 0.0;
};

//==============================================================================
/** Multi-shape LFO used by modulation effects, vibrato and tremolo. */
class Lfo
{
public:
    enum class Shape { Sine, Triangle, Square, Saw, RandomSmooth, FingerVibrato };

    void prepare (double sampleRate) noexcept { sr = sampleRate; reset(); }

    void reset() noexcept
    {
        phase = 0.0;
        randCurrent = 0.0;
        rng.setSeed (kSeed);   // so a reset LFO repeats its random walk
        randTarget = rng.nextBipolar();
    }

    void setRate (double hz) noexcept { inc = juce::jlimit (0.0, 0.49, hz / sr); }
    void setShape (Shape s) noexcept  { shape = s; }
    void setPhase (double p) noexcept { phase = p - std::floor (p); }
    double getPhase() const noexcept  { return phase; }

    /** Returns a value in [-1, 1]. */
    inline double next() noexcept
    {
        double out = 0.0;

        switch (shape)
        {
            case Shape::Sine:     out = std::sin (constants::kTwoPi * phase); break;
            case Shape::Triangle: out = 4.0 * std::abs (phase - 0.5) - 1.0;   break;
            case Shape::Square:   out = phase < 0.5 ? 1.0 : -1.0;             break;
            case Shape::Saw:      out = 2.0 * phase - 1.0;                    break;

            case Shape::RandomSmooth:
                randCurrent += (randTarget - randCurrent) * 0.002;
                out = randCurrent;
                break;

            case Shape::FingerVibrato:
                // A real finger pulls the string sharp faster than it lets it
                // return, so the up-slope is steeper than the down-slope.
                out = (phase < 0.35) ? std::sin (constants::kPi * (phase / 0.35) * 0.5)
                                     : std::cos (constants::kPi * ((phase - 0.35) / 0.65) * 0.5);
                out = out * 2.0 - 1.0;
                break;
        }

        phase += inc;
        if (phase >= 1.0)
        {
            phase -= 1.0;
            randTarget = rng.nextBipolar();
        }

        return out;
    }

private:
    double sr = 44100.0, phase = 0.0, inc = 0.0;
    double randCurrent = 0.0, randTarget = 0.0;
    Shape shape = Shape::Sine;
    static constexpr uint64_t kSeed = 0xC0FFEE1234ull;
    RtRandom rng { kSeed };
};

//==============================================================================
/** Equal-power crossfade helper for the 5 ms discrete-switch rule. */
class SwitchCrossfade
{
public:
    void prepare (double sampleRate, double seconds = constants::kSwitchCrossfadeSeconds) noexcept
    {
        total = juce::jmax (1, (int) (sampleRate * seconds));
        remaining = 0;
    }

    void trigger() noexcept        { remaining = total; }
    bool isActive() const noexcept { return remaining > 0; }

    /** Returns the mix weight for the NEW signal (0 to 1 across the fade). */
    inline double next() noexcept
    {
        if (remaining <= 0)
            return 1.0;

        const double t = 1.0 - (double) remaining / (double) total;
        --remaining;
        return std::sin (t * constants::kPi * 0.5);
    }

private:
    int total = 64, remaining = 0;
};

//==============================================================================
/** Topology-preserving-transform state-variable filter (Zavalishin / Simper),
    in double precision (mic-placement.md 0.4, 5).

    Unlike the direct-form `Biquad`, its state is the integrators' charge, so
    its coefficients can be changed every few samples under modulation without
    zipper noise or a transient blow-up. The output is a mix of the input and
    the band and low outputs, which covers the bell, the shelves and the
    low-pass one class needs. A bell or shelf at 0 dB has mix (1, 0, 0) and so
    passes its input through exactly. */
class TptSvf
{
public:
    void reset() noexcept { ic1 = 0.0; ic2 = 0.0; }

    void setBell (double sr, double hz, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        design (std::tan (constants::kPi * clampHz (sr, hz) / sr), 1.0 / (juce::jmax (0.05, q) * A));
        m0 = 1.0; m1 = k * (A * A - 1.0); m2 = 0.0;
    }

    void setHighShelf (double sr, double hz, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        design (std::tan (constants::kPi * clampHz (sr, hz) / sr) * std::sqrt (A), 1.0 / juce::jmax (0.05, q));
        m0 = A * A; m1 = k * (1.0 - A) * A; m2 = 1.0 - A * A;
    }

    void setLowShelf (double sr, double hz, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        design (std::tan (constants::kPi * clampHz (sr, hz) / sr) / std::sqrt (A), 1.0 / juce::jmax (0.05, q));
        m0 = 1.0; m1 = k * (A - 1.0); m2 = A * A - 1.0;
    }

    void setLowpass (double sr, double hz, double q) noexcept
    {
        design (std::tan (constants::kPi * clampHz (sr, hz) / sr), 1.0 / juce::jmax (0.05, q));
        m0 = 0.0; m1 = 0.0; m2 = 1.0;
    }

    void setHighpass (double sr, double hz, double q) noexcept
    {
        design (std::tan (constants::kPi * clampHz (sr, hz) / sr), 1.0 / juce::jmax (0.05, q));
        m0 = 1.0; m1 = -k; m2 = -1.0;
    }

    /** True when the filter is an exact pass-through. */
    bool isIdentity() const noexcept { return m0 == 1.0 && m1 == 0.0 && m2 == 0.0 && rampLeft == 0; }

    /** Glides this filter's coefficients to `target`'s over `samples`, one
        step per processed sample, landing exactly on them. Control-rate
        updates then change the sound continuously rather than in 32-sample
        steps (mic-placement.md 0.4). */
    void rampTo (const TptSvf& target, int samples) noexcept
    {
        if (target.a1 == a1 && target.a2 == a2 && target.a3 == a3
            && target.m0 == m0 && target.m1 == m1 && target.m2 == m2)
        {
            g = target.g; k = target.k;
            rampLeft = 0;
            return;
        }

        // The recurrence's own coefficients glide linearly (no division per
        // sample); g and k, which only the response readout uses, jump.
        g = target.g; k = target.k;
        endA1 = target.a1; endA2 = target.a2; endA3 = target.a3;
        endM0 = target.m0; endM1 = target.m1; endM2 = target.m2;
        rampLeft = juce::jmax (1, samples);
        const double inv = 1.0 / rampLeft;
        dA1 = (endA1 - a1) * inv; dA2 = (endA2 - a2) * inv; dA3 = (endA3 - a3) * inv;
        dM0 = (endM0 - m0) * inv; dM1 = (endM1 - m1) * inv; dM2 = (endM2 - m2) * inv;
    }

    /** Takes `other`'s coefficients at once, keeping this filter's state. */
    void copyCoefficientsFrom (const TptSvf& other) noexcept
    {
        g = other.g; k = other.k; a1 = other.a1; a2 = other.a2; a3 = other.a3;
        m0 = other.m0; m1 = other.m1; m2 = other.m2;
        rampLeft = 0;
    }

    inline double process (double x) noexcept
    {
        if (rampLeft > 0)
            stepRamp();

        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = flushDenormal (2.0 * v1 - ic1);
        ic2 = flushDenormal (2.0 * v2 - ic2);

        // A non-finite state (a NaN input) would otherwise latch for ever.
        if (! std::isfinite (ic1) || ! std::isfinite (ic2))
        {
            reset();
            return 0.0;
        }

        return m0 * x + m1 * v1 + m2 * v2;
    }

    /** Runs a run of samples in place. Coefficients glide per sample while a
        ramp is in flight; otherwise the loop is branch-free and the state is
        checked (non-finite -> reset) and denormal-flushed once at the end,
        which is what makes seven of these cheap enough per mic. */
    void processBlock (double* x, int n) noexcept
    {
        if (rampLeft > 0)
        {
            for (int i = 0; i < n; ++i)
                x[i] = process (x[i]);

            return;
        }

        double s1 = ic1, s2 = ic2;
        const double c1 = a1, c2 = a2, c3 = a3, o0 = m0, o1 = m1, o2 = m2;

        for (int i = 0; i < n; ++i)
        {
            const double in = x[i];
            const double v3 = in - s2;
            const double v1 = c1 * s1 + c2 * v3;
            const double v2 = s2 + c2 * s1 + c3 * v3;
            s1 = 2.0 * v1 - s1;
            s2 = 2.0 * v2 - s2;
            x[i] = o0 * in + o1 * v1 + o2 * v2;
        }

        if (! std::isfinite (s1) || ! std::isfinite (s2))
        {
            reset();

            for (int i = 0; i < n; ++i)
                x[i] = 0.0;

            return;
        }

        ic1 = flushDenormal (s1);
        ic2 = flushDenormal (s2);
    }

    /** Runs a series cascade of filters over a run of samples, interleaved
        per sample. Each filter's recurrence is latency-bound on its own
        state; interleaving lets the CPU overlap the independent chains, where
        filter-after-filter over a run leaves them in series. Falls back to one
        filter at a time while any coefficient ramp is in flight. */
    static void processCascade (TptSvf* const* filters, int count, double* x, int n) noexcept
    {
        constexpr int maxFilters = 8;
        count = juce::jmin (count, maxFilters);

        bool ramping = false;

        for (int f = 0; f < count; ++f)
            ramping = ramping || filters[f]->rampLeft > 0;

        if (ramping)
        {
            // Interleaved still, stepping each filter's coefficient ramp.
            for (int i = 0; i < n; ++i)
            {
                double v = x[i];

                for (int f = 0; f < count; ++f)
                    v = filters[f]->process (v);

                x[i] = v;
            }

            return;
        }

        double s1[maxFilters], s2[maxFilters], c1[maxFilters], c2[maxFilters], c3[maxFilters];
        double o0[maxFilters], o1[maxFilters], o2[maxFilters];

        for (int f = 0; f < count; ++f)
        {
            const auto& t = *filters[f];
            s1[f] = t.ic1; s2[f] = t.ic2; c1[f] = t.a1; c2[f] = t.a2; c3[f] = t.a3;
            o0[f] = t.m0; o1[f] = t.m1; o2[f] = t.m2;
        }

        for (int i = 0; i < n; ++i)
        {
            double v = x[i];

            for (int f = 0; f < count; ++f)
            {
                const double v3 = v - s2[f];
                const double v1 = c1[f] * s1[f] + c2[f] * v3;
                const double v2 = s2[f] + c2[f] * s1[f] + c3[f] * v3;
                s1[f] = 2.0 * v1 - s1[f];
                s2[f] = 2.0 * v2 - s2[f];
                v = o0[f] * v + o1[f] * v1 + o2[f] * v2;
            }

            x[i] = v;
        }

        bool finite = true;

        for (int f = 0; f < count; ++f)
        {
            finite = finite && std::isfinite (s1[f]) && std::isfinite (s2[f]);
            filters[f]->ic1 = flushDenormal (s1[f]);
            filters[f]->ic2 = flushDenormal (s2[f]);
        }

        if (! finite)
        {
            for (int f = 0; f < count; ++f)
                filters[f]->reset();

            for (int i = 0; i < n; ++i)
                x[i] = 0.0;
        }
    }

    /** The analogue prototype's response at `hz` for a bell / shelf designed
        at `fc` - no tan, for control-rate work that must be rate-free. */
    static std::complex<double> analogBell (double hz, double fc, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double k = 1.0 / (juce::jmax (0.05, q) * A);
        const std::complex<double> s (0.0, hz / fc);
        return 1.0 + k * (A * A - 1.0) * s / (s * s + k * s + 1.0);
    }

    static std::complex<double> analogHighShelf (double hz, double fc, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double k = 1.0 / juce::jmax (0.05, q);
        const std::complex<double> s (0.0, hz / (fc * std::sqrt (A)));
        const auto den = s * s + k * s + 1.0;
        return A * A + k * (1.0 - A) * A * s / den + (1.0 - A * A) / den;
    }

    /** The exact digital response at `hz`, for plots and tests. A TPT SVF is
        the bilinear transform of its analogue prototype, so the response is the
        prototype's at the prewarped normalised frequency. */
    std::complex<double> response (double sr, double hz) const noexcept
    {
        const double w = std::tan (constants::kPi * juce::jlimit (0.0, sr * 0.4999, hz) / sr) / g;
        const std::complex<double> s (0.0, w);
        const auto den = s * s + k * s + 1.0;
        return m0 + m1 * (s / den) + m2 * (1.0 / den);
    }

private:
    static double clampHz (double sr, double hz) noexcept { return juce::jlimit (1.0, sr * 0.49, hz); }

    void design (double gIn, double kIn) noexcept
    {
        g = gIn;
        k = kIn;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    double g = 0.1, k = 1.4, a1 = 1.0, a2 = 0.0, a3 = 0.0;
    double m0 = 1.0, m1 = 0.0, m2 = 0.0;
    double ic1 = 0.0, ic2 = 0.0;

    inline void stepRamp() noexcept
    {
        if (--rampLeft == 0)
        {
            a1 = endA1; a2 = endA2; a3 = endA3; m0 = endM0; m1 = endM1; m2 = endM2;
        }
        else
        {
            a1 += dA1; a2 += dA2; a3 += dA3; m0 += dM0; m1 += dM1; m2 += dM2;
        }
    }

    int rampLeft = 0;
    double endA1 = 1.0, endA2 = 0.0, endA3 = 0.0, endM0 = 1.0, endM1 = 0.0, endM2 = 0.0;
    double dA1 = 0.0, dA2 = 0.0, dA3 = 0.0, dM0 = 0.0, dM1 = 0.0, dM2 = 0.0;
};

//==============================================================================
/** Soft saturator: a musical ceiling rather than a hard clip. */
inline double softClip (double x) noexcept
{
    if (x >  3.0) return  1.0;
    if (x < -3.0) return -1.0;
    return x * (27.0 + x * x) / (27.0 + 9.0 * x * x);
}

/** Asymmetric tube-style transfer curve (engine spec 11.2). */
inline double tubeShape (double x, double bias) noexcept
{
    return 1.5 * std::tanh (x) - 0.5 * std::tanh (x - bias);
}

} // namespace luthier
