#pragma once

/*  Tuning stability (tuning-stability.md): event-driven tuning offsets with a
    physical cause each, and the retune actions that clear them.

    Control rate only: advance() runs once per block, before the engine turns
    frets into frequencies, and never touches an audio sample. Its one output
    is each string's stabilityCents, one more term in TuningEngine's open-
    string sum. With stability_amount at 0, advance() returns at once and every
    string's stabilityCents is exactly 0.0, so the sum is the legacy one (TS-01).

    Every offset comes from something that happened (0.1): a bend, a whammy
    excursion, a tuning change, a capo, playing time on new strings. The only
    chance is when a bound string lets go at the nut, and that is a seeded
    hash, so a render always repeats (0.4).

    Threading: the tuning-change events and the retune requests arrive on the
    message thread through atomics and are consumed at the next block; the
    rest runs on the audio thread. The per-string readouts for the UI are
    atomics. Nothing allocates.
*/

#include "../../DSP/Common/DspCommon.h"
#include "../../DSP/Whammy/WhammyEngine.h"
#include "../Guitar/StringMaterials.h"
#include <array>
#include <atomic>

namespace luthier
{

class TuningEngine;

//==============================================================================
/** tuning-stability.md 5: the parts the offsets come from (part-acoustics.md). */
struct TuningHardware
{
    double tunerRatio = 18.0;            ///< tuners.ratio
    double tunerStability = 0.85;        ///< tuners.stability
    bool   tunerLocking = false;         ///< tuners.locking
    double nutFriction = 0.35;           ///< nut.friction (bone)
    WhammyEngine::BridgeType bridge = WhammyEngine::BridgeType::Fixed;
    bool   acoustic = false;             ///< the bridge rotates with the top
    double capoPressure = 0.7;           ///< capo.pressure (trigger capo)
    double capoGapMm = 6.0;              ///< 2.6: trigger 6, screw 4, partial 6; 5 with no capo part
    double fretHeightMm = 1.0;           ///< setup_fret_height
    double scaleLengthMm = 648.0;
    StringMaterial material = StringMaterial::NickelPlatedSteel;

    int numStrings = 6;
    std::array<double, kMaxStrings> eaOverT {};     ///< E A_core / T, per string
    std::array<double, kMaxStrings> tensionN {};    ///< T, N
    std::array<double, kMaxStrings> openHz {};      ///< for Retune all's order

    /** 2.2: a locking nut, or a Floyd's, holds nothing by friction. */
    double effectiveNutFriction() const noexcept
    {
        return bridge == WhammyEngine::BridgeType::FloydRose ? 0.0 : juce::jlimit (0.0, 1.0, nutFriction);
    }
};

/** tuning-stability.md 4. */
enum class AutoRetune { off = 0, idle, transportStop, idleAndStop, numModes };

struct StabilitySettings
{
    double amount = 0.0;
    double settling = 1.0, nutBinding = 1.0, backlash = 1.0, saddleCreep = 1.0, bendMemory = 1.0, capoBias = 1.0;
    AutoRetune autoRetune = AutoRetune::idle;

    /** Any value past its stock end: the advanced caps (2) apply. */
    bool isAdvanced() const noexcept
    {
        return amount > 1.0 || settling > 2.0 || nutBinding > 2.0 || backlash > 2.0
            || saddleCreep > 2.0 || bendMemory > 2.0 || capoBias > 2.0;
    }
};

//==============================================================================
class StabilityModel
{
public:
    StabilityModel() = default;

    /** 0.3: cents = 865.6 (EA/T) dL/L - that is 1200 / ln 2 x 1/2. */
    static constexpr double kStrainCents = 865.617;

    /** 2.1: sigma for Fresh, Broken-in and Old strings. */
    static double sigmaForAge (StringAge age) noexcept;

    /** 2.1: the material factor of C_s = 8 c x factor. */
    static double settlingMaterialFactor (StringMaterial m) noexcept;

    /** 2.3's b in cents for one string of this hardware. */
    static double backlashCents (const TuningHardware& hw, int stringIndex) noexcept;

    /** 2.6's bias in cents at a capo fret. */
    static double capoBiasCents (const TuningHardware& hw, int stringIndex, int capoFret) noexcept;

    /** 2.4.1: k_f, 2.4.2: k_c, 2.4.3: k_r for a bridge. */
    static double floatingK (const TuningHardware& hw) noexcept;
    static double creepK (const TuningHardware& hw) noexcept;
    static double returnK (const TuningHardware& hw) noexcept;

    //==========================================================================
    void prepare (double sampleRate) noexcept { sr = sampleRate; }

    /** 5: zeroes every event offset and W, and restores sigma from its
        committed value. capoComp is session state and stays. */
    void reset() noexcept;

    void setHardware (const TuningHardware& hardware) noexcept { hw = hardware; }
    const TuningHardware& getHardware() const noexcept { return hw; }

    void setSettings (const StabilitySettings& s) noexcept { settings = s; }
    const StabilitySettings& getSettings() const noexcept { return settings; }

    void setSeed (juce::uint32 s) noexcept { seed = s; }

    /** New strings went on (a string-age change): sigma for every string. */
    void setStringAge (StringAge age) noexcept;

    //==========================================================================
    // Events. Any thread for onTuningChanged; audio thread for the rest.

    /** 5: an open pitch changed, from a tuning preset, a detune or fine tune. */
    void onTuningChanged (int stringIndex, double oldHz, double newHz) noexcept;

    /** 5: a pluck on the string, for the nut's ping and the backlash. */
    void onPluck (int stringIndex, double velocity) noexcept;

    //==========================================================================
    // 3: the retune actions, as an atomic mask consumed at block start.
    void requestRetune (juce::uint32 stringMask) noexcept { retuneMask.fetch_or (stringMask, std::memory_order_release); }
    void requestRetuneAll() noexcept { retuneAll.store (true, std::memory_order_release); }

    /*  7: a preset load. Sigma comes from its string age, capoComp clears,
        every offset clears, and the tuning changes the load itself makes are
        not events (they are the preset's tuning, not a retune of this one)
        until endStructuralApply(). Message thread. */
    void beginPresetLoad (StringAge age) noexcept;
    void endStructuralApply() noexcept { suppressTuningEvents.store (false, std::memory_order_release); }

    //==========================================================================
    struct BlockInput
    {
        int numSamples = 0;
        const double* levels = nullptr;        ///< each string's level
        const double* bendCents = nullptr;     ///< midi.getStringBendCents
        const double* whammyCents = nullptr;   ///< whammy.getCentOffset
        const int* capoFret = nullptr;         ///< tuning.getCapoFretFor
        bool transportPlaying = false;
    };

    struct BlockOutput
    {
        juce::uint32 retunedMask = 0;   ///< strings retuned this block
        bool retunedAll = false;        ///< Retune all ran (the character engine's too)
    };

    /** 5: once per block, before the frequency loop. Writes stabilityCents
        into the tuning engine only where it moved by more than 1e-4 c. */
    BlockOutput advance (const BlockInput& in, TuningEngine& tuning) noexcept;

    //==========================================================================
    // State for the UI and the tests.

    enum Cause { settle = 0, nut, backlash, bridge, memory, capo, numCauses };

    double getTotalCents (int s) const noexcept;
    double getCauseCents (int s, Cause c) const noexcept;

    /** The raw mechanism state, for the tests. */
    double getStuckCents (int s) const noexcept     { return at (s).stuck; }
    double getBendMemoryCents (int s) const noexcept{ return at (s).bendMem; }
    double getCreepCents (int s) const noexcept     { return at (s).creep; }
    double getCreepTarget (int s) const noexcept    { return at (s).creepTarget; }
    double getSigma (int s) const noexcept          { return at (s).sigma; }
    double getSigmaCommitted (int s) const noexcept { return at (s).sigmaCommitted; }
    double getCapoComp (int s) const noexcept       { return at (s).capoComp; }
    bool   isBacklashArmed (int s) const noexcept   { return at (s).backlashArmed; }
    int    getPingPluckIndex (int s) const noexcept { return at (s).pingAtPluck; }
    bool   isGliding (int s) const noexcept         { return at (s).glideRemaining > 0.0; }
    double getIdleSeconds() const noexcept          { return idleSeconds; }
    int    getAutoRetuneCount() const noexcept      { return autoRetunes; }

    /** Blocks advanced so far, for the UI's staleness (gui-engine-dataflow.md). */
    juce::uint32 getBlockCount() const noexcept { return blockCount.load (std::memory_order_relaxed); }

    //==========================================================================
    // 7: session state - sigma_committed and capoComp. Message thread.
    juce::var toVar() const;
    void fromVar (const juce::var& v);

private:
    struct StringState
    {
        double sigma = 0.2, sigmaCommitted = 0.2, w = 0.0;
        double stuck = 0.0, stuckRampLeft = 0.0;
        bool   backlashArmed = false;
        double backlash = 0.0, backlashTarget = 0.0, backlashRampLeft = 0.0;
        double creep = 0.0, creepTarget = 0.0;
        double equil = 0.0, returnErr = 0.0, bendMem = 0.0;
        double capoComp = 0.0;
        double bendPeak = 0.0;
        bool   bendActive = false;
        juce::uint32 pluckIndex = 0;
        int    pingAtPluck = -1;
        double glide = 0.0, glideRemaining = 0.0;
        double written = 0.0;
        double lastLevel = 0.0;
    };

    const StringState& at (int s) const noexcept { return strings[(size_t) juce::jlimit (0, kMaxStrings - 1, s)]; }

    void applyTuningChange (int s, double oldHz, double newHz) noexcept;
    void retuneString (int s, bool transportPlaying, const int* capoFret) noexcept;
    double scaledTotal (int s, const int* capoFret) const noexcept;
    void startBacklash (int s) noexcept;
    double totalTension() const noexcept;

    double sr = 48000.0;
    TuningHardware hw;
    StabilitySettings settings;
    juce::uint32 seed = 0x5EEDu;

    std::array<StringState, kMaxStrings> strings {};
    juce::uint32 excursionIndex = 0;
    double whammyPeak = 0.0;
    bool   whammyActive = false;
    double idleSeconds = 0.0;
    bool   idleFired = false, lastPlaying = false;
    int    autoRetunes = 0;

    // Message-thread hand-overs.
    std::atomic<juce::uint32> retuneMask { 0 };
    std::atomic<bool> retuneAll { false };
    std::array<std::atomic<double>, kMaxStrings> pendingOld {}, pendingNew {};
    std::atomic<juce::uint32> pendingMask { 0 };
    std::atomic<bool> suppressTuningEvents { false };
    std::atomic<int> pendingPresetAge { -1 };

    std::atomic<juce::uint32> blockCount { 0 };

    // The UI's readouts.
    std::array<std::array<std::atomic<float>, numCauses + 1>, kMaxStrings> readout {};

    JUCE_DECLARE_NON_COPYABLE (StabilityModel)
};

} // namespace luthier
