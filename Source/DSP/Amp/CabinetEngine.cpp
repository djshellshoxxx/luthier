#include "CabinetEngine.h"
#include "CabinetVoices.h"
#include "MicPlacement.h"
#include <iterator>
#include "../../ToneMatch/ToneMatch.h"   // SPEC-SWEEP TM-7
#include "../../Support/ThreadProbe.h"

namespace luthier
{

namespace
{
    constexpr int kCabPartitionSize = 128;
    constexpr double kSpeedOfSoundMmPerSample = 343.0;   // scaled by sample rate below

    // Resource keys are persistent asset identifiers, independent of translated or
    // generic display labels. Enum order is also the saved parameter order.
    constexpr const char* cabKeys[] = { "1x12_open", "1x12_closed", "2x12_open", "2x12_closed",
        "4x12", "4x12_vintage", "1x15_bass", "4x10_bass", "8x10_bass", "acoustic_di" };
    constexpr const char* speakerKeys[] = { "celestion_greenback", "celestion_vintage_30",
        "celestion_g12h", "celestion_g12t_75", "jensen_c12", "alnico_blue", "evm_12l", "bass_ceramic" };
    constexpr const char* micKeys[] = { "shure_sm57", "shure_sm7b", "sennheiser_md421",
        "neumann_u87", "royer_r_121", "akg_c414", "akg_d112" };
    constexpr const char* positionKeys[] = { "on_axis_centre", "cap_edge", "off_axis_45", "cone_edge", "rear" };
    constexpr const char* distanceKeys[] = { "close", "medium", "far" };
    static_assert (std::size (cabKeys) == (size_t) CabinetType::NumCabinets);
    static_assert (std::size (speakerKeys) == (size_t) SpeakerType::NumSpeakers);
    static_assert (std::size (micKeys) == (size_t) MicType::NumMics);
    static_assert (std::size (positionKeys) == (size_t) MicPosition::NumPositions);
    static_assert (std::size (distanceKeys) == (size_t) MicDistance::NumDistances);

    template <size_t N>
    const char* keyOrDefault (const char* const (&keys)[N], int index, int fallback) noexcept
    {
        return keys[index >= 0 && index < (int) N ? index : fallback];
    }
}

//==============================================================================
CabinetEngine::CabinetEngine()
{
    // mic-placement.md 5.
    for (auto* path : { &pathA, &pathB })
    {
        path->stage = std::make_unique<MicPlacementStage>();
        path->tof = std::make_unique<SlewedDelayLine>();
    }

    tofMode = TofMode::Aligned;

    // Mic 2's defaults are the mapping of its legacy defaults, Off-Axis 45 and
    // Medium (mic-placement.md 7).
    MicPlacement second;
    second.distCm = 15.0;
    second.angleDeg = 45.0;
    pathB.stage->setPlacement (second);
}

CabinetEngine::~CabinetEngine() = default;

//==============================================================================
void CabinetEngine::MicPath::prepareFallback (double sr, const CabinetConfig& cfgIn) noexcept
{
    // mic-placement.md 5: the fallback always uses the anchor terms, so it and
    // the IR path share one placement stage.
    const auto cfg = CabinetEngine::anchorConfig (cfgIn);
    const auto& sp = speakerVoice (cfg.speaker);
    const auto& mic = micVoice (cfg.mic);

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


}

//==============================================================================
void CabinetEngine::prepare (double sampleRate, int maxBlockSize)
{
    // Responses requested before prepare() were only queued with JUCE's
    // background loader; they are installed again below, synchronously, so the
    // first block hears them on every machine (see BodyEngine::prepare).
    const auto pendingA = pathA.loadedFile, pendingB = pathB.loadedFile;
    pathA.loadedFile = juce::File();
    pathB.loadedFile = juce::File();

    sr = sampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);

    prepared = true;
    for (auto* path : { &pathA, &pathB })
        path->convolution.prepare (sr, maxBlock, 1, kCabPartitionSize,
                                   { QualityProfile::forLevel (QualityLevel::Medium).cabinetIrSeconds,
                                     QualityProfile::forLevel (QualityLevel::Low).cabinetIrSeconds },
                                   QualityProfile::kCabinetMicVariantBudgetMegabytes);

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
    bypassBuffer.setSize (2, maxBlock, false, true, true);
    for (auto& delay : bypassDelay) delay.assign ((size_t) pathA.convolution.getLatency() + 1, 0.0f);
    bypassWrite = enableWarmRemaining = 0;
    enabledMix.prepare (sr, 0.020);
    enabledMix.snapTo (enabled ? 1.0 : 0.0);
    micInput.assign ((size_t) maxBlock, 0.0f);   // SPEC-SWEEP TM-7

    // mic-placement.md 5: 12 ms covers 200 cm of path at any rate.
    for (auto* path : { &pathA, &pathB })
    {
        path->stage->prepare (sr, maxBlock);
        path->tof->prepare (sr, 0.012);
    }

    dcL.prepare (sr, 10.0);
    dcR.prepare (sr, 10.0);

    rebuildFallbacks();

    for (int slot = 0; slot < 2; ++slot)
    {
        auto& path = slot == 0 ? pathA : pathB;
        const auto& pendingFile = slot == 0 ? pendingA : pendingB;

        if (pendingFile.existsAsFile())
        {
            loadImpulseResponse (slot, pendingFile);
        }
        else if (path.pendingIr.getNumSamples() > 0)
        {
            const auto ir = path.pendingIr;   // loadImpulseResponse clears the pending copy
            loadImpulseResponse (slot, ir.getReadPointer (0), ir.getNumSamples(), path.pendingIrRate);
        }
    }

    reset();
}

void CabinetEngine::reset() noexcept
{
    bypassWrite = enableWarmRemaining = 0;
    for (auto& delay : bypassDelay) std::fill (delay.begin(), delay.end(), 0.0f);
    enabledMix.snapTo (enabled ? 1.0 : 0.0);
    pathA.resetFallback();
    pathB.resetFallback();

    for (auto* path : { &pathA, &pathB }) path->convolution.reset();

    std::fill (alignBuffer.begin(), alignBuffer.end(), 0.0);
    alignIndex = 0;

    for (auto* path : { &pathA, &pathB })
    {
        path->stage->reset();
        path->tof->reset();
    }

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
    updateStageVoice (pathA, configA);
}

void CabinetEngine::setConfigB (const CabinetConfig& cfg)
{
    configB = cfg;
    pathB.prepareFallback (sr, configB);
    updateStageVoice (pathB, configB);
}

//==============================================================================
CabinetConfig CabinetEngine::anchorConfig (const CabinetConfig& cfg) noexcept
{
    if (cfg.cabinet == CabinetType::AcousticDI)
        return cfg;

    auto anchor = cfg;
    anchor.position = MicPosition::OnAxisCapEdge;
    anchor.distance = MicDistance::Close;
    return anchor;
}

void CabinetEngine::updateStageVoice (MicPath& path, const CabinetConfig& cfg) noexcept
{
    path.stage->setVoice (cfg.cabinet, cfg.speaker, cfg.mic);

    // The Acoustic DI has no speaker to mic: placement lives upstream, in
    // AcousticMicModel (mic-placement.md 3).
    path.stage->setBypassed (cfg.cabinet == CabinetType::AcousticDI || path.userIrBypass);
}

void CabinetEngine::setMicPlacement (int slot, const MicPlacement& placement) noexcept
{
    ((slot <= 0) ? pathA : pathB).stage->setPlacement (placement);
}

const MicPlacement& CabinetEngine::getMicPlacement (int slot) const noexcept
{
    return ((slot <= 0) ? pathA : pathB).stage->getPlacement();
}

void CabinetEngine::setTimeOfFlightMode (TofMode mode) noexcept
{
    tofMode = mode;

    // Physical leaves a rear mic's polarity inverted - the back of the cone;
    // Aligned flips it back, the way an engineer would (mic-placement.md 2.3).
    for (auto* path : { &pathA, &pathB })
        path->stage->setInvertRearPolarity (mode == TofMode::Physical);
}

void CabinetEngine::setLevelMatch (bool on) noexcept
{
    pathA.stage->setLevelMatch (on);
    pathB.stage->setLevelMatch (on);
}

void CabinetEngine::setRoomMaterialForFloor (RoomMaterial material, bool roomOn) noexcept
{
    const double rho = MicPlacementModel::floorRhoFor ((int) material, roomOn);
    pathA.stage->setFloorReflectivity (rho);
    pathB.stage->setFloorReflectivity (rho);
}

void CabinetEngine::setPlacementBypassed (int slot, bool bypassed) noexcept
{
    auto& path = (slot <= 0) ? pathA : pathB;
    path.userIrBypass = bypassed;
    updateStageVoice (path, (slot <= 0) ? configA : configB);
}

bool CabinetEngine::isPlacementBypassed (int slot) const noexcept
{
    return ((slot <= 0) ? pathA : pathB).stage->isBypassed();
}

MicPlacementStage& CabinetEngine::getPlacementStage (int slot) noexcept
{
    return *((slot <= 0) ? pathA : pathB).stage;
}

const SlewedDelayLine& CabinetEngine::getTofLine (int slot) const noexcept
{
    return *((slot <= 0) ? pathA : pathB).tof;
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
    auto& path = slot == 0 ? pathA : pathB;
    if (path.loaded.load() && file == path.loadedFile && file.existsAsFile()) return true;
    ThreadProbe::noteFileAccess();
    if (! file.existsAsFile()) return false;
    if (prepared && ! path.convolution.loadFile (file, juce::dsp::Convolution::Stereo::no)) return false;
    path.pendingIr.setSize (0, 0); path.loadedFile = file;
    ++path.loadCount;
    path.loaded.store (true);
    return true;
}

void CabinetEngine::loadImpulseResponse (int slot, const float* samples, int numSamples, double irSampleRate)
{
    auto& path = slot == 0 ? pathA : pathB;
    if (samples == nullptr || numSamples <= 0 || ! std::isfinite (irSampleRate) || irSampleRate <= 0) return;
    juce::AudioBuffer<float> ir (1, numSamples);
    ir.copyFrom (0, 0, samples, numSamples);
    if (prepared && ! path.convolution.loadBuffer (ir, irSampleRate)) return;
    path.pendingIr = ir; path.pendingIrRate = irSampleRate;
    path.loadedFile = juce::File(); path.loaded.store (true);
}

bool CabinetEngine::hasImpulseResponse (int slot) const noexcept
{
    return (slot == 0) ? pathA.loaded.load() : pathB.loaded.load();
}

int CabinetEngine::getLatencySamples() const noexcept
{
    int latency = 0;

    if (pathA.loaded.load())
        latency = pathA.convolution.getLatency();

    if (dualMic && pathB.loaded.load())
        latency = juce::jmax (latency, pathB.convolution.getLatency());

    return latency;
}

//==============================================================================
void CabinetEngine::processBlock (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0 || numSamples > maxBlock)
    {
        micTapsValid = false;
        micTapSamples = 0;
        return;
    }

    // Keep bypass at the convolution's physical latency. The dry history runs
    // while engaged too, so either edge can crossfade without a time jump.
    const int delaySamples = getLatencySamples();
    const int capacity = (int) bypassDelay[0].size();
    if (capacity <= delaySamples) return;
    for (int i = 0; i < numSamples; ++i)
    {
        const int read = (bypassWrite + capacity - delaySamples) % capacity;
        for (int ch = 0; ch < juce::jmin (2, numChannels); ++ch)
        {
            auto& delay = bypassDelay[(size_t) ch];
            delay[(size_t) bypassWrite] = buffer.getSample (ch, i);
            bypassBuffer.setSample (ch, i, delay[(size_t) read]);
        }
        bypassWrite = (bypassWrite + 1) % capacity;
    }
    if (! enabled) enableWarmRemaining = 0;
    if (enabled && enabledMix.getCurrent() == 0.0 && enabledMix.getTarget() == 0.0 && enableWarmRemaining == 0)
    {
        for (auto* path : { &pathA, &pathB }) path->convolution.reset();
        // Keep delayed dry audible until the reset partition has real history.
        enableWarmRemaining = delaySamples + QualityProfile::kSwitchSettleSamples;
    }
    enabledMix.setTarget (enabled && enableWarmRemaining == 0 ? 1.0 : 0.0);
    if (! enabled && ! enabledMix.isSmoothing())
    {
        for (int ch = 0; ch < juce::jmin (2, numChannels); ++ch)
            buffer.copyFrom (ch, 0, bypassBuffer, ch, 0, numSamples);
        micTapsValid = false; micTapSamples = 0;
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
        if (pathA.loaded.load())
        {
            juce::dsp::AudioBlock<float> blockA (bufferA);
            auto sub = blockA.getSubBlock (0, (size_t) numSamples);
            pathA.convolution.process (sub);   // the full IR at High
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

    // mic-placement.md 5 step 2: where the mic is, after its anchor response.
    pathA.stage->process (a, numSamples);

    // ---- mic B ----------------------------------------------------------------
    if (dualMic)
    {
        if (pathB.loaded.load())
        {
            juce::dsp::AudioBlock<float> blockB (bufferB);
            auto sub = blockB.getSubBlock (0, (size_t) numSamples);
            pathB.convolution.process (sub);   // the full IR at High
        }
        else
        {
            for (int i = 0; i < numSamples; ++i)
                b[i] = (float) sanitise (pathB.processFallback ((double) b[i]));
        }

        // SPEC-SWEEP TM-7: cabinet slot 2 in place of mic B's response.
        if (userSlotB != nullptr && haveInputCopy && userSlotB->isEngaged())
            userSlotB->processReplacing (micInput.data(), b, numSamples);

        pathB.stage->process (b, numSamples);

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

    // ---- time of arrival (mic-placement.md 2.3) ----------------------------------
    // Physical: the farther mic hears the cabinet later and is delayed by the
    // path difference; the nearer one is not, so no latency is added. Aligned,
    // or a single mic: no relative delay. The delay glides under a slew limit.
    {
        double delayA = 0.0, delayB = 0.0;

        if (dualMic && tofMode == TofMode::Physical)
        {
            const double diff = (pathB.stage->getPathLengthM() - pathA.stage->getPathLengthM())
                                / MicPlacementModel::kSpeedOfSound * sr;
            delayA = juce::jmax (0.0, -diff);
            delayB = juce::jmax (0.0, diff);
        }

        pathA.tof->setTargetDelaySamples (delayA);
        pathA.tof->process (a, numSamples);

        if (dualMic)
        {
            pathB.tof->setTargetDelaySamples (delayB);
            pathB.tof->process (b, numSamples);
        }
        else
        {
            // Nothing is rendered on mic 2: it starts from its target when it
            // is switched on, rather than gliding in from wherever it was.
            pathB.tof->setTargetDelaySamples (0.0);
            pathB.tof->snapToTarget();
        }
    }

    // ---- blend and place ------------------------------------------------------
    if (! dualMic)
    {
        // Single mic: the blend collapses to mic A (gainA = cos 0 = 1, gainB = 0,
        // spread = 0), so the equal-power trig and the width/spread arithmetic all
        // drop out. Only mic A reaches the DC blocker, exactly as before. The
        // width smoother is still advanced so its state matches when the second
        // mic returns (blendSmooth was never advanced on this path).
        for (int i = 0; i < numSamples; ++i)
        {
            widthSmooth.next();

            const double sampleA = (double) a[i];
            inL[i] = (float) sanitise (dcL.process (sampleA));

            if (numChannels > 1)
                inR[i] = (float) sanitise (dcR.process (sampleA));
        }
    }
    else
    {
        // Equal-power gains move only while the blend control glides; parked (the
        // common case) cos/sin take the same argument every sample, so they are
        // computed once up front. Width still smooths per sample. A parked
        // ExpSmoother returns its target verbatim, so hoisting is bit-exact.
        const bool blendStatic = ! blendSmooth.isSmoothing();
        double gainA = 0.0, gainB = 0.0;

        if (blendStatic)
        {
            const double blend = blendSmooth.getCurrent();
            gainA = std::cos (blend * constants::kPi * 0.5);
            gainB = std::sin (blend * constants::kPi * 0.5);
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const double blend = blendSmooth.next();
            const double width = widthSmooth.next();

            if (! blendStatic)
            {
                // Equal-power blend so moving the control never dips in the middle.
                gainA = std::cos (blend * constants::kPi * 0.5);
                gainB = std::sin (blend * constants::kPi * 0.5);
            }

            const double sampleA = (double) a[i];
            const double sampleB = (double) b[i];

            // Width spreads the two mics across the image. At width 0 they are both
            // dead centre, which is the mono-safe default.
            const double spread = width;

            const double outL = sampleA * gainA * (1.0 - spread * 0.5) + sampleB * gainB * (spread * 0.5);
            const double outR = sampleA * gainA * (spread * 0.5) + sampleB * gainB * (1.0 - spread * 0.5);

            inL[i] = (float) sanitise (dcL.process (outL));

            if (numChannels > 1)
                inR[i] = (float) sanitise (dcR.process (outR));
        }
    }

    if (enabledMix.isSmoothing() || enabledMix.getCurrent() != 1.0)
        for (int i = 0; i < numSamples; ++i)
        {
            if (enabled && enableWarmRemaining > 0)
            {
                --enableWarmRemaining;
                if (enableWarmRemaining == 0) enabledMix.setTarget (1.0);
            }
            const float mix = (float) enabledMix.next();
            for (int ch = 0; ch < juce::jmin (2, numChannels); ++ch)
                buffer.setSample (ch, i, buffer.getSample (ch, i) * mix
                                  + bypassBuffer.getSample (ch, i) * (1.0f - mix));
        }

    // The blend wrote to the caller's buffer, not to a[] and b[], so the two mic
    // signals are still intact and can be handed out as tap points.
    micTapsValid = enabled;
    micTapSamples = enabled ? numSamples : 0;
}

//==============================================================================
juce::String CabinetEngine::irFileNameFor (const CabinetConfig& cfg)
{
    return juce::String (keyOrDefault (cabKeys, (int) cfg.cabinet, (int) CabinetType::Cab4x12)) + "/"
           + speakerKeys[juce::jlimit (0, (int) SpeakerType::NumSpeakers - 1, (int) cfg.speaker)] + "_"
           + micKeys[juce::jlimit (0, (int) MicType::NumMics - 1, (int) cfg.mic)] + "_"
           + keyOrDefault (positionKeys, (int) cfg.position, (int) MicPosition::OnAxisCapEdge) + "_"
           + keyOrDefault (distanceKeys, (int) cfg.distance, (int) MicDistance::Close) + ".wav";
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

//==============================================================================
void CabinetEngine::setQualityLevel (const QualityProfile& profile, bool hard) noexcept
{
    pathA.convolution.setLevel ((int) profile.level, hard);
    pathB.convolution.setLevel ((int) profile.level, hard);

    // mic-placement.md 5 under cpu-quality-modes (INTEGRATE-2).
    for (auto* path : { &pathA, &pathB })
        if (path->stage != nullptr)
            path->stage->setEvaluateEvery (profile.micEvaluateEvery);
}

} // namespace luthier
