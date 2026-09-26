#pragma once

/*  The live surface's audio-path pieces (live-performance.md sections 6, 7 and 8).

      - KillSwitch: a momentary hard mute with a 3 ms fade at each end.
      - MonitorMix: the performer's own mix, main plus sidechain plus click.
      - ExpressionCalibration: turning a pedal's real CC range into a clean 0..1.

    The first two run on the audio thread and obey engine.md section 0: doubles
    throughout, nothing allocated once prepare() has run, and a reset() that
    leaves no state behind. The third is control-only and never touches audio.
*/

#include "../DSP/Common/DspCommon.h"

#include <array>
#include <atomic>

namespace luthier
{

//==============================================================================
/** live-performance 6: a momentary mute that silences the output for as long as
    it is held, without touching the DSP underneath.

    The fade is the whole design. Muting by writing zeroes would click on a
    waveform that is anywhere but zero, so the gain ramps to silence over three
    milliseconds and back up over three on release. The string engine keeps
    decaying throughout: releasing the switch drops the player back into the note
    where it would have been, not where it was when they hit it. */
class KillSwitch
{
public:
    /** live-performance 6: 3 ms each way. */
    static constexpr double kFadeMs = 3.0;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** Engages or releases. Safe from any thread. */
    void setActive (bool shouldBeActive) noexcept
    {
        active.store (shouldBeActive, std::memory_order_relaxed);
    }

    bool isActive() const noexcept { return active.load (std::memory_order_relaxed); }

    /** True while the gain is anywhere below unity, which is what the header's
        red pill watches. */
    bool isAudiblyMuting() const noexcept { return gain < 0.999; }

    void processBlock (juce::AudioBuffer<float>& buffer) noexcept;

    /** jam-mode.md 7 (FEAT-JAM): applies this block's ramp again, to a signal
        mixed in after processBlock (the Jam band), without advancing it. */
    void applyBlockRamp (juce::AudioBuffer<float>& buffer, int numSamples) const noexcept;

    /** The current gain, for tests and for the UI. */
    double getGain() const noexcept { return gain; }

private:
    double sr = 44100.0;
    double gain = 1.0;
    double step = 1.0;
    double blockStartGain = 1.0, blockTarget = 1.0;   ///< FEAT-JAM: the last block's ramp

    std::atomic<bool> active { false };

    JUCE_LEAK_DETECTOR (KillSwitch)
};

//==============================================================================
/** live-performance 7: the performer's own mix.

    Sums the main output with whatever is arriving on the sidechain - a backing
    track, the rest of the band - plus the metronome's click, and sends the result
    somewhere the player can hear it without changing what the audience hears.

    The whole path is skipped when there is nothing to do, which is what the spec
    means by consuming no CPU when idle: no sidechain, no click and a monitor
    level at -inf means the block returns before any filter runs. */
class MonitorMix
{
public:
    static constexpr double kSilenceDb = -60.0;

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    //==========================================================================
    void setLevelDb (double db) noexcept;
    double getLevelDb() const noexcept { return levelDb.load (std::memory_order_relaxed); }

    void setPan (double pan) noexcept;      ///< -1 left, 0 centre, +1 right.
    double getPan() const noexcept { return pan.load (std::memory_order_relaxed); }

    /** How much of the sidechain goes into the monitor, in dB. */
    void setSidechainLevelDb (double db) noexcept;
    double getSidechainLevelDb() const noexcept { return sidechainDb.load (std::memory_order_relaxed); }

    /** How much of the main output goes into the monitor, in dB. */
    void setMainLevelDb (double db) noexcept;
    double getMainLevelDb() const noexcept { return mainDb.load (std::memory_order_relaxed); }

    void setClickLevelDb (double db) noexcept;
    double getClickLevelDb() const noexcept { return clickDb.load (std::memory_order_relaxed); }

    //==========================================================================
    // Three-band monitor EQ. Shelves at each end, a bell in the middle.

    void setEqLowDb (double db) noexcept;
    void setEqMidDb (double db) noexcept;
    void setEqHighDb (double db) noexcept;

    double getEqLowDb() const noexcept  { return eqLowDb.load (std::memory_order_relaxed); }
    double getEqMidDb() const noexcept  { return eqMidDb.load (std::memory_order_relaxed); }
    double getEqHighDb() const noexcept { return eqHighDb.load (std::memory_order_relaxed); }

    //==========================================================================
    /** True when this block would change anything. When false the caller can skip
        the monitor path entirely. */
    bool isActive (bool haveSidechain, bool haveClick) const noexcept;

    /** Renders the monitor mix into `destination`.

        @param destination   stereo, cleared and written by this call
        @param main          the plugin's own output
        @param sidechain     the sidechain input, or nullptr
        @param click         mono click from the metronome, or nullptr
        @param numSamples    how many
    */
    void processBlock (juce::AudioBuffer<float>& destination,
                       const juce::AudioBuffer<float>& main,
                       const juce::AudioBuffer<float>* sidechain,
                       const float* click,
                       int numSamples) noexcept;

    double getOutputLevel() const noexcept { return outputLevel.load (std::memory_order_relaxed); }

private:
    void updateFilters() noexcept;

    double sr = 44100.0;

    std::atomic<double> levelDb { -100.0 };
    std::atomic<double> pan { 0.0 };
    std::atomic<double> mainDb { 0.0 };
    std::atomic<double> sidechainDb { 0.0 };
    std::atomic<double> clickDb { -6.0 };

    std::atomic<double> eqLowDb { 0.0 }, eqMidDb { 0.0 }, eqHighDb { 0.0 };

    /** Remembered so the filters are only recomputed when a band actually moved.
        Recomputing three biquads per block for a monitor bus nobody adjusted is
        pure waste. */
    double lastLow = 0.0, lastMid = 0.0, lastHigh = 0.0;

    Biquad lowShelfL, lowShelfR, midBellL, midBellR, highShelfL, highShelfR;

    ExpSmoother levelSmooth, sidechainSmooth, mainSmooth, clickSmooth;

    DCBlocker dcL, dcR;

    std::atomic<double> outputLevel { 0.0 };

    JUCE_LEAK_DETECTOR (MonitorMix)
};

//==============================================================================
/** live-performance 8: one expression pedal's calibration.

    Pedals disagree about what "all the way down" means: some send 0 to 127, some
    send 12 to 118, and most have a stretch at each end where the pot has stopped
    moving but the wiper has not. A calibration records the real travel and the
    dead zones, and maps what is left onto a clean 0 to 1. */
struct ExpressionCalibration
{
    /** How a pedal's travel maps onto the value it controls. */
    enum class Curve { linear = 0, logarithmic, exponential, sCurve, numCurves };

    int ccNumber = -1;

    int rawMinimum = 0;
    int rawMaximum = 127;

    /** live-performance 8: 3% at the heel, 5% at the toe, by default. */
    double heelDeadZone = 0.03;
    double toeDeadZone = 0.05;

    Curve curve = Curve::linear;

    bool isCalibrated() const noexcept { return rawMaximum > rawMinimum; }

    /** Maps a raw CC value to 0..1 through the calibration. */
    double map (int rawValue) const noexcept;

    juce::var toVar() const;
    static ExpressionCalibration fromVar (const juce::var& state);
};

const char* getExpressionCurveName (ExpressionCalibration::Curve curve) noexcept;

//==============================================================================
/** Every pedal calibration the user has made.

    live-performance 11: these are user-global, not per-preset. A pedal that has
    been calibrated once stays calibrated across every preset the user opens,
    which is the only behaviour that makes sense for a piece of hardware bolted
    to the floor. */
class ExpressionCalibrationSet
{
public:
    ExpressionCalibrationSet();

    /** The calibration for a CC, or a default one if it has never been set. */
    ExpressionCalibration get (int ccNumber) const;

    bool has (int ccNumber) const;

    void set (const ExpressionCalibration& calibration);
    void remove (int ccNumber);
    void clear();

    /** Maps a raw CC through its calibration, or straight through 0..127 if that
        CC has never been calibrated. */
    double map (int ccNumber, int rawValue) const;

    juce::Array<int> getCalibratedCcNumbers() const;

    //==========================================================================
    /** The running state of the calibration wizard (live-performance 8). */
    enum class WizardStage { idle = 0, heel, toe, done };

    void beginCalibration (int ccNumber) noexcept;
    void cancelCalibration() noexcept;

    /** Feeds the wizard a CC value. Returns the stage it is now in. */
    WizardStage observe (int ccNumber, int rawValue) noexcept;

    /** Moves the wizard on from heel to toe, or from toe to done, committing
        what it has seen. Returns the new stage. */
    WizardStage confirmStage() noexcept;

    WizardStage getWizardStage() const noexcept { return stage; }
    int getWizardCc() const noexcept { return wizardCc; }

    //==========================================================================
    bool load();
    bool save() const;

    static juce::File getConfigFile();

    juce::var toVar() const;
    void fromVar (const juce::var& state);

private:
    std::array<ExpressionCalibration, 128> calibrations {};
    std::array<bool, 128> present {};

    // --- wizard --------------------------------------------------------------------
    WizardStage stage = WizardStage::idle;
    int wizardCc = -1;
    int observedMin = 127;
    int observedMax = 0;
    int heelValue = 0;

    JUCE_LEAK_DETECTOR (ExpressionCalibrationSet)
};

} // namespace luthier
