#pragma once

/*  Continuous microphone placement (mic-placement.md 2 and 5).

    Each mic's convolution always holds one anchor IR - the shipped Cap Edge,
    close response for its (cabinet, speaker, mic). Everything placement does is
    a continuous correction applied after it, so dragging a mic never swaps an
    IR and never clicks.

    Three consumers, one model (ground rule 0.5): the engine's stage, the live
    response plot and the tests all evaluate `MicPlacementModel::evaluate`, a
    pure, allocation-free function from a placement to a set of filter terms,
    and `PlacementResponse` turns those terms into the exact digital response
    the stage's filters have. So the plot is what you hear.

    Every term is a delta from the anchor placement (Cap Edge, 2.5 cm, 0
    degrees, front, speaker 1). At the anchor every term is exactly zero, the
    filters are exact pass-throughs and the stage is bit-transparent - which is
    how an old preset lands on today's sound.
*/

#include "CabinetVoices.h"
#include "../Common/DspCommon.h"
#include <complex>
#include <array>
#include <vector>

namespace luthier
{

//==============================================================================
/** Where one mic is (mic-placement.md 2.1). A POD. */
struct MicPlacement
{
    double x = 0.35;          ///< Cone-landmark units: 0 cap centre, 0.35 cap edge, 1 cone edge.
    double y = 0.0;
    double distCm = 2.5;      ///< From the grille plane.
    double angleDeg = 0.0;    ///< Off-axis.
    int speaker = 1;          ///< 1-based, left to right, top to bottom.
    bool rear = false;        ///< Behind the cabinet.

    double radius() const noexcept { return std::hypot (x, y); }

    bool operator== (const MicPlacement& o) const noexcept
    {
        return x == o.x && y == o.y && distCm == o.distCm && angleDeg == o.angleDeg
               && speaker == o.speaker && rear == o.rear;
    }

    bool operator!= (const MicPlacement& o) const noexcept { return ! (*this == o); }

    static MicPlacement anchor() noexcept { return {}; }
};

enum class TofMode { Aligned = 0, Physical = 1 };

//==============================================================================
/** The filter terms one placement produces. All gains are deltas from the
    anchor, so a default-constructed set is the identity. */
struct PlacementTerms
{
    // Bells: centre, gain. Q is fixed per term (kProxQ ...).
    double proxHz = 140.0,  proxDb = 0.0;
    double presHz = 2600.0, presDb = 0.0;
    double capHz  = 4400.0, capDb  = 0.0;
    double surrHz = 1560.0, surrDb = 0.0;

    // The HF corner shift: two cascaded high shelves, each carrying half.
    double shelfHz = 5200.0, shelfDbEach = 0.0;

    // Behind the cabinet: 0..1, and whether the back panel low-passes it.
    double rearAmount = 0.0;
    bool   rearLowpass = false;

    // Broadband: level, level match and polarity, as one signed linear gain.
    double gain = 1.0;

    // The floor bounce the anchor IR does not already hold.
    double floorGain = 0.0;
    double floorDelaySec = 0.0;

    // Source-to-capsule path, for the two-mic time of arrival.
    double pathM = 0.0;

    bool isIdentity() const noexcept
    {
        return proxDb == 0.0 && presDb == 0.0 && capDb == 0.0 && surrDb == 0.0
               && shelfDbEach == 0.0 && rearAmount == 0.0 && gain == 1.0 && floorGain == 0.0;
    }
};

//==============================================================================
class MicPlacementModel
{
public:
    static constexpr double kProxQ = 0.9;
    static constexpr double kPresQ = 1.3;
    static constexpr double kCapQ = 2.0;
    static constexpr double kSurrQ = 1.5;
    static constexpr double kShelfQ = 0.9;   // fitted to the legacy 8th-order corner, see the .cpp
    static constexpr double kRearLowpassHz = 600.0;
    static constexpr double kFloorLowpassHz = 4000.0;
    static constexpr double kSpeedOfSound = 343.0;
    static constexpr double kMinDistCm = 0.5;
    static constexpr double kRearExtraPathM = 0.12;
    static constexpr double kAnchorDistCm = 2.5;
    static constexpr double kAnchorU = 0.35;

    /** The rate the model's own 1 kHz trim is read at: high enough that the
        bilinear warp is nil, so the trim is the same at every host rate. */
    static constexpr double kAnalogRate = 4.0e6;

    /** Everything evaluate needs. `rearAmount` and `variation` are the
        engine's glided values of the two discrete switches (rear, speaker). */
    struct Input
    {
        CabinetType cabinet = CabinetType::Cab4x12;
        SpeakerType speaker = SpeakerType::Vintage30;
        MicType mic = MicType::SM57;

        double x = kAnchorU, y = 0.0, distCm = kAnchorDistCm, angleDeg = 0.0;
        double rearAmount = 0.0;

        /** Per-speaker variation rho1..3 in [-1, 1]; zero on speaker 1. */
        double variation[3] = { 0.0, 0.0, 0.0 };

        /** Height of the miked speaker's centre above the floor. */
        double speakerHeightM = 0.62;

        bool levelMatch = true;

        /** Floor reflectivity rho (room material; 0.35 with the room off). */
        double floorRho = 0.45;

        /** Physical ToF leaves a rear mic's polarity inverted; Aligned flips
            it back the way an engineer would. */
        bool invertRearPolarity = false;
    };

    /** Pure, noexcept, allocation-free (mic-placement.md 2.2). */
    static PlacementTerms evaluate (const Input& in) noexcept;

    //==========================================================================
    // The individual mechanisms, public so the tests can pin each to the spec.

    /** Piecewise-linear presence/axis A_r(u) in dB and HF corner T_r(u). */
    static double axisDb (double u) noexcept;
    static double cornerFactor (double u) noexcept;

    /** Proximity factor P(d), log-distance interpolated; 1 at 2.5 cm. */
    static double proximity (double distCm) noexcept;

    /** Dust-cap and surround terms, dB. */
    static double capDb (double u) noexcept;
    static double surroundDb (double u) noexcept;

    /** Beaming weight: 1 close up (the mic hears its spot), 0 far away. */
    static double beamingWeight (double distCm, double coneRadiusCm) noexcept;

    /** Area means over the cone face u in [0, 1] (computed once). */
    struct Means { double axis, corner, cap, surround; };
    static const Means& areaMeans() noexcept;

    /** Inverse-distance level in dB, 0 at the anchor distance. */
    static double distanceLevelDb (double distCm, double coneRadiusCm) noexcept;

    /** Broadband polar level in dB and polarity, for a pattern coefficient a. */
    static double polarLevelDb (double a, double angleDeg) noexcept;
    static double polarSign (double a, double angleDeg) noexcept;

    /** The floor bounce's path length and gain before the anchor is removed. */
    static double floorPathM (double distCm, double heightM) noexcept;
    static double floorGainRaw (double distCm, double heightM, double rho) noexcept;

    /** Source-to-capsule path in metres (mic-placement.md 2.3). */
    static double pathLengthM (double u, double distCm, double coneRadiusCm, double rearAmount) noexcept;

    /** rho1..3 for speaker k of a cabinet; all zero for speaker 1. */
    static void speakerVariation (CabinetType cab, int speaker, double out[3]) noexcept;

    static double floorRhoFor (int roomMaterial, bool roomOn) noexcept;

    /** The tone terms' own level at 1 kHz, which level match trims. */
    static double analogToneDbAt1k (const PlacementTerms& t) noexcept;
};

//==============================================================================
/** The exact digital response of a set of terms, as the stage realises it at a
    sample rate. The plot and MP-25 use it. */
class PlacementResponse
{
public:
    static std::complex<double> response (const PlacementTerms& t, double sampleRate, double hz) noexcept;
    static double magnitudeDb (const PlacementTerms& t, double sampleRate, double hz) noexcept;

    /** The same response without the broadband gain, the floor or the rear
        low-pass: just the placement's tonal delta. */
    static double toneDb (const PlacementTerms& t, double sampleRate, double hz) noexcept;
};

//==============================================================================
/** One mic's placement stage (mic-placement.md 5). Mono, in place.

    Inputs are smoothed (30 ms), `evaluate` runs every 32 samples and the TPT
    filters take the new coefficients; the signed gain glides over 20 ms so a
    polarity flip passes through zero; rear and the speaker index glide their
    derived values over 20 ms. */
class MicPlacementStage
{
public:
    static constexpr int kControlInterval = 32;

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void setVoice (CabinetType cab, SpeakerType speaker, MicType mic) noexcept;
    void setPlacement (const MicPlacement& p) noexcept;
    const MicPlacement& getPlacement() const noexcept { return target; }

    void setLevelMatch (bool on) noexcept { levelMatch = on; }
    void setFloorReflectivity (double rho) noexcept { floorRho = juce::jlimit (0.0, 1.0, rho); }
    void setInvertRearPolarity (bool invert) noexcept { invertRear = invert; }

    /** cpu-quality-modes (INTEGRATE-2): QualityProfile::micEvaluateEvery. */
    void setEvaluateEvery (int n) noexcept { evaluateEvery = juce::jmax (1, n); }

    /** Identity (AcousticDI, a user IR in the slot). Crossfaded over 20 ms. */
    void setBypassed (bool b) noexcept { bypassTarget = b ? 0.0 : 1.0; }
    bool isBypassed() const noexcept { return bypassTarget == 0.0; }

    /** Snaps every smoother to its target: no glide in flight. */
    void snapToTargets() noexcept;

    void process (float* data, int numSamples) noexcept;

    /** The smoothed source-to-capsule path, for the ToF line. */
    double getPathLengthM() const noexcept { return terms.pathM; }

    const PlacementTerms& getCurrentTerms() const noexcept { return terms; }
    MicPlacementModel::Input getCurrentInput() const noexcept;

    int getEvaluateCount() const noexcept { return evaluateCount; }

private:
    void updateControl() noexcept;
    void applyTerms (bool glide) noexcept;
    static bool sameInput (const MicPlacementModel::Input& a, const MicPlacementModel::Input& b) noexcept;

    MicPlacementModel::Input lastInput;
    bool lastInputValid = false;
    int cachedSpeaker = -1;
    CabinetType cachedCabinet = CabinetType::Cab4x12;
    double cachedRho[3] = { 0.0, 0.0, 0.0 };
    double cachedHeight = 0.62;

    double sr = 44100.0;
    bool prepared = false;

    CabinetType cabinet = CabinetType::Cab4x12;
    SpeakerType speakerType = SpeakerType::Vintage30;
    MicType mic = MicType::SM57;

    MicPlacement target;
    bool levelMatch = true;
    double floorRho = 0.45;
    bool invertRear = false;
    double bypassTarget = 1.0;

    // Control-rate smoothers (one step per 32 samples).
    ExpSmoother sx, sy, sd, sa;          // 30 ms
    ExpSmoother sRear, sHeight;          // 20 ms
    ExpSmoother sVar[3];                 // 20 ms

    // Audio-rate smoothers.
    ExpSmoother gainSmooth, floorGainSmooth, floorDelaySmooth, wetSmooth, rearMixSmooth;

    PlacementTerms terms;
    int controlCountdown = 0;
    int evaluateCount = 0;
    int evaluateEvery = 1, evaluateSkip = 0;

    TptSvf prox, pres, cap, surr, shelf1, shelf2, rearLp;
    OnePoleLP floorLp;

    std::vector<double> floorBuffer;
    int floorMask = 0, floorIndex = 0;
};

//==============================================================================
/** A fractional delay line whose delay glides under a slew limit, with 4-point
    Lagrange interpolation (mic-placement.md 2.3). Mono. */
class SlewedDelayLine
{
public:
    static constexpr double kMaxSlew = 0.0025;   ///< samples per sample

    void prepare (double sampleRate, double maxSeconds);
    void reset() noexcept;

    void setTargetDelaySamples (double d) noexcept;
    void snapToTarget() noexcept { current = targetDelay; }
    double getCurrentDelay() const noexcept { return current; }
    double getTargetDelay() const noexcept { return targetDelay; }

    void process (float* data, int numSamples) noexcept;

private:
    std::vector<double> buffer;
    int mask = 0, writeIndex = 0;
    double current = 0.0, targetDelay = 0.0, maxDelay = 0.0;
};

} // namespace luthier
