#pragma once

/*  Body Engine (engine spec 6).

    Two implementations, both built, user-selectable per preset:

      Convolution  - partitioned FFT convolution against a measured body IR.
                     Accurate to whatever body was captured, and cheap.
      Modal        - 30 to 48 resonators built from the body's physical dimensions.
                     Costs more, but the body responds when the user changes its
                     size, wood, thickness or bracing.

    Identity rule 3: the string signal always passes through a body model. The
    "no body" path exists only as an explicit experimental toggle.
*/

#include "../Common/DspCommon.h"
#include "../Common/ConvolutionInstaller.h"
#include "../Common/IrVariants.h"
#include "../../Support/QualityProfile.h"
#include "../../Model/Guitar/BodyModels.h"
#include <atomic>
#include <vector>

namespace luthier
{

//==============================================================================
/** One resonator of the modal bank: a constant-peak-gain bandpass. */
class ModalResonator
{
public:
    void prepare (double sampleRate) noexcept { sr = sampleRate; reset(); }
    void reset() noexcept { filter.reset(); }

    void set (double frequencyHz, double q, double gainLinear) noexcept
    {
        freq = juce::jlimit (20.0, sr * 0.47, frequencyHz);
        qFactor = juce::jlimit (0.5, 250.0, q);
        gain = gainLinear;
        filter.setBandpass (sr, freq, qFactor);
    }

    inline double process (double x) noexcept { return filter.process (x) * gain; }

    double getFrequency() const noexcept { return freq; }
    double getQ() const noexcept { return qFactor; }
    double getGain() const noexcept { return gain; }

private:
    double sr = 44100.0, freq = 100.0, qFactor = 30.0, gain = 1.0;
    Biquad filter;
};

//==============================================================================
class BodyEngine
{
public:
    enum class Mode { Convolution, Modal, Hybrid, Bypassed };

    BodyEngine();
    ~BodyEngine();

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    //==========================================================================
    void setMode (Mode m) noexcept;
    Mode getMode() const noexcept { return mode; }

    /** Rebuilds the modal bank from a body configuration. Safe to call from the
        message thread: the new bank is staged and swapped in on the audio thread. */
    void setBodyConfig (const BodyConfig& cfg);

    /** RT-SAFETY (CODEX_RTSAFETY P0): a modal bank built ahead of time, fixed
        size, so handing it to the audio thread is a bounded copy. */
    struct ModalBank
    {
        std::array<BodyMode, BodyModels::kMaxModes> modes {};
        std::array<int, BodyModels::kMaxModes> priority {};
        int count = 0;
    };

    /** Builds a body's bank (modes and the quality cap's priority order).
        Allocates (in @p scratch) and sorts: message thread or worker only. */
    static void buildModalBank (const BodyConfig& cfg, ModalBank& out, std::vector<BodyMode>& scratch);

    /** Audio thread, at a block boundary: installs a bank built by
        buildModalBank for @p cfg. No lock, no allocation, no sort. */
    void applyPrebuiltBank (const BodyConfig& cfg, const ModalBank& bank) noexcept;
    const BodyConfig& getBodyConfig() const noexcept { return config; }

    /** Loads a body IR from a file. Asynchronous inside juce::dsp::Convolution;
        the engine keeps producing sound throughout (engine spec 13.4). */
    bool loadImpulseResponse (const juce::File& file);

    /** Loads a body IR from raw samples, used by the built-in IR bank. */
    void loadImpulseResponse (const float* samples, int numSamples, double irSampleRate);

    bool hasImpulseResponse() const noexcept { return irLoaded.load(); }
    juce::String getLoadedIrName() const { return loadedIrName; }

    //==========================================================================
    /** Dry/wet between the raw string sum and the body response, 0 to 1.
        The UI calls this "Body". */
    void setAmount (double amount) noexcept;

    /** Extra low-frequency emphasis for the air resonance, in dB. */
    void setAirResonanceGainDb (double db) noexcept;

    /** The body's air resonance, in Hz, for the current configuration.

        Read by the character engine: a dead spot is neck-body coupling, so how
        much a note loses depends on how close it is to this (character-wear 2). */
    double getAirResonanceHz() const noexcept;

    /*  environment.md 4 / body-coupling.md 3: block-rate multipliers on the
        modal bank - plate frequency, air frequency and plate Q. The resonators
        are re-designed at the next block only when one has moved by more than
        0.05 %, so 1, 1, 1 costs nothing and changes nothing. Audio thread. */
    void setRuntimeScaling (double plateFreqMul, double airFreqMul, double plateQMul, double airQMul = 1.0) noexcept;
    double getRuntimePlateScale() const noexcept { return runtimePlate; }
    double getRuntimeAirScale() const noexcept { return runtimeAir; }

    /** Overall output trim so that switching bodies is not a jump in level. */
    void setOutputGainDb (double db) noexcept;

    //==========================================================================
    void processBlock (juce::dsp::AudioBlock<float>& block) noexcept;

    /** Mono convenience path used by the offline renderer and the tests. */
    void processMono (double* samples, int numSamples) noexcept;

    /** Latency the convolution path adds, in samples. Reported to the host. */
    int getLatencySamples() const noexcept;

    //==========================================================================
    /*  cpu-quality-modes 2.1 / 2.3: the level's IR variant and modal cap. The
        modes that run are the eight lowest-frequency ones plus the rest by
        energy; dropped modes ramp to 0 over 20 ms before being skipped (a hard
        switch drops them at once). Audio thread. */
    void setQualityLevel (const QualityProfile& profile, bool hard) noexcept;

    /** Modes actually running (<= getNumModes()). */
    int getRunningModeCount() const noexcept { return juce::jmin (modeRunCount, numActiveModes); }

    /** The priority order the modal cap uses: indices into getModes(). */
    const int* getModePriority() const noexcept { return modePriority.data(); }

    const IrVariants& getIrVariants() const noexcept { return irVariants; }

    /** Number of active modes in the modal bank. */
    int getNumModes() const noexcept { return numActiveModes; }
    const BodyMode* getModes() const noexcept { return activeModes.data(); }

private:
    void rebuildModalBank();
    void applyStagedBank() noexcept;
    void applyRuntimeScaling (bool force) noexcept;

    // setRuntimeScaling's targets and what the resonators were last set from.
    double runtimePlate = 1.0, runtimeAir = 1.0, runtimeQ = 1.0, runtimeAirQ = 1.0;
    double designedPlate = 1.0, designedAir = 1.0, designedQ = 1.0, designedAirQ = 1.0;

    double sr = 44100.0;
    int maxBlock = 512;
    bool prepared = false;

    Mode mode = Mode::Convolution;
    BodyConfig config;

    // --- convolution ---------------------------------------------------------
    std::unique_ptr<juce::dsp::Convolution> convolution;
    std::atomic<bool> irLoaded { false };
    juce::String loadedIrName;

    /** The file behind the installed response, so a guitar rebuild that keeps
        the body does not reload it (a part swap parks the audio thread while
        it runs). Cleared by prepare() and by a response given as samples. */
    juce::File loadedIrFile;

public:
    /** How many responses have really been loaded (cached reloads do not count). */
    int getIrLoadCount() const noexcept { return irLoadCount; }

private:
    int irLoadCount = 0;

    // Held while a response is swapped in; the audio thread try-locks and leaves
    // the convolution out of the path for the one block a swap can overlap.
    juce::SpinLock convolutionLock;

    // --- modal ---------------------------------------------------------------
    std::array<ModalResonator, BodyModels::kMaxModes> resonators;
    std::array<BodyMode, BodyModels::kMaxModes> activeModes {};
    int numActiveModes = 0;

    std::array<BodyMode, BodyModels::kMaxModes> stagedModes {};
    int stagedCount = 0;
    std::atomic<bool> stagedReady { false };

    std::vector<BodyMode> buildScratch;

    // --- cpu-quality-modes ----------------------------------------------------
    IrVariants irVariants;
    std::array<int, BodyModels::kMaxModes> modePriority {}, stagedPriority {};
    std::array<int, BodyModels::kMaxModes> activePriority {};   ///< the installed bank's order
    int modeCap = BodyModels::kMaxModes;
    int modeRunCount = BodyModels::kMaxModes;   ///< modes processed (priority order)
    int modeTarget = BodyModels::kMaxModes;     ///< where a ramp ends
    int modeRampLeft = 0, modeRampTotal = 1;

    void updateModeRun (bool hard) noexcept;
    bool modesCapped() const noexcept { return modeRunCount < numActiveModes || modeRampLeft > 0; }
    inline double runModes (double in, int sampleInBlock) noexcept;
    void advanceModeRamp (int numSamples) noexcept;

    // --- shared --------------------------------------------------------------
    Biquad airShelf;
    DCBlocker dcLeft, dcRight;
    ExpSmoother amountSmooth, gainSmooth;

    juce::AudioBuffer<float> wetBuffer;

    double airGainDb = 0.0;
    double outputGainDb = 0.0;

    juce::CriticalSection rebuildLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BodyEngine)
};

} // namespace luthier
