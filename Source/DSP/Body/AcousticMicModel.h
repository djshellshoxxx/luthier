#pragma once

/*  External microphones around an acoustic guitar (mic-placement.md 3).

    An acoustic guitar's top radiates from regions with different voices: the
    soundhole booms at the air resonance, the lower bout is warm, the bridge
    carries the attack, the 12th fret is bright and hears the strings directly.
    A mic's position on the top weights five radiators by a Gaussian whose
    width grows with distance, so close up it hears one region and at a metre
    the whole guitar. Then the mic's own voice, and the same angle, polar,
    level, level-match, floor and time-of-arrival terms as the cabinet's mics.

    Mixed after the guitar's circuit, before the input gain: a microphone does
    not go through the guitar's electronics. At ac_mic_mix = 0 the engine skips
    this entirely, so an acoustic preset renders exactly as before (MP-20).
*/

#include "../Amp/MicPlacement.h"
#include "../../Model/Guitar/AcousticLandmarks.h"

namespace luthier
{

struct AcousticMicPlacement
{
    double along = 3.6;       ///< 0 tail, 1 lower bout, 2 bridge, 3 soundhole, 4 12th fret
    double across = 0.0;      ///< -1 .. 1 of the lower bout's half-width, + treble
    double distCm = 20.0;
    double angleDeg = 15.0;
};

/** The composite filter one acoustic mic placement produces. */
struct AcousticMicTerms
{
    double airHz = 100.0, airDb = 0.0;       // Q 2.5
    double lowDb = 0.0;                      // 220 Hz, Q 1
    double midDb = 0.0;                      // 2.5 kHz, Q 1 (angle's A_theta included)
    double shelfDb = 0.0;                    // 6 kHz high shelf (angle's T_theta included)
    double send = 0.0;                       // string-direct send
    double proxHz = 140.0, proxDb = 0.0;     // Q 0.9
    double presHz = 5500.0, presDb = 0.0;    // Q 1
    double topHz = 14000.0;                  // mic low-pass
    double gain = 1.0;                       // level, match, polarity, calibration
    double floorGain = 0.0, floorDelaySec = 0.0;
    double pathM = 0.2;
    double weights[5] = {};                  // lower bout, bridge, soundhole, upper bout, 12th fret
};

class AcousticMicModel
{
public:
    static constexpr int kNumRadiators = 5;
    static constexpr double kFloorHeightM = 0.7;      // a seated player
    static constexpr double kLevelRefCm = 10.0;

    struct Input
    {
        AcousticLandmarks landmarks;
        double airHz = 100.0;
        MicType mic = MicType::SM57;
        AcousticMicPlacement placement;
        bool levelMatch = true;
        double floorRho = 0.45;
        double calibration = 1.0;
    };

    /** Pure, allocation-free. */
    static AcousticMicTerms evaluate (const Input& in) noexcept;

    /** Gaussian radiator weights, normalised to sum to 1. */
    static void radiatorWeights (const AcousticLandmarks& lm, double alongX, double acrossY,
                                 double distCm, double out[kNumRadiators]) noexcept;

    /** Width of the radiator blur at a distance, mm (see the .cpp). */
    static double blurSigmaMm (double distCm) noexcept;

    /** The response of a set of terms to the body signal, at `hz`. */
    static std::complex<double> response (const AcousticMicTerms& t, double sr, double hz) noexcept;

    //==========================================================================
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    /** From rebuildBodyFromSpec: where the landmarks are, and f_air (0 -> 100 Hz). */
    void setBody (const AcousticLandmarks& lm, double airHz) noexcept;
    const AcousticLandmarks& getLandmarks() const noexcept { return landmarks; }

    void setMicType (int slot, MicType m) noexcept;
    void setPlacement (int slot, const AcousticMicPlacement& p) noexcept;
    void setSecondMicOn (bool on) noexcept { secondOn = on; }
    void setBlend (double b) noexcept { blendTarget = juce::jlimit (0.0, 1.0, b); }
    void setTimeOfFlightMode (TofMode m) noexcept { tofMode = m; }
    void setLevelMatch (bool on) noexcept { levelMatch = on; }
    void setFloorReflectivity (double rho) noexcept { floorRho = juce::jlimit (0.0, 1.0, rho); }

    /** The calibration constant K: the default placement's 1 kHz level
        matches the internal-mic path's (mic-placement.md 3). */
    double getCalibration() const noexcept { return calibration; }
    static double internalMicGainAt1k (double sr) noexcept;

    /** Renders one block. `body` is the body's output, `strings` the raw
        string sum. Writes the mono mix to `out` and each mic to its tap. */
    void processBlock (const float* body, const double* strings, double* out, int numSamples) noexcept;

    const double* getMicTap (int slot) const noexcept;
    bool isSecondMicOn() const noexcept { return secondOn; }

    int getProcessCount() const noexcept { return processCount; }
    AcousticMicTerms getTerms (int slot) const noexcept { return mics[(size_t) juce::jlimit (0, 1, slot)].terms; }

    /** The weighted distance of the external mics, for the room bleed. */
    double getWeightedDistanceM() const noexcept;

private:
    struct Mic
    {
        MicType type = MicType::SM57;
        AcousticMicPlacement target;
        ExpSmoother sAlong, sAcross, sDist, sAngle;
        ExpSmoother gainSmooth, sendSmooth, floorGainSmooth, floorDelaySmooth;
        AcousticMicTerms terms;
        TptSvf air, low, mid, shelf, prox, pres, top, sendHp;
        OnePoleLP floorLp;
        std::vector<double> floorBuffer;
        int floorMask = 0, floorIndex = 0;
        SlewedDelayLine tof;
        std::vector<double> tap;
        std::vector<float> scratch;
    };

    void updateControl (Mic& m) noexcept;
    void applyTerms (Mic& m) noexcept;
    void recalibrate() noexcept;
    Input makeInput (const Mic& m, bool smoothed) const noexcept;

    double sr = 44100.0;
    int maxBlock = 512;
    bool prepared = false;

    AcousticLandmarks landmarks;
    double airHz = 100.0;
    bool secondOn = false;
    double blendTarget = 0.5;
    ExpSmoother blendSmooth;
    TofMode tofMode = TofMode::Aligned;
    bool levelMatch = true;
    double floorRho = 0.45;
    double calibration = 1.0;

    std::array<Mic, 2> mics;
    int controlCountdown = 0;
    int processCount = 0;
};

} // namespace luthier
