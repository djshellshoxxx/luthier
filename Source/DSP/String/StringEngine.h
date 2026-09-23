#pragma once

/*  One vibrating string (engine spec 5).

    Extended Karplus-Strong: a single lumped fractional delay line closed through
    a loop filter (frequency-dependent decay), a dispersion allpass cascade
    (inharmonicity) and a loss gain derived from the string's target T60.

        excitation + coupling
                 |
                 v
        [ fractional delay ] -> [ loop filter ] -> [ dispersion ] -> [ x loopGain ]
                 ^                                                          |
                 +----------------------------------------------------------+

    Everything that a player does to a string - fretting, bending, vibrato, sliding,
    palm-muting, harmonics, releasing - is expressed here as a change to the delay
    length, the loop filter cutoff or the loss gain. There are no sampled layers.
*/

#include "../Common/DspCommon.h"
#include "FractionalDelayLine.h"
#include "Excitation.h"

namespace luthier
{

class StringEngine
{
public:
    //==========================================================================
    /** The string's physical identity. Derived from material + gauge + scale
        length by StringMaterials, not entered by hand. */
    struct Physical
    {
        double scaleLengthMm      = 648.0;    ///< Nut to bridge.
        double diameterMm         = 0.66;     ///< Gauge (.026" here).
        double linearDensity      = 0.0012;   ///< kg/m.
        double youngsModulus      = 2.0e11;   ///< Pa. Sets stiffness/inharmonicity.
        double inharmonicityB     = 0.00015;  ///< Partial stretch coefficient.
        double sustainSeconds     = 4.5;      ///< Open-string T60.
        double openBrightnessHz   = 5000.0;   ///< Loop filter cutoff, undamped.
        bool   wound              = false;    ///< Wound strings squeak and are stiffer.
        double couplingSend       = 1.0;      ///< How strongly it drives the bridge.
    };

    /** How hard the string is being damped right now. */
    enum class Damping
    {
        Open,          ///< Ringing freely.
        LightTouch,    ///< Left hand resting: muted picking.
        PalmMute,      ///< Right-hand palm on the bridge.
        Released,      ///< Note off, no sustain pedal: finger lifted.
        Choked         ///< Fully stopped.
    };

    //==========================================================================
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void setPhysical (const Physical& p) noexcept;
    const Physical& getPhysical() const noexcept { return physical; }

    void setIndex (int i) noexcept { stringIndex = i; rng.setSeed (0x51E3D00Dull + (uint64_t) i * 7919ull); }
    int  getIndex() const noexcept { return stringIndex; }

    /** Sets the frequency the string is being driven toward. Smoothed internally,
        so bends, slides, vibrato and whammy all come through this one call. */
    void setTargetFrequency (double hz) noexcept;

    /** Jumps straight to a frequency with no glide. Used on note-on and reset. */
    void snapToFrequency (double hz) noexcept;

    double getTargetFrequency() const noexcept { return targetHz; }
    double getCurrentFrequency() const noexcept;
    double getCurrentDelaySamples() const noexcept { return smoothedDelay.getCurrent(); }

    /** How fast the delay length may glide, in seconds. Short for vibrato,
        longer for a lazy slide. */
    void setGlideTime (double seconds) noexcept;

    //==========================================================================
    /** Excites the string. Triggers voice-stealing if it is already ringing loudly
        and the excitation is a fresh pluck rather than a legato re-excitation. */
    void excite (const Excitation::Params& params) noexcept;

    /** Note off. `letRing` keeps the string open (sustain pedal / open string). */
    void release (bool letRing) noexcept;

    void setDamping (Damping d, double amount = 1.0) noexcept;
    Damping getDamping() const noexcept { return damping; }

    /** Extra decay scaling from string age, coating and user sustain control. */
    void setSustainScale (double scale) noexcept { sustainScale = juce::jlimit (0.05, 4.0, scale); needsLoopUpdate = true; }

    /*  part-acoustics.md 4: what the string is stopped against - a fret's
        material, or the nut for an open string - scales the loop filter's
        cutoff. 1 is the reference (nickel-silver fret, bone nut). */
    void setTerminationBrightness (double factor) noexcept
    {
        terminationBrightness = juce::jlimit (0.5, 1.5, factor);
        needsLoopUpdate = true;
    }

    /** Restricts the string to one partial, for natural/artificial harmonics.
        0 disables. */
    void setHarmonicRestriction (int partial) noexcept;

    /** Fret buzz: low action plus light fretting makes the string slap the frets. */
    void setFretBuzz (double amount, double actionMm) noexcept;

    /** Drives slide/squeak noise. `speed` is fret positions per second. */
    void setSlideSpeed (double fretsPerSecond) noexcept;
    void setNoiseAmount (double slideNoise, double fretNoise) noexcept;

    /** Triggers the short click of a finger landing on a fret. */
    void triggerFretNoise (double strength) noexcept;

    /** How much sympathetic energy this string accepts from the bridge. Harmonics
        and muted strings accept less. */
    double getCouplingReceptivity() const noexcept { return couplingReceptivity; }

    //==========================================================================
    /** Produces one sample. `couplingInput` is the sympathetic energy arriving
        from the other strings through the bridge for this sample. */
    double processSample (double couplingInput) noexcept;

    /** Level tap the coupling matrix reads. Already includes couplingSend. */
    double getBridgeOutput() const noexcept { return bridgeOut; }

    /** Smoothed RMS-ish level, for the UI and for feedback/validator logic. */
    double getLevel() const noexcept { return levelFollower.current(); }

    bool isRinging() const noexcept { return levelFollower.current() > 1.0e-5 || excitation.isActive(); }

    /** True once the string has been excited at least once since the last reset. */
    bool hasSounded() const noexcept { return sounded; }

    //==========================================================================
    /** Loop gain currently in use. Exposed for the validator and unit tests. */
    double getLoopGain() const noexcept { return loopGain; }
    double getLoopCutoffHz() const noexcept { return loopCutoffHz; }

private:
    void updateLoopCoefficients() noexcept;
    void updateDispersion() noexcept;
    double filterDelayCompensation() const noexcept;

    static constexpr int kMaxDispersionStages = 12;

    double sr = 44100.0;
    int stringIndex = 0;

    Physical physical;

    FractionalDelayLine delayLine;
    Excitation excitation;
    RtRandom rng { 0x51E3D00Dull };

    // --- loop ---------------------------------------------------------------
    OnePoleLP loopFilter;
    Allpass1  dispersion[kMaxDispersionStages];
    int       dispersionStages = 8;
    int       activeDispersionStages = 8;
    double    dispersionCoeff = 0.0;
    double    loopGain = 0.99;
    double    loopCutoffHz = 5000.0;
    double    loopFilterPole = 0.5;
    double    lastCoefficientHz = 0.0;
    bool      needsLoopUpdate = true;

    DCBlocker dcBlocker;
    EnvelopeFollower levelFollower;

    // --- pitch --------------------------------------------------------------
    double targetHz = 110.0;
    ExpSmoother smoothedDelay;
    double glideSeconds = 0.002;

    // --- articulation state -------------------------------------------------
    Damping damping = Damping::Open;
    double  dampingAmount = 1.0;
    double  sustainScale = 1.0;
    int     harmonicPartial = 0;

    double  terminationBrightness = 1.0;
    double  fretBuzzAmount = 0.0;
    double  fretActionMm = 1.6;
    double  buzzPhase = 0.0;

    double  slideSpeed = 0.0;
    double  slideNoiseAmount = 0.35;
    double  fretNoiseAmount = 0.35;
    OnePoleLP slideNoiseFilter;
    Biquad    slideNoiseBand;
    double  slideNoiseEnv = 0.0;
    double  fretNoiseEnv = 0.0;
    Biquad  fretNoiseBand;

    double  bridgeOut = 0.0;
    double  couplingReceptivity = 1.0;

    // --- voice stealing -----------------------------------------------------
    int     stealCountdown = 0;
    int     stealTotal = 64;
    bool    stealPending = false;
    double  stealGain = 1.0;
    Excitation::Params pendingParams {};

    bool    sounded = false;

    JUCE_LEAK_DETECTOR (StringEngine)
};

} // namespace luthier
