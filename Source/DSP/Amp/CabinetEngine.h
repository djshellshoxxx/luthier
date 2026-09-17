#pragma once

/*  Cabinet + microphone engine (engine spec 13).

    The cabinet is an impulse response: a speaker in a box in front of a mic is a
    linear system, and convolution reproduces it exactly. Two IRs can be loaded at
    once and blended, which is how a real engineer gets a usable guitar sound - an
    SM57 on the cap edge for bite plus a ribbon further back for body.

    If no IR is available the engine falls back to a procedural speaker model so the
    plugin never emits a raw, fizzy, un-cabinet'd signal. A guitar amp without a
    speaker sounds like a wasp in a tin, and shipping that as the failure mode would
    be worse than silence.
*/

#include "../Common/DspCommon.h"
#include <atomic>
#include <memory>

namespace luthier
{

//==============================================================================
enum class CabinetType
{
    Cab1x12Open, Cab1x12Closed, Cab2x12Open, Cab2x12Closed,
    Cab4x12, Cab4x12Vintage, Cab1x15Bass, Cab4x10Bass, Cab8x10Bass,
    AcousticDI, NumCabinets
};

enum class SpeakerType
{
    Greenback, Vintage30, G12H, G12T75, JensenC12, AlnicoBlue, EVM12L,
    BassCeramic, NumSpeakers
};

enum class MicType
{
    SM57, SM7B, MD421, U87, RibbonR121, C414, D112, NumMics
};

enum class MicPosition
{
    OnAxisCentre, OnAxisCapEdge, OffAxis45, OffAxisEdge, Rear, NumPositions
};

enum class MicDistance
{
    Close, Medium, Far, NumDistances
};

//==============================================================================
struct CabinetConfig
{
    CabinetType cabinet   = CabinetType::Cab4x12;
    SpeakerType speaker   = SpeakerType::Vintage30;
    MicType     mic       = MicType::SM57;
    MicPosition position  = MicPosition::OnAxisCapEdge;
    MicDistance distance  = MicDistance::Close;
    double      speakerAge = 0.4;   ///< 0 new and stiff, 1 broken in.
};

//==============================================================================
class CabinetEngine
{
public:
    CabinetEngine();
    ~CabinetEngine();

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    //==========================================================================
    void setEnabled (bool e) noexcept { enabled = e; }
    bool isEnabled() const noexcept { return enabled; }

    /** Primary mic. */
    void setConfigA (const CabinetConfig& cfg);
    const CabinetConfig& getConfigA() const noexcept { return configA; }

    /** Second mic; only used when the blend is above zero. */
    void setConfigB (const CabinetConfig& cfg);
    const CabinetConfig& getConfigB() const noexcept { return configB; }

    void setDualMicEnabled (bool e) noexcept;
    bool isDualMicEnabled() const noexcept { return dualMic; }

    /** 0 = all mic A, 1 = all mic B. */
    void setMicBlend (double blend) noexcept;

    /** How far apart the two mics are placed in the stereo field, 0 to 1. */
    void setStereoWidth (double width) noexcept;

    /** Compensates the time-of-flight difference between the two mics, in
        millimetres. Getting this wrong is what makes a two-mic blend sound thin. */
    void setPhaseAlignMm (double mm) noexcept;

    //==========================================================================
    bool loadImpulseResponse (int slot, const juce::File& file);
    void loadImpulseResponse (int slot, const float* samples, int numSamples, double irSampleRate);
    bool hasImpulseResponse (int slot) const noexcept;

    /** Where a configuration's IR file should live, relative to Resources/CabIRs. */
    static juce::String irFileNameFor (const CabinetConfig& cfg);

    static const char* getCabinetName (CabinetType t) noexcept;
    static const char* getSpeakerName (SpeakerType t) noexcept;
    static const char* getMicName (MicType t) noexcept;
    static const char* getPositionName (MicPosition p) noexcept;
    static const char* getDistanceName (MicDistance d) noexcept;

    //==========================================================================
    void processBlock (juce::AudioBuffer<float>& buffer) noexcept;

    int getLatencySamples() const noexcept;

private:
    struct MicPath
    {
        std::unique_ptr<juce::dsp::Convolution> convolution;
        std::atomic<bool> loaded { false };

        // Procedural fallback: a speaker is a bandpass with a cone-breakup peak
        // and a sharp roll-off above it.
        Biquad lowShelf, bodyPeak, presencePeak, topRoll, highpass;

        void prepareFallback (double sr, const CabinetConfig& cfg) noexcept;
        void resetFallback() noexcept;
        inline double processFallback (double x) noexcept
        {
            double y = highpass.process (x);
            y = lowShelf.process (y);
            y = bodyPeak.process (y);
            y = presencePeak.process (y);
            return topRoll.process (y);
        }
    };

    void rebuildFallbacks() noexcept;

    double sr = 44100.0;
    int maxBlock = 512;
    bool enabled = true;
    bool dualMic = false;

    CabinetConfig configA, configB;

    MicPath pathA, pathB;

    ExpSmoother blendSmooth, widthSmooth;
    double phaseAlignMm = 0.0;

    // Delay line for the mic-B time-of-flight alignment.
    std::vector<double> alignBuffer;
    int alignSize = 0, alignMask = 0, alignIndex = 0, alignSamples = 0;

    juce::AudioBuffer<float> bufferA, bufferB;

    DCBlocker dcL, dcR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CabinetEngine)
};

} // namespace luthier
