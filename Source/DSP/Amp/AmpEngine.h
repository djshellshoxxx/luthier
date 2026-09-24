#pragma once

/*  Amp Engine (engine spec 11).

    Signal path, all of it oversampled:

        input -> [bright switch] -> [preamp stage] x N -> [tone stack]
              -> [phase inverter] -> [power tubes, push-pull] -> [sag]
              -> [output transformer] -> [presence, in the NFB loop] -> output

    Each preamp stage is: gain, an asymmetric tube transfer curve, a cathode-bypass
    low-mid lift, and an interstage coupling capacitor that removes the DC the
    asymmetry creates. Cascading them is what turns a clean amp into a high-gain one,
    and the order matters: the tone stack sits between the preamp and the power amp
    on every amp modelled here, which is why turning the bass up on a cranked
    Marshall makes it flub rather than simply adding low end.
*/

#include "../Common/DspCommon.h"
#include "../Common/Oversampler.h"
#include "ToneStack.h"

namespace luthier
{

//==============================================================================
enum class AmpModel
{
    FenderTwin,        ///< Blackface clean
    FenderTweed,       ///< 5F6-A Bassman, early breakup
    FenderDeluxe,      ///< Smaller, breaks up sooner
    FenderChamp,       ///< Single-ended, very early breakup
    MarshallPlexi,     ///< 1959 Super Lead
    MarshallJCM800,    ///< Hot-rodded, tighter
    VoxAC30,           ///< Top Boost, EL84 chime
    MesaRectifier,     ///< Modern high gain
    BognerEcstasy,     ///< High gain with clarity
    DiezelVH4,         ///< Modern metal
    OrangeOR120,       ///< British, thick
    AmpegSVT,          ///< Bass
    AcousticDI,        ///< Clean acoustic preamp, no tube colour
    Custom,
    NumModels
};

enum class PowerTube
{
    EL84,     ///< Vox: early, chimey compression
    EL34,     ///< Marshall: aggressive midrange
    Tube6L6,  ///< Fender: clean headroom, firm low end
    KT88,     ///< Big, clean, tight
    Tube6V6,  ///< Small Fenders: soft and warm
    NumTubes
};

//==============================================================================
struct AmpVoicing
{
    const char* name;
    int preampStages;            ///< How many 12AX7 gain stages.
    double inputGain;            ///< Trim into the first stage.
    double stageBias;            ///< Asymmetry of the transfer curve.
    double stageGainScale;       ///< Gain multiplier per cascaded stage.
    PowerTube powerTube;
    int toneStackStyle;          ///< 0 Fender, 1 Marshall, 2 Vox, 3 Modern.
    double negativeFeedback;     ///< 0 = none (Vox), 1 = plenty (Fender).
    double sagAmount;
    double brightCapGain;        ///< How much the bright switch adds.
    double outputTransformerHz;  ///< HF corner of the transformer.
    double lowCutHz;             ///< Coupling cap corner: tightness.
};

//==============================================================================
class AmpEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    //==========================================================================
    void setModel (AmpModel m) noexcept;
    AmpModel getModel() const noexcept { return model; }

    static const AmpVoicing& getVoicing (AmpModel m) noexcept;
    static const char* getModelName (AmpModel m) noexcept;
    static const char* getPowerTubeName (PowerTube t) noexcept;

    //==========================================================================
    void setGain (double normalised) noexcept;        ///< Preamp drive, 0-1.
    void setMaster (double normalised) noexcept;      ///< Power amp drive, 0-1.
    void setBass (double normalised) noexcept;
    void setMid (double normalised) noexcept;
    void setTreble (double normalised) noexcept;
    void setPresence (double normalised) noexcept;
    void setBrightSwitch (bool on) noexcept;
    void setMidBoost (bool on) noexcept;
    void setStandby (bool on) noexcept;

    double getGain() const noexcept { return gainNorm; }
    double getMaster() const noexcept { return masterNorm; }
    bool isStandby() const noexcept { return standby; }

    /** True while the amp is still warming up out of standby. */
    bool isWarmingUp() const noexcept { return warmupGain.isSmoothing(); }
    double getWarmupProgress() const noexcept { return warmupGain.getCurrent(); }

    //==========================================================================
    /** Custom-model controls. Only used when the model is AmpModel::Custom. */
    void setCustomStages (int stages) noexcept;
    void setCustomPowerTube (PowerTube t) noexcept;
    void setCustomToneStackStyle (int style) noexcept;

    void setOversamplingFactor (int factor) noexcept;
    int getLatencySamples() const noexcept { return oversampler.getLatencySamples(); }
    double getOversampledRate() const noexcept { return oversampler.getOversampledRate(); }   // performance-budget.md 7

    //==========================================================================
    /** Mono in, mono out. The amp is a mono device; stereo appears later, at the
        cabinet. */
    double processSample (double x) noexcept;
    void processMono (double* samples, int numSamples) noexcept;

    /** Power-supply sag, 0 (none) to 1 (fully collapsed). For the UI. */
    double getSagAmount() const noexcept { return 1.0 - supplyVoltage; }

private:
    static constexpr int kMaxStages = 5;

    void updateVoicing() noexcept;
    void updateFilters() noexcept;
    void updateBeyondStock() noexcept;

    inline double preampStage (double x, int stageIndex) noexcept;
    inline double powerAmpStage (double x) noexcept;

    double sr = 44100.0;
    double osRate = 176400.0;

    AmpModel model = AmpModel::FenderTwin;
    AmpVoicing voicing {};

    int customStages = 3;
    PowerTube customPowerTube = PowerTube::Tube6L6;
    int customToneStackStyle = 0;

    double gainNorm = 0.35, masterNorm = 0.7;
    double bassNorm = 0.5, midNorm = 0.5, trebleNorm = 0.5, presenceNorm = 0.4;
    bool brightSwitch = false, midBoost = false, standby = false;

    Oversampler oversampler;
    ToneStack toneStack;

    // Preamp
    Biquad brightShelf;
    Biquad stageEq[kMaxStages];
    OnePoleHP stageCoupling[kMaxStages];
    OnePoleLP stageSmoothing[kMaxStages];
    Biquad midBoostEq;

    /*  advanced-ranges.md 3.1: bass, mid and treble past the ends of the
        knob. The tone stack is a passive network and a pot position outside
        0-1 is a negative resistance, so it stays on the knob's travel and the
        part beyond is extra shelving after it - the "exaggerated" region is
        honest about not being the circuit any more. */
    Biquad bassBeyond, midBeyond, trebleBeyond;

    // Power amp
    OnePoleHP piCoupling;
    Biquad presenceShelf;
    OnePoleLP transformerHf;
    OnePoleHP transformerLf;
    EnvelopeFollower sagFollower;
    double stageRest = 0.0;   ///< tubeShape (0, bias): subtracted so a cold start is silent (qa-polish.md 5.10)
    double supplyVoltage = 1.0;
    double sagAttack = 0.0, sagRelease = 0.0;
    double lastOutput = 0.0;

    DCBlocker outputDc;
    ExpSmoother gainSmooth, masterSmooth, warmupGain;

    JUCE_LEAK_DETECTOR (AmpEngine)
};

} // namespace luthier
