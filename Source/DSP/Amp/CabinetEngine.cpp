#include "CabinetEngine.h"
#include "../../ToneMatch/ToneMatch.h"   // SPEC-SWEEP TM-7
#include "../../Support/ThreadProbe.h"

namespace luthier
{

namespace
{
    constexpr int kCabPartitionSize = 128;
    constexpr double kSpeedOfSoundMmPerSample = 343.0;   // scaled by sample rate below

    struct SpeakerVoice
    {
        const char* name;
        double lowCornerHz;    ///< Below this the cone stops moving air.
        double bodyHz;         ///< The chest-thump resonance.
        double bodyDb;
        double presenceHz;     ///< Cone breakup: the bite.
        double presenceDb;
        double topRollHz;      ///< Above this a guitar speaker is essentially deaf.
    };

    const SpeakerVoice kSpeakers[(size_t) SpeakerType::NumSpeakers] =
    {
        { "British 25 W Vintage",  95.0, 200.0,  3.0, 2100.0,  5.5, 4600.0 },
        { "British 60 W Modern", 88.0, 180.0,  2.5, 2600.0,  7.0, 5200.0 },
        { "British 30 W Heavy",       82.0, 165.0,  3.5, 2300.0,  6.0, 5000.0 },
        { "British 75 W",    92.0, 210.0,  2.0, 3000.0,  4.5, 5400.0 },
        { "American Alnico 12",          105.0, 240.0,  2.2, 1900.0,  5.0, 5600.0 },
        { "British Alnico 15 W",         110.0, 260.0,  1.8, 2400.0,  6.5, 6200.0 },
        { "American 300 W Heavy",              70.0, 150.0,  1.2, 3200.0,  3.0, 5800.0 },
        { "Bass Ceramic",         42.0,  95.0,  2.6, 1400.0,  2.0, 3600.0 }
    };

    struct MicVoice
    {
        const char* name;
        double proximityHz;    ///< Where the proximity bump sits.
        double proximityDb;
        double presenceHz;
        double presenceDb;
        double topHz;
    };

    const MicVoice kMics[(size_t) MicType::NumMics] =
    {
        { "Classic Dynamic",      140.0,  2.0, 5500.0,  5.0, 14000.0 },
        { "Broadcast Dynamic",      120.0,  2.5, 4200.0,  2.5, 15000.0 },
        { "Wide Dynamic",130.0,  3.5, 3800.0,  3.5, 16000.0 },
        { "Large Condenser",      95.0,  1.5, 9000.0,  3.0, 20000.0 },
        { "Ribbon",     110.0,  3.0, 6000.0, -3.5,  9000.0 },
        { "Studio Condenser",         90.0,  1.2, 10000.0, 3.5, 20000.0 },
        { "Kick Dynamic",         70.0,  4.0, 3200.0,  3.0,  9000.0 }
    };

    juce::String slug (const char* s)
    {
        return juce::String (s).toLowerCase()
                               .replaceCharacters (" /-", "___")
                               .removeCharacters (".");
    }
}

//==============================================================================
CabinetEngine::CabinetEngine()
{
    pathA.convolution = std::make_unique<juce::dsp::Convolution> (
        juce::dsp::Convolution::Latency { kCabPartitionSize });
    pathB.convolution = std::make_unique<juce::dsp::Convolution> (
        juce::dsp::Convolution::Latency { kCabPartitionSize });
}

CabinetEngine::~CabinetEngine() = default;

//==============================================================================
void CabinetEngine::MicPath::prepareFallback (double sr, const CabinetConfig& cfg) noexcept
{
    const auto& sp = kSpeakers[(size_t) juce::jlimit (0, (int) SpeakerType::NumSpeakers - 1,
                                                      (int) cfg.speaker)];
    const auto& mic = kMics[(size_t) juce::jlimit (0, (int) MicType::NumMics - 1, (int) cfg.mic)];

    // A broken-in speaker has a looser surround: it goes lower and is less peaky.
    const double age = juce::jlimit (0.0, 1.0, cfg.speakerAge);
    const double lowCorner = sp.lowCornerHz * (1.0 - age * 0.12);
    const double presenceDb = sp.presenceDb * (1.0 - age * 0.22);

    // Closed-back cabs are tighter and have less low-end extension than open ones.
    double cabLowTrim = 1.0;
    double cabBodyDb = 0.0;

    switch (cfg.cabinet)
    {
        case CabinetType::Cab1x12Open:   cabLowTrim = 1.20; cabBodyDb = -1.0; break;
        case CabinetType::Cab1x12Closed: cabLowTrim = 0.95; cabBodyDb =  1.0; break;
        case CabinetType::Cab2x12Open:   cabLowTrim = 1.10; cabBodyDb = -0.5; break;
        case CabinetType::Cab2x12Closed: cabLowTrim = 0.90; cabBodyDb =  1.5; break;
        case CabinetType::Cab4x12:       cabLowTrim = 0.80; cabBodyDb =  2.5; break;
        case CabinetType::Cab4x12Vintage:cabLowTrim = 0.84; cabBodyDb =  2.0; break;
        case CabinetType::Cab1x15Bass:   cabLowTrim = 0.60; cabBodyDb =  3.0; break;
        case CabinetType::Cab4x10Bass:   cabLowTrim = 0.65; cabBodyDb =  2.5; break;
        case CabinetType::Cab8x10Bass:   cabLowTrim = 0.55; cabBodyDb =  3.5; break;
        case CabinetType::AcousticDI:    cabLowTrim = 1.00; cabBodyDb =  0.0; break;
        case CabinetType::NumCabinets:
        default: break;
    }

    // Mic position: on-axis at the dust cap is brightest, off-axis and at the
    // cone edge progressively darker. Distance trades proximity bass for room.
    double axisTrimDb = 0.0;
    double topTrim = 1.0;

    switch (cfg.position)
    {
        case MicPosition::OnAxisCentre:  axisTrimDb =  2.5; topTrim = 1.15; break;
        case MicPosition::OnAxisCapEdge: axisTrimDb =  0.0; topTrim = 1.00; break;
        case MicPosition::OffAxis45:     axisTrimDb = -2.5; topTrim = 0.80; break;
        case MicPosition::OffAxisEdge:   axisTrimDb = -4.5; topTrim = 0.65; break;
        case MicPosition::Rear:          axisTrimDb = -6.0; topTrim = 0.55; break;
        case MicPosition::NumPositions:
        default: break;
    }

    double proximityScale = 1.0;

    switch (cfg.distance)
    {
        case MicDistance::Close:  proximityScale = 1.00; break;
        case MicDistance::Medium: proximityScale = 0.45; break;
        case MicDistance::Far:    proximityScale = 0.10; break;
        case MicDistance::NumDistances:
        default: break;
    }

    if (cfg.cabinet == CabinetType::AcousticDI)
    {
        // The acoustic path is not a guitar speaker at all: it should stay open.
        highpass.setHighpass (sr, 35.0, 0.707);
        lowShelf.setLowShelf (sr, 160.0, 0.7, 0.0);
        bodyPeak.setPeaking (sr, 300.0, 1.0, 0.0);
        presencePeak.setPeaking (sr, 4000.0, 0.8, 1.0);
        topRoll.setLowpass (sr, juce::jmin (18000.0, sr * 0.47), 0.707);
        topRoll2.setLowpass (sr, juce::jmin (19000.0, sr * 0.48), 0.707);
        return;
    }

    highpass.setHighpass (sr, juce::jlimit (30.0, 400.0, lowCorner * cabLowTrim), 0.85);
    lowShelf.setLowShelf (sr, sp.bodyHz * 1.5, 0.7, sp.bodyDb + cabBodyDb);
    bodyPeak.setPeaking (sr, mic.proximityHz, 0.9, mic.proximityDb * proximityScale);
    presencePeak.setPeaking (sr, sp.presenceHz, 1.3,
                             presenceDb + axisTrimDb + mic.presenceDb * 0.35);

    // The steep top-end roll-off is the single most recognisable feature of a
    // guitar cabinet; a 12" speaker is effectively a brick wall above 5 kHz.
    const double corner = juce::jmin (sp.topRollHz * topTrim, juce::jmin (mic.topHz, sr * 0.46));
    topRoll.setLowpass (sr, corner, 1.1);
    topRoll2.setLowpass (sr, juce::jmin (corner * 1.15, sr * 0.47), 0.62);
}

void CabinetEngine::MicPath::resetFallback() noexcept
{
    lowShelf.reset();
    bodyPeak.reset();
    presencePeak.reset();
    topRoll.reset();
    topRoll2.reset();
    highpass.reset();

    if (convolution != nullptr)
    {
        // A try-lock, so a reset is safe from any thread. If a response is being
        // swapped in right now there is nothing to clear: the swap resets the
        // convolution itself once it completes.
        const juce::SpinLock::ScopedTryLockType lock (convolutionLock);

        if (lock.isLocked())
            convolution->reset();
    }
}

//==============================================================================
void CabinetEngine::prepare (double sampleRate, int maxBlockSize)
{
    pathA.loadedFile = juce::File();
    pathB.loadedFile = juce::File();

    sr = sampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = (juce::uint32) maxBlock;
    spec.numChannels = 1;

    pathA.convolution->prepare (spec);
    pathB.convolution->prepare (spec);
    prepared = true;

    blendSmooth.prepare (sr, constants::kParamSmoothSeconds);
    widthSmooth.prepare (sr, constants::kParamSmoothSeconds);
    blendSmooth.snapTo (0.0);
    widthSmooth.snapTo (0.5);

    // Up to 300 mm of mic separation to compensate.
    alignSize = juce::nextPowerOfTwo ((int) (sr * 0.3 / kSpeedOfSoundMmPerSample * 1000.0) + 64);
    alignSize = juce::jmax (64, alignSize);
    alignMask = alignSize - 1;
    alignBuffer.assign ((size_t) alignSize, 0.0);
    alignIndex = 0;

    bufferA.setSize (1, maxBlock, false, true, true);
    bufferB.setSize (1, maxBlock, false, true, true);
    micInput.assign ((size_t) maxBlock, 0.0f);   // SPEC-SWEEP TM-7

    dcL.prepare (sr, 10.0);
    dcR.prepare (sr, 10.0);

    rebuildFallbacks();
    reset();
}

void CabinetEngine::reset() noexcept
{
    pathA.resetFallback();
    pathB.resetFallback();

    // The loaded responses' tails too; a try-lock, as BodyEngine::reset does,
    // because a response being swapped in resets itself when the swap lands.
    for (auto* path : { &pathA, &pathB })
    {
        const juce::SpinLock::ScopedTryLockType lock (path->convolutionLock);

        if (lock.isLocked() && path->convolution != nullptr)
            path->convolution->reset();
    }

    std::fill (alignBuffer.begin(), alignBuffer.end(), 0.0);
    alignIndex = 0;

    bufferA.clear();
    bufferB.clear();

    dcL.reset();
    dcR.reset();

    blendSmooth.snapToTarget();
    widthSmooth.snapToTarget();
}

void CabinetEngine::rebuildFallbacks() noexcept
{
    pathA.prepareFallback (sr, configA);
    pathB.prepareFallback (sr, configB);
}

//==============================================================================
void CabinetEngine::setConfigA (const CabinetConfig& cfg)
{
    configA = cfg;
    pathA.prepareFallback (sr, configA);
}

void CabinetEngine::setConfigB (const CabinetConfig& cfg)
{
    configB = cfg;
    pathB.prepareFallback (sr, configB);
}

void CabinetEngine::setDualMicEnabled (bool e) noexcept
{
    dualMic = e;
}

void CabinetEngine::setMicBlend (double blend) noexcept
{
    blendSmooth.setTarget (juce::jlimit (0.0, 1.0, blend));
}

void CabinetEngine::setStereoWidth (double width) noexcept
{
    widthSmooth.setTarget (juce::jlimit (0.0, 1.0, width));
}

void CabinetEngine::setPhaseAlignMm (double mm) noexcept
{
    phaseAlignMm = juce::jlimit (0.0, 300.0, mm);
    alignSamples = juce::jlimit (0, alignSize - 2,
                                 (int) std::round (phaseAlignMm * 0.001 / 343.0 * sr));
}

//==============================================================================
bool CabinetEngine::loadImpulseResponse (int slot, const juce::File& file)
{
    auto& path = (slot == 0) ? pathA : pathB;

    if (path.loaded.load() && file == path.loadedFile && file.existsAsFile())
        return true;

    ThreadProbe::noteFileAccess();
    path.loadedFile = juce::File();

    if (! file.existsAsFile())
    {
        path.loaded.store (false);
        return false;
    }

    // The fallback carries the signal for as long as the swap takes, so the amp is
    // never heard without a speaker on it.
    path.loaded.store (false);

    const juce::SpinLock::ScopedLockType lock (path.convolutionLock);

    if (prepared)
        ConvolutionInstaller::installUnitImpulse (*path.convolution, sr, 1, maxBlock);

    path.convolution->loadImpulseResponse (file,
                                           juce::dsp::Convolution::Stereo::no,
                                           juce::dsp::Convolution::Trim::yes,
                                           0,
                                           juce::dsp::Convolution::Normalise::yes);

    if (prepared)
    {
        if (! ConvolutionInstaller::pumpUntilInstalled (*path.convolution, 1, maxBlock, 1, 4000, (int) (0.06 * sr)))
            return false;

        path.convolution->reset();
    }

    path.loadedFile = file;
    ++path.loadCount;
    path.loaded.store (true);
    return true;
}

void CabinetEngine::loadImpulseResponse (int slot, const float* samples, int numSamples, double irSampleRate)
{
    auto& path = (slot == 0) ? pathA : pathB;
    path.loadedFile = juce::File();

    if (samples == nullptr || numSamples <= 0)
    {
        path.loaded.store (false);
        return;
    }

    juce::AudioBuffer<float> ir (1, numSamples);
    ir.copyFrom (0, 0, samples, numSamples);

    path.loaded.store (false);

    const juce::SpinLock::ScopedLockType lock (path.convolutionLock);

    if (prepared)
        ConvolutionInstaller::installUnitImpulse (*path.convolution, sr, 1, maxBlock);

    path.convolution->loadImpulseResponse (std::move (ir),
                                           irSampleRate,
                                           juce::dsp::Convolution::Stereo::no,
                                           juce::dsp::Convolution::Trim::no,
                                           juce::dsp::Convolution::Normalise::yes);

    if (prepared)
    {
        if (! ConvolutionInstaller::pumpUntilInstalled (*path.convolution, 1, maxBlock, 1, 4000, (int) (0.06 * sr)))
            return;

        path.convolution->reset();
    }

    path.loaded.store (true);
}

bool CabinetEngine::hasImpulseResponse (int slot) const noexcept
{
    return (slot == 0) ? pathA.loaded.load() : pathB.loaded.load();
}

int CabinetEngine::getLatencySamples() const noexcept
{
    if (! enabled)
        return 0;

    int latency = 0;

    if (pathA.loaded.load() && pathA.convolution != nullptr)
        latency = (int) pathA.convolution->getLatency();

    if (dualMic && pathB.loaded.load() && pathB.convolution != nullptr)
        latency = juce::jmax (latency, (int) pathB.convolution->getLatency());

    return latency;
}

//==============================================================================
void CabinetEngine::processBlock (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (! enabled || numSamples <= 0 || numChannels <= 0)
    {
        micTapsValid = false;
        micTapSamples = 0;
        return;
    }

    bufferA.setSize (1, numSamples, false, false, true);
    bufferB.setSize (1, numSamples, false, false, true);

    auto* inL = buffer.getWritePointer (0);
    auto* inR = (numChannels > 1) ? buffer.getWritePointer (1) : inL;

    auto* a = bufferA.getWritePointer (0);
    auto* b = bufferB.getWritePointer (0);

    // The amp is mono, so both mics see the same signal.
    for (int i = 0; i < numSamples; ++i)
    {
        const float mono = (numChannels > 1) ? 0.5f * (inL[i] + inR[i]) : inL[i];
        a[i] = mono;
        b[i] = mono;
    }

    // SPEC-SWEEP TM-7: what each mic hears, for a user IR that replaces it.
    const bool haveInputCopy = numSamples <= (int) micInput.size();

    if (haveInputCopy)
        std::copy (a, a + numSamples, micInput.begin());

    // ---- mic A ----------------------------------------------------------------
    {
        const juce::SpinLock::ScopedTryLockType lock (pathA.convolutionLock);

        if (pathA.loaded.load() && lock.isLocked())
        {
            juce::dsp::AudioBlock<float> blockA (bufferA);
            auto sub = blockA.getSubBlock (0, (size_t) numSamples);
            juce::dsp::ProcessContextReplacing<float> ctx (sub);
            pathA.convolution->process (ctx);
        }
        else
        {
            for (int i = 0; i < numSamples; ++i)
                a[i] = (float) sanitise (pathA.processFallback ((double) a[i]));
        }

        // SPEC-SWEEP TM-7: cabinet slot 1 in place of mic A's response.
        if (userSlotA != nullptr && haveInputCopy && userSlotA->isEngaged())
            userSlotA->processReplacing (micInput.data(), a, numSamples);
    }

    // ---- mic B ----------------------------------------------------------------
    if (dualMic)
    {
        const juce::SpinLock::ScopedTryLockType lock (pathB.convolutionLock);

        if (pathB.loaded.load() && lock.isLocked())
        {
            juce::dsp::AudioBlock<float> blockB (bufferB);
            auto sub = blockB.getSubBlock (0, (size_t) numSamples);
            juce::dsp::ProcessContextReplacing<float> ctx (sub);
            pathB.convolution->process (ctx);
        }
        else
        {
            for (int i = 0; i < numSamples; ++i)
                b[i] = (float) sanitise (pathB.processFallback ((double) b[i]));
        }

        // SPEC-SWEEP TM-7: cabinet slot 2 in place of mic B's response.
        if (userSlotB != nullptr && haveInputCopy && userSlotB->isEngaged())
            userSlotB->processReplacing (micInput.data(), b, numSamples);

        // Time-of-flight alignment: the further mic hears the cabinet later, and
        // summing them without compensating comb-filters the result.
        if (alignSamples > 0 && alignSize > 0)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                alignBuffer[(size_t) alignIndex] = (double) b[i];
                const int readIndex = (alignIndex - alignSamples) & alignMask;
                b[i] = (float) alignBuffer[(size_t) readIndex];
                alignIndex = (alignIndex + 1) & alignMask;
            }
        }
    }

    // ---- blend and place ------------------------------------------------------
    for (int i = 0; i < numSamples; ++i)
    {
        const double blend = dualMic ? blendSmooth.next() : 0.0;
        const double width = widthSmooth.next();

        const double sampleA = (double) a[i];
        const double sampleB = dualMic ? (double) b[i] : sampleA;

        // Equal-power blend so moving the control never dips in the middle.
        const double gainA = std::cos (blend * constants::kPi * 0.5);
        const double gainB = std::sin (blend * constants::kPi * 0.5);

        // Width spreads the two mics across the image. At width 0 they are both
        // dead centre, which is the mono-safe default.
        const double spread = dualMic ? width : 0.0;

        double outL = sampleA * gainA * (1.0 - spread * 0.5) + sampleB * gainB * (spread * 0.5);
        double outR = sampleA * gainA * (spread * 0.5) + sampleB * gainB * (1.0 - spread * 0.5);

        if (! dualMic)
        {
            outL = sampleA;
            outR = sampleA;
        }

        inL[i] = (float) sanitise (dcL.process (outL));

        if (numChannels > 1)
            inR[i] = (float) sanitise (dcR.process (outR));
    }

    // The blend wrote to the caller's buffer, not to a[] and b[], so the two mic
    // signals are still intact and can be handed out as tap points.
    micTapsValid = true;
    micTapSamples = numSamples;
}

//==============================================================================
juce::String CabinetEngine::irFileNameFor (const CabinetConfig& cfg)
{
    return slug (getCabinetName (cfg.cabinet)) + "/"
           + slug (getSpeakerName (cfg.speaker)) + "_"
           + slug (getMicName (cfg.mic)) + "_"
           + slug (getPositionName (cfg.position)) + "_"
           + slug (getDistanceName (cfg.distance)) + ".wav";
}

const char* CabinetEngine::getCabinetName (CabinetType t) noexcept
{
    switch (t)
    {
        case CabinetType::Cab1x12Open:    return "1x12 Open";
        case CabinetType::Cab1x12Closed:  return "1x12 Closed";
        case CabinetType::Cab2x12Open:    return "2x12 Open";
        case CabinetType::Cab2x12Closed:  return "2x12 Closed";
        case CabinetType::Cab4x12:        return "4x12";
        case CabinetType::Cab4x12Vintage: return "4x12 Vintage";
        case CabinetType::Cab1x15Bass:    return "1x15 Bass";
        case CabinetType::Cab4x10Bass:    return "4x10 Bass";
        case CabinetType::Cab8x10Bass:    return "8x10 Bass";
        case CabinetType::AcousticDI:     return "Acoustic DI";
        case CabinetType::NumCabinets:
        default:                          return "4x12";
    }
}

const char* CabinetEngine::getSpeakerName (SpeakerType t) noexcept
{
    return kSpeakers[(size_t) juce::jlimit (0, (int) SpeakerType::NumSpeakers - 1, (int) t)].name;
}

const char* CabinetEngine::getMicName (MicType t) noexcept
{
    return kMics[(size_t) juce::jlimit (0, (int) MicType::NumMics - 1, (int) t)].name;
}

const char* CabinetEngine::getPositionName (MicPosition p) noexcept
{
    switch (p)
    {
        case MicPosition::OnAxisCentre:  return "On-Axis Centre";
        case MicPosition::OnAxisCapEdge: return "Cap Edge";
        case MicPosition::OffAxis45:     return "Off-Axis 45";
        case MicPosition::OffAxisEdge:   return "Cone Edge";
        case MicPosition::Rear:          return "Rear";
        case MicPosition::NumPositions:
        default:                         return "Cap Edge";
    }
}

const char* CabinetEngine::getDistanceName (MicDistance d) noexcept
{
    switch (d)
    {
        case MicDistance::Close:  return "Close";
        case MicDistance::Medium: return "Medium";
        case MicDistance::Far:    return "Far";
        case MicDistance::NumDistances:
        default:                  return "Close";
    }
}

} // namespace luthier
