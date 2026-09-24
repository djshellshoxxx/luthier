#include "BodyEngine.h"
#include "../../Support/ThreadProbe.h"

namespace luthier
{

namespace
{
    /** Partition size for the body convolution. 128 keeps the reported latency
        low enough that playing feels immediate (engine spec 6.1). */
    constexpr int kBodyPartitionSize = 128;
}

//==============================================================================
BodyEngine::BodyEngine()
{
    convolution = std::make_unique<juce::dsp::Convolution> (
        juce::dsp::Convolution::Latency { kBodyPartitionSize });
}

BodyEngine::~BodyEngine() = default;

//==============================================================================
void BodyEngine::prepare (double sampleRate, int maxBlockSize)
{
    loadedIrFile = juce::File();

    sr = sampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = (juce::uint32) maxBlock;
    spec.numChannels = 2;

    convolution->prepare (spec);
    prepared = true;

    for (auto& r : resonators)
        r.prepare (sr);

    airShelf.reset();
    dcLeft.prepare (sr, 8.0);
    dcRight.prepare (sr, 8.0);

    amountSmooth.prepare (sr, constants::kParamSmoothSeconds);
    gainSmooth.prepare (sr, constants::kParamSmoothSeconds);
    amountSmooth.snapTo (1.0);
    gainSmooth.snapTo (1.0);

    wetBuffer.setSize (2, maxBlock, false, true, true);

    // cpu-quality-modes 2.3: the Medium and Low variants, same partition latency.
    irVariants.prepare (sr, maxBlock, 2, kBodyPartitionSize,
                        { QualityProfile::forLevel (QualityLevel::Medium).bodyIrSeconds,
                          QualityProfile::forLevel (QualityLevel::Low).bodyIrSeconds },
                        QualityProfile::kBodyVariantBudgetMegabytes);
    modeRampTotal = juce::jmax (1, (int) std::round (QualityProfile::kDroppedVoiceRampSeconds * sr));

    setAirResonanceGainDb (airGainDb);
    rebuildModalBank();
    applyStagedBank();
    reset();
}

void BodyEngine::reset() noexcept
{
    // A try-lock, so this is safe to call from anywhere. If a response is being
    // swapped in right now there is nothing to clear: the swap resets the
    // convolution itself once it completes.
    {
        const juce::SpinLock::ScopedTryLockType lock (convolutionLock);

        if (lock.isLocked())
        {
            convolution->reset();
            irVariants.reset();
        }
    }

    // A bank staged since the last block goes in now, not part-way into the
    // next render (reset runs with the audio stopped).
    applyStagedBank();

    for (auto& r : resonators)
        r.reset();

    airShelf.reset();
    dcLeft.reset();
    dcRight.reset();
    wetBuffer.clear();

    amountSmooth.snapToTarget();
    gainSmooth.snapToTarget();
}

//==============================================================================
void BodyEngine::setMode (Mode m) noexcept
{
    if (m == mode)
        return;

    mode = m;

    for (auto& r : resonators)
        r.reset();
}

void BodyEngine::setBodyConfig (const BodyConfig& cfg)
{
    config = cfg;
    rebuildModalBank();
}

void BodyEngine::setAmount (double amount) noexcept
{
    amountSmooth.setTarget (juce::jlimit (0.0, 1.0, amount));
}

void BodyEngine::setAirResonanceGainDb (double db) noexcept
{
    airGainDb = juce::jlimit (-12.0, 12.0, db);

    const double airHz = BodyModels::computeAirResonance (config);
    airShelf.setPeaking (sr, airHz > 0.0 ? airHz : 110.0, 1.1, airGainDb);
}

double BodyEngine::getAirResonanceHz() const noexcept
{
    const double airHz = BodyModels::computeAirResonance (config);

    return airHz > 0.0 ? airHz : 110.0;
}

void BodyEngine::setOutputGainDb (double db) noexcept
{
    outputGainDb = juce::jlimit (-24.0, 24.0, db);
    gainSmooth.setTarget (dbToGain (outputGainDb));
}

//==============================================================================
void BodyEngine::rebuildModalBank()
{
    const juce::ScopedLock sl (rebuildLock);

    BodyModels::buildModes (config, buildScratch);

    stagedCount = juce::jmin ((int) buildScratch.size(), BodyModels::kMaxModes);

    for (int i = 0; i < stagedCount; ++i)
        stagedModes[(size_t) i] = buildScratch[(size_t) i];

    // cpu-quality-modes 2.1: the modal cap's order - the eight lowest modes,
    // then the rest by the energy each passes of a plucked string's signal:
    // gain squared times bandwidth (f / Q), times the string's spectrum,
    // which falls at least 6 dB an octave (1 / f squared in power).
    {
        std::array<int, BodyModels::kMaxModes> order {};

        for (int i = 0; i < stagedCount; ++i)
            order[(size_t) i] = i;

        std::sort (order.begin(), order.begin() + stagedCount, [this] (int a, int b)
        {
            return stagedModes[(size_t) a].frequencyHz < stagedModes[(size_t) b].frequencyHz;
        });

        const int kept = juce::jmin (stagedCount, QualityProfile().bodyModesAlwaysKept);

        auto energy = [this] (int i)
        {
            const auto& m = stagedModes[(size_t) i];
            return m.gain * m.gain / (juce::jmax (20.0, m.frequencyHz) * juce::jmax (0.5, m.q));
        };

        std::stable_sort (order.begin() + kept, order.begin() + stagedCount,
                          [&energy] (int a, int b) { return energy (a) > energy (b); });

        stagedPriority = order;
    }

    stagedReady.store (true);
}

void BodyEngine::applyStagedBank() noexcept
{
    if (! stagedReady.exchange (false))
        return;

    const juce::ScopedTryLock sl (rebuildLock);

    if (! sl.isLocked())
    {
        // The message thread is mid-rebuild; try again next block rather than
        // blocking the audio thread.
        stagedReady.store (true);
        return;
    }

    numActiveModes = stagedCount;

    for (int i = 0; i < numActiveModes; ++i)
    {
        activeModes[(size_t) i] = stagedModes[(size_t) i];
        resonators[(size_t) i].set (activeModes[(size_t) i].frequencyHz,
                                    activeModes[(size_t) i].q,
                                    activeModes[(size_t) i].gain);
    }

    modePriority = stagedPriority;
    updateModeRun (true);   // a new bank starts at its cap
}

//==============================================================================
void BodyEngine::setQualityLevel (const QualityProfile& profile, bool hard) noexcept
{
    irVariants.setLevel ((int) profile.level, hard);
    modeCap = juce::jlimit (0, BodyModels::kMaxModes, profile.bodyModes);
    updateModeRun (hard);
}

void BodyEngine::updateModeRun (bool hard) noexcept
{
    const int target = juce::jmin (modeCap, numActiveModes);

    if (hard)
    {
        // Modes joining start from rest.
        for (int k = juce::jmin (modeRunCount, numActiveModes); k < target; ++k)
            resonators[(size_t) modePriority[(size_t) k]].reset();

        modeRunCount = modeTarget = target;
        modeRampLeft = 0;
        return;
    }

    if (target < juce::jmin (modeRunCount, numActiveModes))
    {
        // 2.5: dropped modes ramp to 0 over 20 ms before they are skipped.
        modeTarget = target;
        modeRunCount = juce::jmin (modeRunCount, numActiveModes);
        modeRampLeft = modeRampTotal;
    }
    else if (target > modeRunCount || modeRampLeft > 0)
    {
        for (int k = juce::jmax (modeTarget, 0); k < target; ++k)
            if (k >= modeRunCount)
                resonators[(size_t) modePriority[(size_t) k]].reset();

        modeRunCount = modeTarget = target;
        modeRampLeft = 0;
    }
}

inline double BodyEngine::runModes (double in, int sampleInBlock) noexcept
{
    double sum = 0.0;

    if (! modesCapped())
    {
        for (int m = 0; m < numActiveModes; ++m)
            sum += resonators[(size_t) m].process (in);

        return sum;
    }

    const int kept = juce::jmin (modeTarget, numActiveModes);

    for (int k = 0; k < kept; ++k)
        sum += resonators[(size_t) modePriority[(size_t) k]].process (in);

    if (modeRampLeft > 0)
    {
        const double g = juce::jmax (0.0, (double) (modeRampLeft - sampleInBlock) / (double) modeRampTotal);
        double ramped = 0.0;

        for (int k = kept; k < juce::jmin (modeRunCount, numActiveModes); ++k)
            ramped += resonators[(size_t) modePriority[(size_t) k]].process (in);

        sum += ramped * g;
    }

    return sum;
}

void BodyEngine::advanceModeRamp (int numSamples) noexcept
{
    if (modeRampLeft <= 0)
        return;

    modeRampLeft = juce::jmax (0, modeRampLeft - numSamples);

    if (modeRampLeft == 0)
        modeRunCount = modeTarget;
}

//==============================================================================
bool BodyEngine::loadImpulseResponse (const juce::File& file)
{
    if (irLoaded.load() && file == loadedIrFile && file.existsAsFile())
        return true;

    ThreadProbe::noteFileAccess();
    loadedIrFile = juce::File();

    if (! file.existsAsFile())
    {
        irLoaded.store (false);
        return false;
    }

    // Until the response is actually installed the convolution is a unit impulse,
    // which would pass the strings through with no body on them at all. Keep it
    // out of the path until the swap has really happened.
    irLoaded.store (false);

    const juce::SpinLock::ScopedLockType lock (convolutionLock);
    irVariants.clear();

    if (prepared)
        ConvolutionInstaller::installUnitImpulse (*convolution, sr, 2, maxBlock);

    convolution->loadImpulseResponse (file,
                                      juce::dsp::Convolution::Stereo::yes,
                                      juce::dsp::Convolution::Trim::yes,
                                      0,
                                      juce::dsp::Convolution::Normalise::yes);

    if (prepared)
    {
        if (! ConvolutionInstaller::pumpUntilInstalled (*convolution, 2, maxBlock, 1, 4000, (int) (0.06 * sr)))
            return false;

        convolution->reset();
    }

    // cpu-quality-modes 2.3: the shorter responses, under the same lock.
    irVariants.buildFromFile (file, juce::dsp::Convolution::Stereo::yes,
                              juce::dsp::Convolution::Trim::yes, juce::dsp::Convolution::Normalise::yes);

    loadedIrName = file.getFileNameWithoutExtension();
    loadedIrFile = file;
    ++irLoadCount;
    irLoaded.store (true);
    return true;
}

void BodyEngine::loadImpulseResponse (const float* samples, int numSamples, double irSampleRate)
{
    loadedIrFile = juce::File();

    if (samples == nullptr || numSamples <= 0)
    {
        irLoaded.store (false);
        return;
    }

    juce::AudioBuffer<float> ir (1, numSamples);
    ir.copyFrom (0, 0, samples, numSamples);
    const auto rawForVariants = ir;   // cpu-quality-modes 2.3

    irLoaded.store (false);

    const juce::SpinLock::ScopedLockType lock (convolutionLock);
    irVariants.clear();

    if (prepared)
        ConvolutionInstaller::installUnitImpulse (*convolution, sr, 2, maxBlock);

    convolution->loadImpulseResponse (std::move (ir),
                                      irSampleRate,
                                      juce::dsp::Convolution::Stereo::no,
                                      juce::dsp::Convolution::Trim::no,
                                      juce::dsp::Convolution::Normalise::yes);

    if (prepared)
    {
        if (! ConvolutionInstaller::pumpUntilInstalled (*convolution, 2, maxBlock, 1, 4000, (int) (0.06 * sr)))
            return;

        convolution->reset();
        irVariants.buildFromBuffer (rawForVariants, irSampleRate, juce::dsp::Convolution::Stereo::no,
                                    juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
    }

    irLoaded.store (true);
}

int BodyEngine::getLatencySamples() const noexcept
{
    if (mode == Mode::Convolution || mode == Mode::Hybrid)
        return convolution != nullptr ? (int) convolution->getLatency() : 0;

    return 0;
}

//==============================================================================
void BodyEngine::processBlock (juce::dsp::AudioBlock<float>& block) noexcept
{
    applyStagedBank();

    const int numSamples = (int) block.getNumSamples();
    const int numChannels = (int) block.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    if (mode == Mode::Bypassed)
    {
        // Still apply the output trim so toggling the experimental "no body"
        // switch is not also a level jump.
        for (int i = 0; i < numSamples; ++i)
        {
            const double g = gainSmooth.next();

            for (int ch = 0; ch < numChannels; ++ch)
                block.setSample (ch, i, (float) (block.getSample (ch, i) * g));
        }
        return;
    }

    // Keep a dry copy so the "Body" amount can blend.
    wetBuffer.setSize (numChannels, numSamples, false, false, true);

    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
            wetBuffer.setSample (ch, i, block.getSample (ch, i));

    juce::dsp::AudioBlock<float> wet (wetBuffer);
    auto wetSub = wet.getSubBlock (0, (size_t) numSamples);

    // ---- convolution path ---------------------------------------------------
    if (mode == Mode::Convolution || mode == Mode::Hybrid)
    {
        const juce::SpinLock::ScopedTryLockType lock (convolutionLock);

        if (irLoaded.load() && lock.isLocked())
            irVariants.process (*convolution, wetSub);   // the full IR at High (cpu-quality-modes 2.3)
    }

    // ---- modal path ----------------------------------------------------------
    if (mode == Mode::Modal || mode == Mode::Hybrid)
    {
        const double modalMix = (mode == Mode::Hybrid) ? 0.5 : 1.0;
        const double dryPass = (mode == Mode::Hybrid) ? 0.5 : 0.0;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = wetBuffer.getWritePointer (ch);

            for (int i = 0; i < numSamples; ++i)
            {
                const double in = (mode == Mode::Hybrid) ? (double) data[i]
                                                         : (double) block.getSample (ch, i);

                const double sum = runModes (in, i);

                data[i] = (float) sanitise (sum * modalMix + in * dryPass);
            }
        }

        advanceModeRamp (numSamples);
    }

    // ---- air emphasis, DC block, blend --------------------------------------
    for (int i = 0; i < numSamples; ++i)
    {
        const double amount = amountSmooth.next();
        const double gain = gainSmooth.next();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            double w = (double) wetBuffer.getSample (ch, i);

            if (std::abs (airGainDb) > 0.01)
                w = airShelf.process (w);

            w = (ch == 0) ? dcLeft.process (w) : dcRight.process (w);
            w = sanitise (w);

            const double dry = (double) block.getSample (ch, i);
            block.setSample (ch, i, (float) ((dry * (1.0 - amount) + w * amount) * gain));
        }
    }
}

//==============================================================================
void BodyEngine::processMono (double* samples, int numSamples) noexcept
{
    applyStagedBank();

    if (samples == nullptr || numSamples <= 0 || mode == Mode::Bypassed)
        return;

    // The offline/test path uses the modal bank, which needs no FFT partitioning
    // and therefore reports zero latency - convenient for unit tests that compare
    // input and output sample-for-sample.
    for (int i = 0; i < numSamples; ++i)
    {
        const double in = samples[i];
        double sum = runModes (in, i);

        if (std::abs (airGainDb) > 0.01)
            sum = airShelf.process (sum);

        sum = dcLeft.process (sum);

        const double amount = amountSmooth.next();
        const double gain = gainSmooth.next();

        samples[i] = sanitise ((in * (1.0 - amount) + sum * amount) * gain);
    }

    advanceModeRamp (numSamples);
}

} // namespace luthier
