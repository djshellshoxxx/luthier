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
#include <array>
#include <atomic>

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
        double waveImpedance      = 0.4;      ///< body-coupling.md 3: Z0 = sqrt(T mu), kg/s.

        // sustain-and-decay.md 4 and 2.2: the core that carries the tension.
        double coreDiameterMm     = 0.0;      ///< 0 = unknown, the shape's pitch and ping stay off
        double tensionNewtons     = 0.0;
    };

    /*  sustain-and-decay.md 7: the eight shape values. At their defaults every
        behaviour skips its code, so the loop is bit-identical to the legacy one
        (SUS-01). */
    struct SustainShape
    {
        double attackTransient = 0.0;    ///< A
        double attackTimeSeconds = 0.030;///< tau_a
        double fastShare = 0.0;          ///< a
        double fastRatio = 0.2;          ///< rho = tau_f / tau_s
        double tensionMod = 0.0;         ///< S, 1 = physical
        double releaseSeconds = 0.0;     ///< T_r
        double releaseSagMm = 0.0;       ///< d
        double releaseRing = 0.0;        ///< R
        bool advanced = false;           ///< the strings family unlocked: the pitch clamp is +50 c

        bool isNeutral() const noexcept
        {
            return attackTransient <= 0.0 && fastShare <= 0.0 && tensionMod <= 0.0
                && releaseSeconds <= 0.0 && releaseSagMm <= 0.0 && releaseRing <= 0.0;
        }

        bool operator== (const SustainShape& o) const noexcept
        {
            return attackTransient == o.attackTransient && attackTimeSeconds == o.attackTimeSeconds
                && fastShare == o.fastShare && fastRatio == o.fastRatio && tensionMod == o.tensionMod
                && releaseSeconds == o.releaseSeconds && releaseSagMm == o.releaseSagMm
                && releaseRing == o.releaseRing && advanced == o.advanced;
        }
    };

    /** How hard the string is being damped right now. */
    enum class Damping
    {
        Open,          ///< Ringing freely.
        LightTouch,    ///< Left hand resting: muted picking.
        PalmMute,      ///< Right-hand palm on the bridge.
        Released,      ///< Note off, no sustain pedal: finger lifted.
        Choked,        ///< Fully stopped.
        Silenced,      ///< A hand flat on the string: gone in 80 ms whatever its sustain.
        Chuck,         ///< strum-dynamics 6.1: the fretting hand across the strings; amount 1 ends the note in ~10 ms.
        PalmMuteBass   ///< bass-techniques 7: the palm on a bass - shorter decay, more of the fundamental kept.
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

    /** Note off. `letRing` keeps the string open (sustain pedal / open string).

        sustain-and-decay.md 5: `fret` is where the note was stopped (0 for an
        open string, a harmonic, a bar or a fretless neck, none of which sag or
        ring). With the shape neutral this is exactly the legacy release. */
    void release (bool letRing, double fret = 0.0) noexcept;

    //==========================================================================
    // sustain-and-decay.md: the decay's shape.
    void setSustainShape (const SustainShape& shape) noexcept;
    const SustainShape& getSustainShape() const noexcept { return shape; }

    /** SUS-01's test hook: the shape code removed entirely. */
    void setShapeBypassedForTest (bool b) noexcept { shapeBypassed = b; }

    /** Where the string is stopped, in frets from the nut (capo included), for
        the ping's and the tension's vibrating length. Set before excite(). */
    void setStoppedFret (double fret) noexcept { stoppedFret = juce::jmax (0.0, fret); }

    /** 3: the E-Bow and the feedback path add energy, so they restart the clock. */
    void restartShapeClock() noexcept { samplesSinceExcite = 0; }

    /** 4: the tension-modulation offset in cents, for the UI's readout. Any thread. */
    double getTensionCents() const noexcept { return tensionCentsUi.load (std::memory_order_relaxed); }

    /** The shape's current pitch ratio (tension x sag x ring), and its pieces. */
    double getShapePitchRatio() const noexcept { return pitchRatio; }

    /** 2.2 and 4: the open string's longitudinal frequency and kappa at S = 1. */
    double getLongitudinalHz() const noexcept { return longitudinalHz; }
    double getKappaAtPhysical() const noexcept { return kappa0; }

    /** The shape's current brightness and decay-rate multipliers (b, m). */
    double getBrightnessMultiplier() const noexcept { return brightMul; }
    double getDecayRateMultiplier() const noexcept { return decayMul; }

    /*  bass-techniques 6 (MODEL-GAPS): a finger comes to rest on the string - the
        rest stroke. Scales what is already in the loop toward (1 - depth) over
        about 1.5 ms, for one trip round it; pair it with a damping so the loop
        keeps it stopped. Audio thread. */
    void touch (double depth) noexcept;

    void setDamping (Damping d, double amount = 1.0) noexcept;
    Damping getDamping() const noexcept { return damping; }
    double getDampingAmount() const noexcept { return dampingAmount; }

    /** fingerstyle-attack.md 2: this note's bridge drive, as a multiplier on
        Physical::couplingSend (a rest stroke drives the top 1.3x). */
    void setCouplingSendScale (double scale) noexcept { couplingSendScale = juce::jlimit (0.0, 4.0, scale); }
    double getCouplingSendScale() const noexcept { return couplingSendScale; }

    /** Extra decay scaling from string age, coating and user sustain control. */
    void setSustainScale (double scale) noexcept { sustainScale = juce::jlimit (0.05, 4.0, scale); needsLoopUpdate = true; }

    /*  string-aging.md 5: what the set's age does to this string - multipliers
        on the loop cutoff, the target T60 and the inharmonicity. Kept apart
        from sustainScale (per-note character, slide and magnet values) so
        neither can overwrite the other. 1, 1, 1 is a fresh string. */
    void setAgingFactors (double brightness, double sustain, double dispersion) noexcept
    {
        if (brightness == agingBrightness && sustain == agingSustain && dispersion == agingDispersion)
            return;

        agingBrightness = juce::jlimit (0.05, 2.0, brightness);
        agingSustain = juce::jlimit (0.05, 2.0, sustain);

        if (dispersion != agingDispersion)
        {
            agingDispersion = juce::jlimit (0.5, 2.0, dispersion);
            updateDispersion();
        }

        needsLoopUpdate = true;
    }

    double getAgingBrightness() const noexcept { return agingBrightness; }
    double getAgingSustain() const noexcept { return agingSustain; }

    /*  body-coupling.md 3: the wave arriving at the bridge - the loop's return
        (after the loop filter, the dispersion and the loss), before anything is
        injected - which BodyCouplingBank turns into the force on the bridge. */
    double getBridgeWave() const noexcept { return bridgeWave; }

    /*  part-acoustics.md 4: what the string is stopped against - a fret's
        material, or the nut for an open string - scales the loop filter's
        cutoff. 1 is the reference (nickel-silver fret, bone nut). */
    void setTerminationBrightness (double factor) noexcept
    {
        terminationBrightness = juce::jlimit (0.5, 1.5, factor);
        needsLoopUpdate = true;
    }

    /** Kept as a shim (harmonic-realism.md 2): adds a contact for the given
        partial at its first node. 0 clears every contact. */
    void setHarmonicRestriction (int partial) noexcept;

    /** animated-strings.md 4.1: the partial the string is restricted to; 0 = none. */
    int getHarmonicPartial() const noexcept { return harmonicPartial; }
    //==========================================================================
    /*  harmonic-realism.md 2: a contact - a finger, thumb or tap touching the
        string at a point - realised in the lumped loop as the n-tap node comb
        H = (1 - g) + g e C_n(z). Partials with a node under the touch survive,
        the rest lose (1 - g) per round trip. With no contact active the path
        is skipped and the string is bit-identical to one without contacts. */
    struct Contact
    {
        double positionFromBridge = 0.5;   ///< fraction of the vibrating length
        double vibratingLengthMm  = 648.0; ///< for d and w in mm
        double strength           = 0.6;   ///< g
        double widthMm            = 2.5;   ///< w
        double seconds            = 0.07;  ///< auto-release; <= 0 holds until cleared
    };

    static constexpr int kMaxContacts = 4;

    /** Returns the slot, or -1 if all four are in use. */
    int  addContact (const Contact& c) noexcept;
    void clearContact (int slot) noexcept;
    void clearAllContacts() noexcept;

    bool hasActiveContact() const noexcept { return numActiveContacts > 0; }

    /** The partial a slot's contact selected (0 off-node), and its efficiency. */
    int getContactPartial (int slot) const noexcept;
    double getContactEfficiency (int slot) const noexcept;

    /** True when the loop is long enough to hold the comb for this contact at
        the current pitch (2: compensated - (n - 1) M >= 2). An off-node touch
        is always realisable: it is uniform damping. */
    bool canRealiseContact (const Contact& c) const noexcept;

    /** The model's own frequency of partial n at the current loop - the loop's
        phase delay solved for n cycles, so dispersion is included. */
    double getPartialFrequency (int n) const noexcept;

    /** A fresh pluck is waiting for the old note's 5 ms fade (voice steal). */
    bool isStealPending() const noexcept { return stealPending; }

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
        from the other strings through the bridge for this sample, scaled by the
        string's receptivity; `directInput` is injected as it is (harmonic-
        realism.md 2: scrape catches, slap and tap impulses). */
    double processSample (double couplingInput, double directInput = 0.0) noexcept;

    /*  processSample in two halves, for body-coupling.md 3: beginSample reads
        the loop and leaves the wave arriving at the bridge in getBridgeWave();
        endSample injects and writes. A bank that needs every string's
        current bridge wave before any string is written sits between them,
        with no lag. processSample is exactly the two in a row. */
    void beginSample() noexcept;
    double endSample (double couplingInput, double directInput = 0.0) noexcept;

    /** Level tap the coupling matrix reads. Already includes couplingSend. */
    double getBridgeOutput() const noexcept { return bridgeOut; }

    /** Smoothed RMS-ish level, for the UI and for feedback/validator logic. */
    double getLevel() const noexcept { return levelFollower.current(); }

    bool isRinging() const noexcept { return levelFollower.current() > 1.0e-5 || excitation.isActive(); }

    /** True once the string has been excited at least once since the last reset. */
    bool hasSounded() const noexcept { return sounded; }

    //==========================================================================
    //==========================================================================
    // cpu-quality-modes 2.4.

    /** The dispersion cap: 4 stages at or above `fourFromHz`, 2 at or above
        `twoFromHz` (0 disables a rule). Latched in excite() only, so a ringing
        note keeps its stage count until it is re-excited. */
    void setDispersionRule (double fourFromHz, double twoFromHz) noexcept { ruleFourHz = fourFromHz; ruleTwoHz = twoFromHz; }

    /** Idle-string sleep on or off (Medium, Low). */
    void setSleepEnabled (bool on) noexcept;

    /** E-Bow and feedback strings are never put to sleep or truncated. */
    void setSleepExempt (bool exempt) noexcept { sleepExempt = exempt; if (exempt) wake(); }

    /** Fades what is left over `seconds` and then sleeps (ring-out truncation
        and E3). Ignored while exempt. With `holdUntilExcited` (E3) the string
        stays asleep through coupling until it is plucked or touched again. */
    void fadeToSleep (double seconds, bool holdUntilExcited = false) noexcept;

    bool isSleeping() const noexcept { return sleeping; }
    bool isFadingToSleep() const noexcept { return fadeLeft > 0; }
    int getActiveDispersionStages() const noexcept { return activeDispersionStages; }
    int getLatchedDispersionStages() const noexcept { return latchedStages; }

    /** Loop gain currently in use. Exposed for the validator and unit tests. */
    double getLoopGain() const noexcept { return loopGain; }
    double getLoopCutoffHz() const noexcept { return loopCutoffHz; }

private:
    void updateLoopCoefficients() noexcept;
    void updateDispersion() noexcept;
    double filterDelayCompensation() const noexcept;

    static constexpr int kMaxDispersionStages = 12;

    // cpu-quality-modes 2.4 --------------------------------------------------------
    void latchDispersion() noexcept;
    void applyCappedDispersion() noexcept;
    void goToSleep() noexcept;
    void wake() noexcept { sleeping = false; holdAsleep = false; quietSamples = 0; }

    double ruleFourHz = 0.0, ruleTwoHz = 0.0;
    int latchedStages = 8;              ///< dispersionStages unless capped
    double cappedCoeff = 0.0;
    double cappedCompensation = 0.0;    ///< holds the fundamental where High has it
    bool sleepEnabled = true, sleepExempt = false, sleeping = false;
    bool sleptThisSample = false, holdAsleep = false;
    int quietSamples = 0, sleepAfterSamples = 4410;
    int fadeLeft = 0, fadeTotal = 1;

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

    // touch() (MODEL-GAPS): a hand laid on the string, applied to what is in the loop.
    double touchGain = 1.0, touchDepthGain = 1.0, touchCoeff = 0.0;
    int touchSamplesLeft = 0;
    double  dampingAmount = 1.0;
    double  sustainScale = 1.0;
    int     harmonicPartial = 0;

    double  terminationBrightness = 1.0;
    double  agingBrightness = 1.0, agingSustain = 1.0, agingDispersion = 1.0;   // string-aging.md 5
    double  bridgeWave = 0.0;                                                     // body-coupling.md 3
    double  pendingDelayOut = 0.0;
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
    double  dampingReceptivity = 1.0;   ///< the damping state's value, without contacts
    double  couplingSendScale = 1.0;

    // --- contacts (harmonic-realism.md 2) --------------------------------------
    struct ContactState
    {
        bool   active = false;
        Contact contact {};
        int    partial = 0;          ///< n; 0 = off-node
        double efficiency = 0.0;     ///< e
        double combSpacing = 0.0;    ///< M, samples
        double gain = 0.0;           ///< g now (ramped)
        double target = 0.0;         ///< g heading
        int    samplesLeft = -1;     ///< -1 holds
        std::array<FractionalDelayLine::TapKernel, 8> kernels {};   ///< per comb tap, cached
    };

    std::array<ContactState, kMaxContacts> contacts {};
    int    numActiveContacts = 0;
    double contactRampStep = 1.0 / 44.1;

    void updateContactSpacing() noexcept;
    void updateReceptivity() noexcept;
    double applyContacts (double delayOut, double compensated) noexcept;

    // --- voice stealing -----------------------------------------------------
    int     stealCountdown = 0;
    int     stealTotal = 64;
    bool    stealPending = false;
    double  stealGain = 1.0;
    Excitation::Params pendingParams {};

    bool    sounded = false;

    // --- sustain-and-decay.md 7 -----------------------------------------------
    static constexpr int kShapeTick = 32;

    void onShapeExcite (const Excitation::Params& p) noexcept;
    void updateShapeTick() noexcept;
    void updateShapeConstants() noexcept;

    SustainShape shape;
    bool shapeActive = false, shapeBypassed = false;
    int64_t samplesSinceExcite = 0;
    double exciteStrength = 0.0;
    int tickCounter = 0;
    double stoppedFret = 0.0;
    double kappa0 = 0.0, longitudinalHz = 0.0;

    double brightMul = 1.0, decayMul = 1.0;
    double pitchRatio = 1.0, pitchRatioTarget = 1.0, pitchRatioStep = 0.0;
    double tensionRatio = 1.0;

    bool releaseActive = false;
    double releaseRamp = 0.0, sagTargetCents = 0.0, ringRatio = 1.0;
    int ringSamplesLeft = 0;
    double ringGain = 1.0;

    struct Resonator
    {
        double c1 = 0.0, c2 = 0.0, y1 = 0.0, y2 = 0.0;
        void reset() noexcept { y1 = y2 = 0.0; }
        inline double process (double x) noexcept
        {
            const double y = x + c1 * y1 - c2 * y2;
            y2 = y1;
            y1 = flushDenormal (y);
            return y;
        }
    };

    Resonator ping1, ping2;
    int pingSamplesLeft = 0;
    std::atomic<float> tensionCentsUi { 0.0f };

    JUCE_LEAK_DETECTOR (StringEngine)
};

} // namespace luthier
