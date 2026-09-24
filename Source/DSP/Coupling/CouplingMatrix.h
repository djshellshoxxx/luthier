#pragma once

/*  Sympathetic coupling through the bridge (engine spec 5.6).

    Every string shares one piece of wood with every other string. When one string
    vibrates it moves the bridge, and the bridge moves the others; each of those
    strings responds most strongly at frequencies near its own partials. That ring
    is a large part of why a guitar sounds like a guitar, and identity rule 7 says
    it is never switchable off.

    Cost note: the spec warns against an N-to-N per-sample update. For N <= 12 the
    matrix multiply is at most 144 multiply-adds per sample, which is cheap, and
    updating per block instead injects a step into every string's delay line at
    each block boundary - an audible tick. So the matrix runs per sample and the
    saving is made elsewhere: one receive filter per string rather than per pair.
*/

#include "../Common/DspCommon.h"
#include "../../Model/Playing/TuningEngine.h"
#include <array>

namespace luthier
{

class CouplingMatrix
{
public:
    void prepare (double sampleRate, int numStrings) noexcept;
    void reset() noexcept;

    void setNumStrings (int n) noexcept;
    int  getNumStrings() const noexcept { return numStrings; }

    /** Global scaler on the whole matrix, 0 to 1. Identity rule 7 means the UI
        never offers a true zero; the engine still accepts one for testing. */
    void setAmount (double amount) noexcept { globalAmount = juce::jlimit (0.0, 1.0, amount); }
    double getAmount() const noexcept { return globalAmount; }

    /** Rebuilds the default symmetric matrix. Neighbouring strings share more
        bridge saddle and therefore couple more strongly than distant ones. */
    void buildDefault (double baseCoupling = 0.020) noexcept;

    void setPairCoupling (int i, int j, double value) noexcept;
    double getPairCoupling (int i, int j) const noexcept;

    /** Tells the matrix what each string is currently tuned to, so the receive
        filters sit on the right fundamental. Cheap enough to call per block. */
    void setStringFrequency (int stringIndex, double hz) noexcept;

    /** Reads the strings' bridge outputs and writes each string's coupling input.
        Both arrays must have room for getNumStrings() entries. */
    void process (const double* bridgeOutputs, double* couplingInputs) noexcept;

    /** Total coupling energy the last call had to scale back, for diagnostics.
        Stays at 0 unless the safety cap engaged. */
    double getLastLimiting() const noexcept { return lastLimiting; }

    /*  string-interaction.md 1: the air path. A rank-1 term: the top's sound
        pressure, driven by every string's bridge output, arrives at each
        string 0.29 ms later through 0.1 m of air, high-passed at 120 Hz, and
        goes through the same receive filter and energy cap as the bridge
        term. `coefficient` is a_air = coupling_air_amount x a_cat; 0 skips the
        whole path and the matrix is bit-identical to one without it. */
    void setAirCoefficient (double coefficient) noexcept { airCoefficient = juce::jlimit (0.0, 0.05, coefficient); }
    double getAirCoefficient() const noexcept { return airCoefficient; }

    /** The air delay in samples (0.1 m / 343 m/s at the prepared rate). */
    int getAirDelaySamples() const noexcept { return airDelay; }

    /** a_cat, per string-interaction.md 1. */
    static double airCategoryCoefficient (bool acoustic, bool semiHollow, bool bass) noexcept
    {
        return acoustic ? 0.0030 : semiHollow ? 0.0010 : bass ? 0.0004 : 0.0001;
    }

private:
    static constexpr double kEnergyCap = 0.35;

    double sr = 44100.0;
    int numStrings = 6;
    double globalAmount = 1.0;
    double lastLimiting = 0.0;

    std::array<std::array<double, kMaxStrings>, kMaxStrings> matrix {};
    std::array<double, kMaxStrings> frequencies {};

    // One resonant receive filter per string, centred on that string's fundamental.
    std::array<Biquad, kMaxStrings> receiveFilter {};
    std::array<DCBlocker, kMaxStrings> receiveDC {};

    // --- air path (string-interaction.md 1) --------------------------------------
    static constexpr int kAirRing = 64;      ///< holds 0.29 ms at 192 kHz (56 samples)
    double airCoefficient = 0.0;
    int airDelay = 14;
    int airWrite = 0;
    std::array<std::array<double, kMaxStrings>, kAirRing> airHistory {};
    std::array<double, kAirRing> airSums {};
    std::array<OnePoleHP, kMaxStrings> airHighPass {};
};

} // namespace luthier
