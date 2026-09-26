#pragma once

/*  String aging (string-aging.md).

    A continuous, per-string model of how a set of strings dies, built from four
    real mechanisms - contamination, corrosion, core fatigue and coating - that
    all end up as three multipliers on things StringEngine already has (loop
    brightness, loss / T60, dispersion) plus a tuning offset. Nothing here adds
    a filter (INDEX.md: physical deltas only).

    With `detail` at 0 the model is the old three-row `kAgeEffects` table,
    interpolated through its rows at 0, 12 and 120 hours, so a preset saved
    with the `string_age` choice sounds exactly as it did (section 3.3).

    Threading: the inputs are set and advance() runs on the audio thread, once
    per block. The per-string state (restring points and accrued hours) is
    relaxed atomics, read by the message thread when a preset is saved, and
    the message thread asks for a restring through an atomic mask. No
    allocation anywhere but toVar / fromVar.
*/

#include "../Common/DspCommon.h"

#include <array>
#include <atomic>

namespace luthier
{

enum class StringCoating { none = 0, thin, thick, numCoatings };

/** `string_age_accrual`: Off / Real time / x10 / x100. */
enum class AgeAccrual { off = 0, realTime, x10, x100, numRates };

//==============================================================================
/** What aging does to one string (string-aging.md 5). */
struct AgingFactors
{
    double brightness = 1.0;              ///< multiplies the loop-filter cutoff
    double sustain = 1.0;                 ///< multiplies the target T60
    double dispersion = 1.0;              ///< multiplies inharmonicity B
    double detuneCents = 0.0;             ///< open-string offset (3.4)
    double intonationCentsPerFret = 0.0;  ///< added to the intonation slope (3.4)
    double roughness = 1.0;               ///< squeak level (3.5)
    double squeakCentroid = 1.0;          ///< squeak band-pass centre multiplier (3.5)
    double squeakScale = 1.0;             ///< in-loop slide noise, relative to a Fresh spec (3.5)

    // The effective hours behind it, for the UI and the tests.
    double hours = 0.0, contaminationHours = 0.0, corrosionHours = 0.0, fatigueHours = 0.0;
};

//==============================================================================
class StringAging
{
public:
    // --- the section 3.2 constants ------------------------------------------------
    static constexpr double kBrightDrop = 0.525, kBrightTau = 25.0;
    static constexpr double kSustainDrop = 0.375, kSustainTau = 50.0;
    static constexpr double kDetuneMax = 5.17, kDetuneTau = 35.0;
    static constexpr double kDispersionRise = 0.10, kDispersionTau = 50.0;
    static constexpr double kLevelThreshold = 1.0e-3;   ///< about -60 dBFS (6)

    struct Inputs
    {
        double hours = 12.0;           ///< string_age_hours
        double corrosivity = 1.0;      ///< string_corrosivity, rho
        double detail = 0.0;           ///< string_age_detail, d. The engine starts on the legacy
                                       ///< table (Broken In at 12 h); new presets' parameter is 1.
        StringCoating coating = StringCoating::none;
        AgeAccrual accrual = AgeAccrual::off;
        double kRH = 1.0;              ///< environment.md 2.7's corrosion hook

        bool operator== (const Inputs& o) const noexcept
        {
            return hours == o.hours && corrosivity == o.corrosivity && detail == o.detail
                && coating == o.coating && accrual == o.accrual && kRH == o.kRH;
        }
    };

    StringAging();

    void prepare (double sampleRate) noexcept;

    /** Snaps the hours smoother to its target. Accrued hours are instrument
        state and survive (section 6). */
    void reset() noexcept;

    void setNumStrings (int n) noexcept;

    /** Which strings are wound, and whether the material is the coated one
        (which forces coating to at least Thin, 3.2). From refreshStringPhysics. */
    void setStringInfo (int stringIndex, bool wound, bool coatedMaterial) noexcept;

    /** j_i in [-1, 1], from the character seed (3.1). */
    void setJitter (int stringIndex, double bipolar) noexcept;

    /** Everything but kRH, which is the environment's (setCorrosionRate). */
    void setInputs (const Inputs& in) noexcept;

    /** environment.md 2.7's k_RH. */
    void setCorrosionRate (double kRH) noexcept { target.kRH = juce::jlimit (0.5, 3.0, std::isfinite (kRH) ? kRH : 1.0); }
    const Inputs& getInputs() const noexcept { return target; }

    /*  Once per block, audio thread. `levels` are the strings' level followers
        (for accrual). Returns true when the factors changed and must be pushed
        to the strings and the tuning engine. */
    bool advance (double seconds, const double* levels, int numLevels) noexcept;

    /** The factors computed by the last advance() (or recompute()). */
    const AgingFactors& getFactors (int stringIndex) const noexcept
    {
        return factors[(size_t) juce::jlimit (0, kMaxStrings - 1, stringIndex)];
    }

    /*  One string's factors at the target hours (no glide), from the inputs as
        set now. For refreshStringPhysics, which writes the detune synchronously
        as it always did, so nothing built during a structural change sees a
        stale tuning. Message thread; reads relaxed atomics. */
    AgingFactors computeNow (int stringIndex) const noexcept;

    /** Forces the next advance() to report a change (a string refresh re-pushes). */
    void markDirty() noexcept { dirty = true; }

    //==========================================================================
    // Restring (section 6). Message thread asks; the audio thread applies.
    void requestRestring (int stringIndex) noexcept;
    void requestRestringAll() noexcept;

    double getBaseHours (int s) const noexcept    { return baseHours[(size_t) juce::jlimit (0, kMaxStrings - 1, s)].load (std::memory_order_relaxed); }
    double getAccruedHours (int s) const noexcept { return accruedHours[(size_t) juce::jlimit (0, kMaxStrings - 1, s)].load (std::memory_order_relaxed); }

    /** The UI's live readouts (4 Hz): effective hours and brightness per string. */
    double getPublishedHours (int s) const noexcept      { return publishedHours[(size_t) juce::jlimit (0, kMaxStrings - 1, s)].load (std::memory_order_relaxed); }
    double getPublishedBrightness (int s) const noexcept { return publishedBright[(size_t) juce::jlimit (0, kMaxStrings - 1, s)].load (std::memory_order_relaxed); }
    juce::uint32 getPublishSerial() const noexcept       { return publishSerial.load (std::memory_order_relaxed); }

    //==========================================================================
    /*  The pure model, for one string at a given effective set age. Exposed
        for the tests and the UI: `hours` is H_i (3.1). */
    static AgingFactors compute (double hours, bool wound, bool coatedMaterial, double jitter,
                                 int stringIndex, const Inputs& in) noexcept;

    /** The same with the section 3.1 weights given: wc (contamination) and wk
        (corrosion), jitter included. compute() derives them and calls this. */
    static AgingFactors computeWithWeights (double hours, double wc, double wk, bool coatedMaterial,
                                            int stringIndex, const Inputs& in) noexcept;

    /** 3.1's weights for a string. */
    static void weightsFor (bool wound, double jitter, double detail, double& wc, double& wk) noexcept;

    /** The legacy table interpolated through (0, 12, 120) h, flat after. 0 sustain,
        1 brightness, 2 detune, 3 roughness, 4 squeak. */
    static double legacy (int column, double hours) noexcept;

    /** r_i: the same draw refreshStringPhysics used to make. */
    static double detuneSign (int stringIndex) noexcept;

    static double coatingRate (StringCoating c) noexcept;
    static double coatingBrightness (StringCoating c) noexcept;

    //==========================================================================
    /** section 8: {"base_hours": [...], "accrued_hours": [...]}. Message thread. */
    juce::var toVar() const;
    void fromVar (const juce::var& v);

private:
    void recompute() noexcept;
    void applyRestrings() noexcept;

    double sr = 44100.0;
    int numStrings = 6;

    Inputs target, applied;
    double smoothedHours = 12.0;
    bool dirty = true;

    std::array<bool, kMaxStrings> wound {};
    std::array<bool, kMaxStrings> coatedMaterial {};
    std::array<double, kMaxStrings> jitter {};

    std::array<std::atomic<double>, kMaxStrings> baseHours {};
    std::array<std::atomic<double>, kMaxStrings> accruedHours {};
    std::array<AgingFactors, kMaxStrings> factors {};

    std::atomic<juce::uint32> restringMask { 0 };
    std::atomic<bool> stateLoaded { false };

    std::array<std::atomic<double>, kMaxStrings> publishedHours {}, publishedBright {};
    std::atomic<juce::uint32> publishSerial { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StringAging)
};

} // namespace luthier
