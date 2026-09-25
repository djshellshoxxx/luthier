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
#include "../Common/ConvolutionInstaller.h"
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

    /** How many responses have really been loaded into either mic path. */
    int getIrLoadCount() const noexcept { return pathA.loadCount + pathB.loadCount; }
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

    //==========================================================================
    /** Each mic on its own, as it was after its impulse response and the
        time-of-flight alignment but before the blend and the stereo placement.
        This is what routing-io's Aux 3 and Aux 4 carry.

        The pointer is the cabinet's own working buffer, so it is valid only
        until the next processBlock call, and only for as many samples as that
        call was given. It costs nothing: both mics are rendered separately
        anyway, and the blend is what throws the separation away. */
    const float* getMicTap (int slot) const noexcept
    {
        if (! micTapsValid)
            return nullptr;

        const auto& buf = (slot <= 0) ? bufferA : bufferB;

        return (buf.getNumChannels() > 0) ? buf.getReadPointer (0) : nullptr;
    }

    /** False when the cabinet is bypassed, or for slot 1 when the second mic is
        not in use - in both cases there is no separate mic signal to hand out. */
    bool hasMicTap (int slot) const noexcept
    {
        return micTapsValid && (slot <= 0 || dualMic);
    }

    int getMicTapNumSamples() const noexcept { return micTapSamples; }

private:
    struct MicPath
    {
        std::unique_ptr<juce::dsp::Convolution> convolution;
        std::atomic<bool> loaded { false };

        /** The file behind the installed response; see BodyEngine::loadedIrFile. */
        juce::File loadedFile;
        int loadCount = 0;

        // Held while an impulse response is swapped in. The loading thread takes
        // it and blocks; the audio thread try-locks and uses the fallback for the
        // one block a swap can overlap, so neither ever waits on the other.
        juce::SpinLock convolutionLock;

        // Procedural fallback: a speaker is a bandpass with a cone-breakup peak
        // and a sharp roll-off above it.
        Biquad lowShelf, bodyPeak, presencePeak, topRoll, topRoll2, highpass;

        void prepareFallback (double sr, const CabinetConfig& cfg) noexcept;
        void resetFallback() noexcept;
        inline double processFallback (double x) noexcept
        {
            double y = highpass.process (x);
            y = lowShelf.process (y);
            y = bodyPeak.process (y);
            y = presencePeak.process (y);
            // Two cascaded poles: a guitar speaker falls off far faster than
            // 12 dB/octave above cone breakup, and that steepness is most of
            // what separates a cabinet from a tweeter.
            return topRoll2.process (topRoll.process (y));
        }
    };

    void rebuildFallbacks() noexcept;

    double sr = 44100.0;
    int maxBlock = 512;
    bool prepared = false;
    bool enabled = true;
    bool dualMic = false;

    CabinetConfig configA, configB;

    MicPath pathA, pathB;

    ExpSmoother blendSmooth, widthSmooth;
    double phaseAlignMm = 0.0;

    // Delay line for the mic-B time-of-flight alignment.
    std::vector<double> alignBuffer;

    /*  The last kHistorySamples of the mono signal the mics see. A response is
        installed by pumping silence through juce::dsp::Convolution
        (ConvolutionInstaller), which also fills the convolver's latency buffer
        with silence: the first getLatency() samples it then produces are zeros,
        a hole of one block with a hard edge at each end, on every cabinet
        change under a sounding note (a preset morph crossing 0.5 measured a
        0.17 step). So a newly installed response is primed with what was
        actually playing before it is handed to the audio thread, and its
        first block is the tail of the note under the new cabinet. Written by
        the audio thread, read on the loading thread: a sample torn between
        the two is a rounding error in the primed tail, never a fault. */
    static constexpr int kHistorySamples = 16384;   // a power of two: 340 ms at 48 kHz
    std::vector<double> inputHistory;
    int historyIndex = 0;

    void primeConvolution (juce::dsp::Convolution& convolution);
    int alignSize = 0, alignMask = 0, alignIndex = 0, alignSamples = 0;

    juce::AudioBuffer<float> bufferA, bufferB;

    // Set at the end of processBlock, cleared when the cabinet is bypassed, so a
    // caller can never read a stale or never-written mic buffer.
    bool micTapsValid = false;
    int  micTapSamples = 0;

    DCBlocker dcL, dcR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CabinetEngine)
};

} // namespace luthier
