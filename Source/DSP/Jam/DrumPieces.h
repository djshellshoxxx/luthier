#pragma once

/*  The Jam kit's pieces (jam-mode.md 5). Physically informed, no samples.

    - MembranePiece: a drum head as a circular-membrane mode bank struck by a
      raised-cosine force pulse (felt 1.5 ms, plastic 0.6 ms). Tension
      modulation f(t) = f0 (1 + k a(t)^2) gives the pitch drop of a hard hit:
      k is up to 0.25 at velocity 127 for the kick, less for the toms. The kick
      adds two resonant-head modes, driven by the batter at 0.3.
    - SnarePiece: a membrane plus snare wires - noise gated by
      max(0, |head| - threshold), band-passed at 3.5 kHz - plus rim contact on
      accents and a brush excitation (a 120-400 ms filtered-noise sweep).
    - CymbalPiece: inharmonic mode banks for hat (32 modes, f_k = f_h k^1.35,
      seeded +-3 % jitter), ride (48, 300 Hz-14 kHz) and crash (48, with the
      "bloom": the high band rises over 30 ms). A hat closing on an open hat
      chokes it by ramping the decay over 10 ms.
    - RimPiece: three wood modes and faint batter modes; the count-in sticks.
    - ShakerPiece: PhISEM stochastic particles (64 beads) into resonances at
      3.2 and 6.5 kHz.

    Every piece: double precision, a DC blocker and a NaN guard on its output
    (engine.md 0.3), reset(), and no allocation after prepare. Pieces render
    in blocks (render) with their control-rate work (tension modulation,
    damping ramps) on a sample-accurate countdown, so the result is the same
    whatever the block sizes. process() renders one sample, for the tests.
    Randomness is RtRandom, advanced only while a piece sounds, so a render
    repeats exactly.
*/

#include "ModalResonatorBank.h"

namespace luthier
{

//==============================================================================
/** A raised-cosine contact force: the beater or stick in contact with the head. */
class ForcePulse
{
public:
    void prepare (double sampleRate) noexcept { sr = sampleRate; remaining = 0; }
    void reset() noexcept { remaining = 0; }

    void trigger (double amplitude, double seconds) noexcept
    {
        length = juce::jmax (2, (int) std::round (seconds * sr));
        remaining = length;
        amp = amplitude;
    }

    inline double next() noexcept
    {
        if (remaining <= 0)
            return 0.0;

        const double phase = (double) (length - remaining) / (double) length;
        --remaining;
        return amp * 0.5 * (1.0 - std::cos (constants::kTwoPi * phase));
    }

    bool isActive() const noexcept { return remaining > 0; }

private:
    double sr = 48000.0, amp = 0.0;
    int length = 1, remaining = 0;
};

//==============================================================================
/** Output guard shared by every piece: a 7 Hz DC blocker and a clamp to
    [-4, 4] with a non-finite sample replaced by silence. */
class PieceOutput
{
public:
    void prepare (double sampleRate) noexcept { dc.prepare (sampleRate, 7.0); }
    void reset() noexcept { dc.reset(); }

    inline double process (double x) noexcept
    {
        if (! std::isfinite (x))
        {
            dc.reset();
            return 0.0;
        }

        return juce::jlimit (-4.0, 4.0, dc.process (x));
    }

private:
    DCBlocker dc;
};

/** The size of a piece's inner scratch chunk. */
inline constexpr int kPieceChunk = 64;

//==============================================================================
class MembranePiece
{
public:
    static constexpr int kMaxModes = 8;

    struct Design
    {
        int numModes = 6;
        std::array<double, kMaxModes> ratios { { 1.0, 1.594, 2.136, 2.296, 2.653, 2.918, 3.156, 3.501 } };
        std::array<double, kMaxModes> gains  { { 1.0, 0.55, 0.42, 0.30, 0.24, 0.18, 0.14, 0.11 } };
        std::array<double, kMaxModes> decays { { 1.0, 0.70, 0.60, 0.50, 0.42, 0.36, 0.32, 0.28 } };   ///< x fundamental T60
        double f0 = 55.0;
        double t60 = 0.45;             ///< fundamental T60, s
        double pitchDropK = 0.25;      ///< k at velocity 1
        double sweepSeconds = 0.04;    ///< a(t) = exp(-t / sweep)
        double pulseSeconds = 0.0015;  ///< felt 1.5 ms, plastic 0.6 ms
        double level = 1.0;
        bool resonantHead = false;     ///< the kick's front head
        std::array<double, 2> headRatios { { 1.30, 1.87 } };   ///< between the batter's modes
        double headGain = 0.15;
    };

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** Applies a design. Real-time safe (fixed arrays); the kit calls it at a
        bar line when the kit changes. */
    void setDesign (const Design& d) noexcept;
    const Design& getDesign() const noexcept { return design; }

    /** jam_kit_tuning as a ratio (a tension change: every f0 moves, the mode
        ratios stay) and jam_kit_damping as a T60 scale (head muffling). Take
        effect at the next hit. */
    void setTuning (double ratio, double t60Scale) noexcept;

    void strike (double velocity) noexcept;

    /** Ramps an extra damping in over `seconds`, toward a `t60` decay. */
    void choke (double t60, double seconds) noexcept;

    /** Writes `n` samples. `batter`, if given, receives the batter head's own
        displacement (the snare wires read it). */
    void render (double* out, int n, double* batter = nullptr) noexcept;

    double process() noexcept { double y = 0.0; render (&y, 1); return y; }

    /** The fundamental's instantaneous frequency (jam-mode 17, JM-24). */
    double getCurrentFundamentalHz() const noexcept { return bank.getModeFrequency (0); }

    bool isActive() const noexcept { return active; }
    void housekeep() noexcept;

private:
    void updateControl() noexcept;
    void applyModes() noexcept;

    static constexpr int kControlInterval = 16;

    double sr = 48000.0;
    Design design;
    double tuningRatio = 1.0, t60Scale = 1.0;
    double hitF0 = 55.0, k = 0.0, envelope = 0.0, envelopeStep = 1.0;
    int controlCountdown = 0;

    double chokeTo = 1.0, chokeStep = 0.0, chokePosition = 1.0;
    bool choking = false;

    bool active = false;

    ForcePulse pulse;
    ModalResonatorBank bank, head;
    PieceOutput output;
};

//==============================================================================
class SnarePiece
{
public:
    enum class Stroke { normal, ghost, rimshot, brush };

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setDesign (const MembranePiece::Design& d) noexcept { batter.setDesign (d); }
    void setTuning (double ratio, double t60Scale) noexcept { batter.setTuning (ratio, t60Scale); }

    /** The wires' gate threshold, 0..1 of the head's peak (jam-mode 5). */
    void setWireThreshold (double t) noexcept { wireThreshold = juce::jlimit (0.0, 1.0, t); }
    double getWireThreshold() const noexcept { return wireThreshold; }

    void setSeed (uint64_t seed) noexcept { rng.setSeed (seed); }

    void strike (double velocity, Stroke stroke) noexcept;
    void choke (double t60, double seconds) noexcept;

    void render (double* out, int n) noexcept;
    double process() noexcept { double y = 0.0; render (&y, 1); return y; }

    bool isActive() const noexcept { return active; }
    void housekeep() noexcept;

    MembranePiece& getBatter() noexcept { return batter; }

private:
    double sr = 48000.0;
    MembranePiece batter;
    ModalResonatorBank rimBank;
    ForcePulse rimPulse;
    bool rimActive = false;

    Biquad wireBand, brushBand;
    RtRandom rng { 0x5A4E5E11ull };
    double wireThreshold = 0.06, wireLevel = 0.9, headNorm = 0.8;
    int brushLength = 1, brushRemaining = 0, sweepCountdown = 0;
    double brushAmp = 0.0;
    bool active = false;
    PieceOutput output;
};

//==============================================================================
class CymbalPiece
{
public:
    enum class Kind { hat, ride, crash };
    enum class Hit { closed, open, pedal, bow, bell, crash, close };

    struct Design
    {
        double baseHz = 330.0;       ///< hat f_h; ride / crash lowest mode
        double topHz = 14000.0;      ///< ride / crash highest mode
        double brightness = 1.0;     ///< high-mode gain scale (darker kits < 1)
        double decayScale = 1.0;
        double level = 1.0;
    };

    void prepare (double sampleRate, Kind kind) noexcept;
    void reset() noexcept;
    void setDesign (const Design& d, uint64_t seed) noexcept;

    /** CPU relief step (jam-mode 13): cymbals 48 -> 24 modes, hat 32 -> 16. */
    void setReduced (bool reduced) noexcept;

    void strike (double velocity, Hit hit) noexcept;

    /** Ramps the decay toward `t60` over `seconds`, with no new hit (the hat
        closing, a hand on the crash). */
    void choke (double seconds, double t60) noexcept;

    void render (double* out, int n) noexcept;
    double process() noexcept { double y = 0.0; render (&y, 1); return y; }

    bool isActive() const noexcept { return active; }
    void housekeep() noexcept;

    /** Mode count now live, low and high banks together (tests, CPU relief). */
    int getNumModes() const noexcept { return low.getNumModes() + high.getNumModes(); }

private:
    void layoutModes() noexcept;
    void setDecays (double scale) noexcept;
    void updateRamp() noexcept;
    double modeT60 (int index, int count) const noexcept;

    Kind kind = Kind::hat;
    double sr = 48000.0;
    Design design;
    uint64_t seed = 1;
    bool reduced = false;

    int totalModes = 32;
    std::array<double, 48> modeHz {}, modeGain {};
    std::array<bool, 48> modeIsHigh {};
    std::array<int, 48> modeSlot {};

    double currentScale = 1.0;       ///< decay multiplier currently applied
    double rampFrom = 1.0, rampTo = 1.0;
    int rampRemaining = 0, rampTotal = 1, controlCountdown = 0;
    double openT60 = 1.0;

    ForcePulse pulse;
    ModalResonatorBank low, high;
    double highDirect = 1.0;

    RtRandom rng { 0xC1A5ull };
    int bloomLength = 1, bloomRemaining = 0;
    double bloomAmp = 0.0;
    int chickLength = 1, chickRemaining = 0;
    double chickAmp = 0.0;

    bool active = false;
    PieceOutput output;
};

//==============================================================================
class RimPiece
{
public:
    enum class Hit { rim, sticks };

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void setBatterHz (double snareF0) noexcept;
    void strike (double velocity, Hit hit) noexcept;

    void render (double* out, int n) noexcept;

    bool isActive() const noexcept { return active; }
    void housekeep() noexcept;

private:
    void configure (Hit hit) noexcept;

    double sr = 48000.0, batterHz = 200.0;
    ForcePulse pulse;
    ModalResonatorBank bank;
    bool active = false;
    PieceOutput output;
};

//==============================================================================
class ShakerPiece
{
public:
    static constexpr int kBeads = 64;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void setSeed (uint64_t seed) noexcept { rng.setSeed (seed); }
    void strike (double velocity) noexcept;

    void render (double* out, int n) noexcept;

    bool isActive() const noexcept { return active; }
    void housekeep() noexcept;

private:
    double sr = 48000.0;
    RtRandom rng { 0x5AA4E2ull };
    double shakeEnergy = 0.0, soundLevel = 0.0, energyDecay = 0.999, soundDecay = 0.99;
    double collisionProbability = 0.0004;
    Biquad resonanceA, resonanceB;
    bool active = false;
    PieceOutput output;
};

} // namespace luthier
