#include "AmpEngine.h"
#include "../../Support/QualityProfile.h"

namespace luthier
{

namespace
{
    // name, stages, inputGain, bias, gainScale, powerTube, toneStyle, NFB, sag,
    // brightCap, transformerHz, lowCutHz
    const AmpVoicing kVoicings[(size_t) AmpModel::NumModels] =
    {
        { "American Twin",      2, 0.85, 0.15, 1.35, PowerTube::Tube6L6, 0, 0.85, 0.10, 6.0,  9500.0, 28.0 },
        { "Tweed Combo",     3, 1.10, 0.30, 1.55, PowerTube::Tube6L6, 0, 0.35, 0.45, 3.0,  7200.0, 34.0 },
        { "Blackface Combo 22",    2, 1.05, 0.26, 1.50, PowerTube::Tube6V6, 0, 0.55, 0.40, 5.0,  7800.0, 32.0 },
        { "Small Tweed",     2, 1.25, 0.34, 1.60, PowerTube::Tube6V6, 0, 0.15, 0.60, 2.0,  6200.0, 45.0 },
        { "British Plexi",   3, 1.15, 0.28, 1.70, PowerTube::EL34,    1, 0.55, 0.35, 7.0,  8200.0, 38.0 },
        { "British 800",  4, 1.40, 0.24, 1.85, PowerTube::EL34,    1, 0.65, 0.22, 4.0,  8800.0, 52.0 },
        { "British Top-Boost 30",         2, 1.00, 0.32, 1.45, PowerTube::EL84,    2, 0.05, 0.55, 5.0, 10500.0, 30.0 },
        { "California Rectified",   5, 1.60, 0.18, 1.95, PowerTube::Tube6L6, 3, 0.45, 0.30, 2.0,  9000.0, 72.0 },
        { "Boutique Lead",   4, 1.45, 0.22, 1.82, PowerTube::EL34,    1, 0.60, 0.25, 3.5,  9200.0, 58.0 },
        { "German Four-Channel",       5, 1.55, 0.16, 1.92, PowerTube::EL34,    3, 0.70, 0.18, 2.0,  9600.0, 80.0 },
        { "British Crunch 120",     3, 1.20, 0.34, 1.68, PowerTube::EL34,    1, 0.40, 0.38, 3.0,  7000.0, 26.0 },
        { "Classic Bass 300",        3, 0.95, 0.20, 1.40, PowerTube::KT88,    0, 0.75, 0.20, 2.0,  6500.0, 18.0 },
        { "Acoustic DI",      1, 0.70, 0.02, 1.05, PowerTube::KT88,    0, 1.00, 0.00, 1.0, 16000.0, 20.0 },
        { "Custom",           3, 1.10, 0.25, 1.60, PowerTube::EL34,    1, 0.50, 0.30, 4.0,  8500.0, 40.0 }
    };

    /** Each power tube type has its own compression curve and harmonic signature. */
    struct TubeCharacter { double knee; double asymmetry; double compression; };

    const TubeCharacter kTubeCharacters[(size_t) PowerTube::NumTubes] =
    {
        { 1.05, 0.22, 0.85 },   // EL84: breaks up early and hard
        { 1.35, 0.16, 0.62 },   // EL34: aggressive mids
        { 1.75, 0.09, 0.40 },   // 6L6: clean headroom
        { 2.20, 0.06, 0.26 },   // KT88: big and tight
        { 1.20, 0.19, 0.74 }    // 6V6: soft and warm
    };
}

//==============================================================================
const AmpVoicing& AmpEngine::getVoicing (AmpModel m) noexcept
{
    return kVoicings[(size_t) juce::jlimit (0, (int) AmpModel::NumModels - 1, (int) m)];
}

const char* AmpEngine::getModelName (AmpModel m) noexcept
{
    return getVoicing (m).name;
}

const char* AmpEngine::getPowerTubeName (PowerTube t) noexcept
{
    switch (t)
    {
        case PowerTube::EL84:    return "EL84";
        case PowerTube::EL34:    return "EL34";
        case PowerTube::Tube6L6: return "6L6";
        case PowerTube::KT88:    return "KT88";
        case PowerTube::Tube6V6: return "6V6";
        case PowerTube::NumTubes:
        default:                 return "EL34";
    }
}

//==============================================================================
void AmpEngine::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sr = sampleRate;

    oversampler.prepare (sr, 4);
    osRate = oversampler.getOversampledRate();

    toneStack.prepare (osRate);

    for (int i = 0; i < kMaxStages; ++i)
    {
        stageCoupling[i].prepare (osRate);
        stageSmoothing[i].prepare (osRate);
    }

    piCoupling.prepare (osRate);
    transformerHf.prepare (osRate);
    transformerLf.prepare (osRate);

    sagFollower.prepare (osRate);

    // Sag is slow: the supply droops over tens of milliseconds and recovers over
    // a few hundred. That time constant is what makes an amp "breathe".
    sagAttack = std::exp (-1.0 / (0.020 * osRate));
    sagRelease = std::exp (-1.0 / (0.350 * osRate));
    sagFollower.setTimes (0.008, 0.220);

    outputDc.prepare (sr, 8.0);

    gainSmooth.prepare (sr, constants::kParamSmoothSeconds);
    masterSmooth.prepare (sr, constants::kParamSmoothSeconds);

    // A real amp takes about 30 seconds to come out of standby.
    warmupGain.prepare (sr, 8.0);
    warmupGain.snapTo (standby ? 0.0 : 1.0);

    updateVoicing();

    // cpu-quality-modes 2.2: the pad and the crossfade's old path, made here
    // on the message thread so a factor change never allocates.
    nominalFactor = 4;
    latencyPad.setLength (0);

    if (twin.engine == nullptr)
        twin.engine = std::make_shared<AmpEngine>();

    reset();
}

void AmpEngine::reset() noexcept
{
    latencyPad.reset();
    history.reset();
    fadeLeft = 0;
    oversampler.reset();
    toneStack.reset();
    brightShelf.reset();
    midBoostEq.reset();
    bassBeyond.reset();
    midBeyond.reset();
    trebleBeyond.reset();

    for (int i = 0; i < kMaxStages; ++i)
    {
        stageEq[i].reset();
        stageCoupling[i].reset();
        stageSmoothing[i].reset();
    }

    piCoupling.reset();
    presenceShelf.reset();
    transformerHf.reset();
    transformerLf.reset();
    sagFollower.reset();
    outputDc.reset();

    supplyVoltage = 1.0;
    lastOutput = 0.0;

    gainSmooth.snapTo (gainNorm);
    masterSmooth.snapTo (masterNorm);
    warmupGain.snapToTarget();
}

//==============================================================================
void AmpEngine::setModel (AmpModel m) noexcept
{
    if (m == model)
        return;

    model = m;
    updateVoicing();
}

void AmpEngine::updateVoicing() noexcept
{
    voicing = getVoicing (model);
    stageRest = tubeShape (0.0, voicing.stageBias);   // qa-polish.md 5.10

    if (model == AmpModel::Custom)
    {
        voicing.preampStages = juce::jlimit (1, kMaxStages, customStages);
        voicing.powerTube = customPowerTube;
        voicing.toneStackStyle = juce::jlimit (0, 3, customToneStackStyle);
    }

    voicing.preampStages = juce::jlimit (1, kMaxStages, voicing.preampStages);

    switch (voicing.toneStackStyle)
    {
        case 1:  toneStack.setComponents (ToneStackComponents::marshall()); break;
        case 2:  toneStack.setComponents (ToneStackComponents::vox());      break;
        case 3:  toneStack.setComponents (ToneStackComponents::modern());   break;
        case 0:
        default: toneStack.setComponents (ToneStackComponents::fender());   break;
    }

    updateFilters();
}

void AmpEngine::updateFilters() noexcept
{
    toneStack.setControls (bassNorm, midNorm, trebleNorm);
    updateBeyondStock();

    // Bright switch: a cap across the volume pot. Its effect is strongest at low
    // gain settings and disappears as the pot is opened, exactly as in the circuit.
    const double brightDb = brightSwitch
                              ? voicing.brightCapGain * (1.0 - gainNorm * 0.85)
                              : 0.0;
    brightShelf.setHighShelf (osRate, 2200.0, 0.7, brightDb);

    midBoostEq.setPeaking (osRate, 650.0, 0.9, midBoost ? 6.0 : 0.0);

    for (int i = 0; i < kMaxStages; ++i)
    {
        // Each cascaded stage is progressively tighter in the bass: that is what
        // keeps a five-stage amp from turning into mud.
        const double cut = voicing.lowCutHz * (1.0 + 0.55 * (double) i);
        stageCoupling[i].setCutoff (juce::jmin (cut, osRate * 0.45));

        // The Miller capacitance of each triode rolls off the top a little.
        stageSmoothing[i].setCutoff (juce::jmin (11000.0 - 700.0 * (double) i, osRate * 0.45));

        // A small presence dip between stages keeps the cascade from getting harsh.
        stageEq[i].setPeaking (osRate, 2600.0 + 400.0 * (double) i, 1.1, -1.4);
    }

    piCoupling.setCutoff (juce::jmin (12.0, osRate * 0.45));

    // Presence sits in the negative-feedback loop: it works by *removing* feedback
    // at high frequencies, so its effect grows with how much NFB the amp has. A Vox
    // has almost none, which is why it has no presence control worth speaking of.
    const double presenceDb = presenceNorm * 12.0 * juce::jmax (0.15, voicing.negativeFeedback);
    presenceShelf.setHighShelf (osRate, 3800.0, 0.6, presenceDb);

    transformerHf.setCutoff (juce::jmin (voicing.outputTransformerHz, osRate * 0.45));
    transformerLf.setCutoff (juce::jmin (22.0, osRate * 0.45));
}

void AmpEngine::updateBeyondStock() noexcept
{
    // 18 dB per unit of travel past the end: the advanced limits (-0.5, 1.5)
    // are 9 dB of extra cut or boost, on top of the stack's own extreme.
    auto beyondDb = [] (double v) { return 18.0 * (v > 1.0 ? v - 1.0 : (v < 0.0 ? v : 0.0)); };

    bassBeyond.setLowShelf (osRate, 120.0, 0.7, beyondDb (bassNorm));
    midBeyond.setPeaking (osRate, 650.0, 0.8, beyondDb (midNorm));
    trebleBeyond.setHighShelf (osRate, 3200.0, 0.7, beyondDb (trebleNorm));
}

//==============================================================================
//  Setters take the advanced ranges (advanced-ranges.md 3.1); the stock range
//  is 0-1 and the knob's own travel. Clamped to the advanced limits so a
//  malformed value still cannot reach the DSP.
void AmpEngine::setGain (double n) noexcept      { gainNorm = juce::jlimit (0.0, 2.0, n); gainSmooth.setTarget (gainNorm); updateFilters(); }
void AmpEngine::setMaster (double n) noexcept    { masterNorm = juce::jlimit (0.0, 2.0, n); masterSmooth.setTarget (masterNorm); }
void AmpEngine::setBass (double n) noexcept      { bassNorm = juce::jlimit (-0.5, 1.5, n); toneStack.setControls (bassNorm, midNorm, trebleNorm); updateBeyondStock(); }
void AmpEngine::setMid (double n) noexcept       { midNorm = juce::jlimit (-0.5, 1.5, n); toneStack.setControls (bassNorm, midNorm, trebleNorm); updateBeyondStock(); }
void AmpEngine::setTreble (double n) noexcept    { trebleNorm = juce::jlimit (-0.5, 1.5, n); toneStack.setControls (bassNorm, midNorm, trebleNorm); updateBeyondStock(); }
void AmpEngine::setPresence (double n) noexcept  { presenceNorm = juce::jlimit (0.0, 2.0, n); updateFilters(); }
void AmpEngine::setBrightSwitch (bool on) noexcept { brightSwitch = on; updateFilters(); }
void AmpEngine::setMidBoost (bool on) noexcept   { midBoost = on; updateFilters(); }

void AmpEngine::setStandby (bool on) noexcept
{
    standby = on;
    warmupGain.setTarget (on ? 0.0 : 1.0);
}

void AmpEngine::setCustomStages (int stages) noexcept
{
    customStages = juce::jlimit (1, kMaxStages, stages);

    if (model == AmpModel::Custom)
        updateVoicing();
}

void AmpEngine::setCustomPowerTube (PowerTube t) noexcept
{
    customPowerTube = t;

    if (model == AmpModel::Custom)
        updateVoicing();
}

void AmpEngine::setCustomToneStackStyle (int style) noexcept
{
    customToneStackStyle = juce::jlimit (0, 3, style);

    if (model == AmpModel::Custom)
        updateVoicing();
}

void AmpEngine::retuneForFactor (int factor) noexcept
{
    // cpu-quality-modes 2.2: the oversampled-rate filters keep their state and
    // only their coefficients move to the new rate.
    oversampler.setFactor (factor);
    const double newRate = oversampler.getOversampledRate();

    if (std::abs (newRate - osRate) < 1.0)
        return;

    osRate = newRate;

    toneStack.setSampleRateKeepingState (osRate);

    for (int i = 0; i < kMaxStages; ++i)
    {
        stageCoupling[i].setSampleRateKeepingState (osRate);
        stageSmoothing[i].setSampleRateKeepingState (osRate);
    }

    piCoupling.setSampleRateKeepingState (osRate);
    transformerHf.setSampleRateKeepingState (osRate);
    transformerLf.setSampleRateKeepingState (osRate);
    sagFollower.setSampleRateKeepingState (osRate);

    sagAttack = std::exp (-1.0 / (0.020 * osRate));
    sagRelease = std::exp (-1.0 / (0.350 * osRate));
    sagFollower.setTimes (0.008, 0.220);

    updateVoicing();
}

void AmpEngine::setOversamplingFactor (int effective, int nominal, bool crossfade) noexcept
{
    auto clampFactor = [] (int f) { return f >= 8 ? 8 : f >= 4 ? 4 : f >= 2 ? 2 : 1; };
    nominal = clampFactor (nominal);
    effective = juce::jmin (clampFactor (effective), nominal);
    nominalFactor = nominal;

    const int padLength = Oversampler::latencyFor (nominal) - Oversampler::latencyFor (effective);

    if (effective == oversampler.getFactor())
    {
        latencyPad.setLength (padLength);
        return;
    }

    if (! crossfade || twin.engine == nullptr)
    {
        // A hard switch: exactly what a factor change always did.
        fadeLeft = 0;
        oversampler.setFactor (effective);
        const double newRate = oversampler.getOversampledRate();

        if (std::abs (newRate - osRate) >= 1.0)
        {
            osRate = newRate;
            toneStack.prepare (osRate);

            for (int i = 0; i < kMaxStages; ++i)
            {
                stageCoupling[i].prepare (osRate);
                stageSmoothing[i].prepare (osRate);
            }

            piCoupling.prepare (osRate);
            transformerHf.prepare (osRate);
            transformerLf.prepare (osRate);
            sagFollower.prepare (osRate);

            sagAttack = std::exp (-1.0 / (0.020 * osRate));
            sagRelease = std::exp (-1.0 / (0.350 * osRate));
            sagFollower.setTimes (0.008, 0.220);

            updateVoicing();
        }

        latencyPad.setLength (padLength);
        return;
    }

    // The old path carries on in the twin, exactly as it was (no allocation:
    // the twin was made in prepare and this is a plain copy).
    *twin.engine = *this;
    twin.engine->fadeLeft = 0;

    // The new path: same state, new rate, primed from the recent input so the
    // half-band filters are not starting from silence.
    retuneForFactor (effective);
    oversampler.reset();

    for (int i = 0; i < InputHistory::kSize; ++i)
    {
        double work[Oversampler::kMaxFactor];
        oversampler.up (history.get (i), work);
        oversampler.down (work);
    }

    latencyPad.setLength (padLength);

    // A pad whose length changed restarts empty; the crossfade starts on the
    // old path, so those few samples are not heard.
    fadeTotal = juce::jmax (1, (int) std::round (QualityProfile::kOversamplerFadeSeconds * sr))
                  + QualityProfile::kSwitchSettleSamples;
    fadeLeft = fadeTotal;
}

//==============================================================================
inline double AmpEngine::preampStage (double x, int stageIndex) noexcept
{
    // Asymmetric transfer curve: a triode clips the two halves of the waveform
    // differently, which is what generates the even harmonics that make tube
    // distortion sound warm rather than buzzy.
    // qa-polish.md 5.10: less the curve's resting point. The coupling cap below
    // removes that constant anyway once it has settled, so the steady-state
    // sound is unchanged; without the subtraction a cold start stepped from 0
    // to the bias point and the cascade amplified the step into a thump.
    double y = tubeShape (x, voicing.stageBias) - stageRest;

    // Cathode bypass cap: a low-mid lift on each stage.
    y = stageEq[stageIndex].process (y);

    // Miller capacitance.
    y = stageSmoothing[stageIndex].process (y);

    // Interstage coupling cap: removes the DC the asymmetry just created, so the
    // next stage's bias point does not walk away.
    y = stageCoupling[stageIndex].process (y);

    return y;
}

inline double AmpEngine::powerAmpStage (double x) noexcept
{
    const auto& tube = kTubeCharacters[(size_t) juce::jlimit (0, (int) PowerTube::NumTubes - 1,
                                                              (int) voicing.powerTube)];

    // ---- phase inverter ------------------------------------------------------
    // A real long-tailed-pair inverter is not perfectly balanced. The mismatch is
    // small but it is what stops the push-pull pair from cancelling every even
    // harmonic, so it is modelled rather than assumed away.
    const double imbalance = 0.04;
    const double positive = x * (1.0 + imbalance);
    const double negative = -x * (1.0 - imbalance);

    // ---- sag ------------------------------------------------------------------
    // Under load the supply droops, which lowers the headroom and compresses the
    // dynamics. The droop follows the envelope of the signal, not the sample.
    const double demand = sagFollower.process (x);
    const double sagTarget = 1.0 - juce::jlimit (0.0, 0.55, demand * voicing.sagAmount * 0.9);
    const double coeff = (sagTarget < supplyVoltage) ? sagAttack : sagRelease;
    supplyVoltage = sagTarget + (supplyVoltage - sagTarget) * coeff;
    supplyVoltage = juce::jlimit (0.35, 1.0, supplyVoltage);

    const double headroom = tube.knee * supplyVoltage;

    // ---- the two tubes --------------------------------------------------------
    auto tubeCurve = [headroom, &tube] (double v) noexcept
    {
        const double biased = v + tube.asymmetry;
        return std::tanh (biased / juce::jmax (0.05, headroom)) * headroom - tube.asymmetry;
    };

    const double outPositive = tubeCurve (positive);
    const double outNegative = tubeCurve (negative);

    // Push-pull: the two halves are recombined by the transformer.
    double y = (outPositive - outNegative) * 0.5;

    // Class-AB crossover: at very low levels the two tubes hand over, and cheap
    // amps show a little notch there. Modelled small, because a well-biased amp
    // barely shows it.
    const double crossover = 0.004 * tube.compression;

    if (std::abs (y) < crossover)
        y *= 0.72;

    return y;
}

//==============================================================================
double AmpEngine::processSample (double x) noexcept
{
    history.push (x);

    if (fadeLeft > 0)
    {
        // cpu-quality-modes 2.2: both paths for 10 ms under a linear crossfade.
        const double oldOut = twin.engine->latencyPad.process (twin.engine->processCore (x));
        const double newOut = latencyPad.process (processCore (x));
        const double t = juce::jlimit (0.0, 1.0, 1.0 - (double) fadeLeft / (double) (fadeTotal - QualityProfile::kSwitchSettleSamples));
        --fadeLeft;
        return sanitise (oldOut * (1.0 - t) + newOut * t);
    }

    return latencyPad.process (processCore (x));
}

double AmpEngine::processCore (double x) noexcept
{
    const double warm = warmupGain.next();

    if (warm <= 1.0e-5)
        return 0.0;

    const double gain = gainSmooth.next();
    const double master = masterSmooth.next();

    // Preamp drive spans about 45 dB. Below about a third of the knob most amps
    // here are clean, which matches where the useful range sits on the real thing.
    // Past the knob's end (advanced ranges) the gain keeps climbing, more
    // gently: another 24 dB of drive and 12 dB of master at the limit.
    // dbToGain() is a std::pow; the drive/master gains are parked most of the
    // time, so the conversion is cached and only redone when its smoothed input
    // actually moves. The gain-to-dB map and the pow are a pure function of
    // `gain` / `master`, so reusing the cached result while the input is
    // unchanged is bit-exact (a parked ExpSmoother returns its target verbatim).
    if (gain != cachedGainInput)
    {
        cachedGainInput = gain;
        const double preGainDb = gain <= 1.0 ? juce::jmap (gain, 0.0, 1.0, -6.0, 40.0) : 40.0 + (gain - 1.0) * 24.0;
        cachedPreGainPow = dbToGain (preGainDb);
    }

    if (master != cachedMasterInput)
    {
        cachedMasterInput = master;
        const double postGainDb = master <= 1.0 ? juce::jmap (master, 0.0, 1.0, -40.0, 8.0) : 8.0 + (master - 1.0) * 12.0;
        cachedPostGain = dbToGain (postGainDb);
    }

    // voicing.inputGain stays out of the cache: it is one cheap multiply, and
    // leaving it here means a voicing change takes effect at once, with no key.
    const double preGain = voicing.inputGain * cachedPreGainPow;
    const double postGain = cachedPostGain;

    const int stages = voicing.preampStages;

    double out = oversampler.processSample (x, [this, preGain, postGain, stages] (double v) noexcept
    {
        // ---- bright switch and mid boost, ahead of the gain ------------------
        v = brightShelf.process (v);
        v = midBoostEq.process (v);

        // ---- preamp cascade ---------------------------------------------------
        v *= preGain;

        for (int s = 0; s < stages; ++s)
        {
            v = preampStage (v, s);

            // Each subsequent stage sees a bit more gain than the last.
            if (s + 1 < stages)
                v *= voicing.stageGainScale;
        }

        // ---- tone stack -------------------------------------------------------
        v = toneStack.process (v);
        v = bassBeyond.process (v);
        v = midBeyond.process (v);
        v = trebleBeyond.process (v);

        // ---- master volume, then the power amp -------------------------------
        v = piCoupling.process (v);
        v *= postGain;

        v = powerAmpStage (v);

        // ---- presence, inside the feedback loop ------------------------------
        v = presenceShelf.process (v);

        // ---- output transformer ------------------------------------------------
        // Slight low-frequency saturation from core flux, plus the bandwidth
        // limits of the iron at both ends.
        v = transformerLf.process (v);
        v = softClip (v * 0.85) * 1.18;
        v = transformerHf.process (v);

        return sanitise (v);
    });

    out = outputDc.process (out * warm);

    lastOutput = out;
    return sanitise (out);
}

void AmpEngine::processMono (double* samples, int numSamples) noexcept
{
    if (samples == nullptr)
        return;

    for (int i = 0; i < numSamples; ++i)
        samples[i] = processSample (samples[i]);
}

} // namespace luthier
