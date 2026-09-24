#pragma once

/*  The bridge admittance bank (body-coupling.md).

    The strings drive the bridge, the bridge moves the body, and the moving body
    pushes back on every string. This is that return path: a small bank of the
    body's own modes (the same BodyModels modes the radiated body uses), seen
    from the bridge as an admittance, with the strings' loading folded into each
    mode analytically so the discrete system stays passive (2.3):

        Q'_k = 1 / (1/Q_k + Z_tot / (M_k w_k))
        Y'_k = (kappa / M_k) s / (s^2 + (w_k/Q'_k) s + w_k^2)

        F[n] = sum_j 2 Z0_j a_j[n-1] + F_tap[n]
        v[n] = sum_k R_k(F)[n]
        c_i[n] = -v[n]       added to every string's coupling input

    Wolf notes, tap-tone response and sympathetic ring through the body come out
    of it; nothing is scheduled (ground rule 1).

    Threading: design() runs on the message thread and allocates only into a
    member scratch vector; stage() hands the POD result to the audio thread
    through an atomic flag and a try-lock (BodyEngine::applyStagedBank's
    pattern). Everything else is audio-thread, allocation-free.
*/

#include "../Common/DspCommon.h"
#include "../Body/BodyEngine.h"
#include "../../Model/Guitar/Chambering.h"

#include <array>
#include <atomic>
#include <vector>

namespace luthier
{

/** The bridge and tailpiece as the body sees them (part-acoustics.md 5). */
struct BridgeCoupling
{
    double massKg = 0.110;     ///< bridge + tailpiece
    double coupling = 0.70;    ///< 0-1, how much string energy reaches the body
};

//==============================================================================
/** design()'s result: POD, so it can cross to the audio thread. */
struct BodyCouplingDesign
{
    static constexpr int kMaxModes = 16;

    std::array<double, kMaxModes> f {};       ///< Hz, before runtime scaling
    std::array<double, kMaxModes> q {};       ///< unloaded Q
    std::array<double, kMaxModes> m {};       ///< effective mass at the bridge, kg
    std::array<bool, kMaxModes> isAir {};
    int count = 0;                             ///< strongest first

    std::array<double, kMaxStrings> z0 {};    ///< each string's wave impedance, kg/s
    int numStrings = 6;
    double coupling = 0.70;
    Chambering chambering = Chambering::solid;

    double totalImpedance() const noexcept
    {
        double z = 0.0;

        for (int s = 0; s < numStrings; ++s)
            z += z0[(size_t) s];

        return z;
    }
};

/** The runtime multipliers every block (3, "Scaling"). */
struct BodyCouplingScaling
{
    double plateFreq = 1.0, airFreq = 1.0, q = 1.0, airQ = 1.0, mass = 1.0;   ///< q: plate modes, airQ: air modes
};

//==============================================================================
class BodyCouplingBank
{
public:
    static constexpr double kMaxModeHz = 1200.0;   ///< 3: the K strongest below 1.2 kHz
    static constexpr double kEnergyCap = 0.35;     ///< CouplingMatrix's per-sample cap
    static constexpr double kTapSeconds = 0.001;   ///< 2.4: a 1 ms half-sine
    static constexpr double kMaxKappa = 0.95;      ///< a margin under unity for the one-sample lag

    /** 2.2: the lowest mode's effective mass by chambering, kg. */
    static double baseMass (Chambering c) noexcept;

    /** part-acoustics.md 5's bridge table, keyed on the compiled spec's bridge. */
    static BridgeCoupling bridgeFor (int whammyBridgeType, bool acoustic, bool resonator) noexcept;

    BodyCouplingBank() = default;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    //==========================================================================
    /** Message thread: the mode set for a body, a bridge and a set of strings. */
    BodyCouplingDesign design (const BodyConfig& body, Chambering chambering, const BridgeCoupling& bridge,
                               const double* z0, int numStrings);

    /** Message thread: hands a design to the audio thread. */
    void stage (const BodyCouplingDesign& d) noexcept;

    /** The design last staged, for the UI's mode list and wolf map. Message thread. */
    const BodyCouplingDesign& getStagedDesign() const noexcept { return staged; }

    //==========================================================================
    // Block rate, audio thread.

    void setAmount (double amount) noexcept { amountTarget = juce::jlimit (0.0, 1.0, amount); }
    double getAmount() const noexcept { return amountTarget; }

    /** body_coupling_modes: how many of the design's modes run (4, 8, 12, 16). */
    void setModeCount (int k) noexcept { modeCount = juce::jlimit (1, BodyCouplingDesign::kMaxModes, k); }
    int getModeCount() const noexcept { return modeCount; }

    void setScaling (const BodyCouplingScaling& s) noexcept { scalingTarget = s; }

    /** Applies a staged design and any scaling change. Call once per block. */
    void beginBlock() noexcept;

    /** 2.4 / engine-technique-layer 3.4: a body tap. `position` 0 top, 1 side,
        2 back weights the modes. Bypasses the strings: the bank rings and the
        strings answer through it. */
    void driveDirect (double impulse, int position) noexcept;

    /** A plain tap at the bridge (noise_body_knock). */
    void injectTap (double force) noexcept { driveDirect (force, 0); }

    /** True when processSample would do anything. */
    bool isActive() const noexcept { return amountTarget > 0.0 || amountNow > 0.0; }

    /*  Per sample. `bridgeWaves` are the strings' previous-sample bridge waves;
        the bank ADDS into `couplingInputs`. */
    void processSample (const double* bridgeWaves, double* couplingInputs, int numStrings) noexcept;

    //==========================================================================
    // Readouts.
    int getNumActiveModes() const noexcept { return juce::jmin (modeCount, active.count); }
    double getModeFrequency (int k) const noexcept { return resonators[(size_t) juce::jlimit (0, BodyCouplingDesign::kMaxModes - 1, k)].getFrequency(); }
    double getModeGain (int k) const noexcept { return resonators[(size_t) juce::jlimit (0, BodyCouplingDesign::kMaxModes - 1, k)].getGain(); }
    double getModeLoadedQ (int k) const noexcept { return resonators[(size_t) juce::jlimit (0, BodyCouplingDesign::kMaxModes - 1, k)].getQ(); }

    /** Fraction of processed samples where the cap engaged, since reset. */
    double getCapFraction() const noexcept { return samplesRun > 0 ? (double) samplesCapped / (double) samplesRun : 0.0; }

    //==========================================================================
    // The physics, pure: for the UI's mode list and wolf map, and the tests.

    struct ModeView { double hz, q, loadedQ, massKg, peakAdmittance; bool isAir; };

    static ModeView viewMode (const BodyCouplingDesign& d, int k, const BodyCouplingScaling& s) noexcept;

    /** Re Y'(f) summed over the first `k` modes, m/(N s). */
    static double realAdmittance (const BodyCouplingDesign& d, int k, const BodyCouplingScaling& s,
                                  double hz, double amount) noexcept;

    /*  5: the wolf map's cell. Predicted sustain loss 1 - s_s/(s_s + s_b) for a
        string of impedance z0 whose own decay is `sigmaString` (1/s), with
        s_b = 2 f z0 Re Y'(f) summed over the fundamental and its first two
        partials (weighted 1, 1/2, 1/3). */
    static double predictedLoss (const BodyCouplingDesign& d, int k, const BodyCouplingScaling& s,
                                 double amount, double f0, double z0, double sigmaString) noexcept;

    /*  5: the wolf map, strings x frets 0..numFrets, row-major, from a design.
        `openHz` and `sigmaOpen` are each string's open pitch and its own
        amplitude decay rate open (1/s); a fretted note's own decay follows the
        string's pitch scaling, sigma ~ f^0.4. Message thread. */
    static void wolfMap (const BodyCouplingDesign& d, int k, const BodyCouplingScaling& s, double amount,
                         const double* openHz, const double* sigmaOpen, int numStrings, int numFrets,
                         float* dest) noexcept;

private:
    void redesignResonators() noexcept;

    double sr = 44100.0;

    std::vector<BodyMode> scratch;
    BodyCouplingDesign staged, pending, active;
    std::atomic<bool> stagedReady { false };
    juce::SpinLock stageLock;

    std::array<ModalResonator, BodyCouplingDesign::kMaxModes> resonators;
    std::array<double, BodyCouplingDesign::kMaxModes> tapWeight {};
    int modeCount = 8, runningCount = 0;

    BodyCouplingScaling scalingTarget, scalingNow, scalingDesigned;
    bool needsRedesign = true;

    double amountTarget = 0.0, amountNow = 0.0, amountStep = 0.0;

    // The tap (2.4).
    double tapAmplitude = 0.0;
    int tapSample = 0, tapLength = 44;
    std::array<double, BodyCouplingDesign::kMaxModes> pendingTapWeight {};

    juce::int64 samplesRun = 0, samplesCapped = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BodyCouplingBank)
};

} // namespace luthier
