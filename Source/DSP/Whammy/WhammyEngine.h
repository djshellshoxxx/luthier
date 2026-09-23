#pragma once

/*  Whammy bar / tremolo engine (engine spec 8).

    The bar moves the bridge, which changes every string's tension at once. The
    important distinction between bridge types is *how* that shared movement maps
    onto the individual strings:

      Vintage / Bigsby  - a fixed cent offset on every string. Single notes bend
                          correctly; chords go out of tune, exactly as they do on
                          a real vintage trem.
      Floyd Rose        - the same, over a far wider range, plus the spring
                          cavity ringing when the bar snaps back.
      TransTrem         - a frequency *ratio* on every string, so the intervals
                          inside a chord are preserved and the chord stays in tune
                          through the bend. That is the whole point of the design.

    The engine produces one offset per string; the tuning engine applies it.
*/

#include "../Common/DspCommon.h"
#include <array>

namespace luthier
{

class WhammyEngine
{
public:
    enum class BridgeType
    {
        Fixed,        ///< Hardtail: the bar does nothing.
        VintageTrem,  ///< Fender-style, usually down-only.
        FloydRose,    ///< Locking, full range, dive-bombs.
        TransTrem,    ///< Steinberger: interval-preserving.
        Bigsby,       ///< Short, gentle range.
        NumTypes
    };

    void prepare (double sampleRate, int numStrings) noexcept;
    void reset() noexcept;

    void setNumStrings (int n) noexcept { numStrings = juce::jlimit (1, kMaxStrings, n); }

    void setBridgeType (BridgeType t) noexcept;
    BridgeType getBridgeType() const noexcept { return bridgeType; }

    /** Bar position, -1 (fully depressed) to +1 (pulled up). */
    void setPosition (double normalised) noexcept;
    double getPosition() const noexcept { return positionSmooth.getTarget(); }

    /** Range in semitones for a full dive and a full pull-up. */
    void setRange (double downSemitones, double upSemitones) noexcept;
    double getDownRange() const noexcept { return downRange; }
    double getUpRange() const noexcept { return upRange; }

    /** Some vintage bridges only go down. */
    void setDownOnly (bool downOnly) noexcept { restrictToDown = downOnly; }

    /** TransTrem transposition detents: locks the bar at a whole number of
        semitones so the whole instrument transposes and stays in tune. */
    void setTransposeLock (int semitones) noexcept;
    int getTransposeLock() const noexcept { return transposeLock; }
    void setTransposeLockEnabled (bool enabled) noexcept { transposeLockEnabled = enabled; }

    /** Per-string whammy, for MPE controllers that can drive each string. */
    void setPerStringEnabled (bool enabled) noexcept { perString = enabled; }
    void setStringPosition (int stringIndex, double normalised) noexcept;

    /** Spring cavity resonance level on a Floyd Rose, 0 to 1. */
    void setSpringAmount (double amount) noexcept { springAmount = juce::jlimit (0.0, 1.0, amount); }

    //==========================================================================
    /** Advances the smoothers by one block and recomputes the per-string offsets. */
    void updateBlock (int numSamples) noexcept;

    /** Cent offset for one string this block. Add to the tuning engine's bend. */
    double getCentOffset (int stringIndex) const noexcept;

    /** Frequency ratio for one string. Equivalent to the cent offset; handy for
        the TransTrem tests. */
    double getRatio (int stringIndex) const noexcept;

    /** Produces the spring-cavity noise burst. Mixed into the instrument bus. */
    double processSpringNoise() noexcept;

    bool isActive() const noexcept { return std::abs (positionSmooth.getCurrent()) > 1.0e-4; }

private:
    void triggerSprings (double velocity) noexcept;

    double sr = 44100.0;
    int numStrings = 6;

    BridgeType bridgeType = BridgeType::VintageTrem;
    double downRange = 2.0;
    double upRange = 2.0;
    bool restrictToDown = false;
    bool perString = false;

    int transposeLock = 0;
    bool transposeLockEnabled = false;

    ExpSmoother positionSmooth;
    std::array<ExpSmoother, kMaxStrings> stringSmooth {};
    std::array<double, kMaxStrings> centOffsets {};

    // Spring cavity: a short filtered noise burst around 200-500 Hz.
    double springAmount = 0.5;
    double springEnv = 0.0;
    double springDecay = 0.9995;
    double lastPosition = 0.0;
    Biquad springBand1, springBand2;
    static constexpr uint64_t kSpringSeed = 0x5B1A9E77ull;
    RtRandom springRng { kSpringSeed };

    JUCE_LEAK_DETECTOR (WhammyEngine)
};

} // namespace luthier
