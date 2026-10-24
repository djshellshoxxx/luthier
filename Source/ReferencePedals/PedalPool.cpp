// OWNER: WP2
#include "PedalPool.h"
#include "PedalRegistry.h"
#include "ExtraState.h"
#include "Services/AssetStore.h"
#include <algorithm>
#include <exception>
namespace luthier::reference
{
void PedalRuntime::writeParam (int index, float value, float smoothMs) noexcept
{
    if (index < 0 || index >= pedal->desc().numParams || ! std::isfinite (value)) return;
    value = std::clamp (value, 0.f, 1.f);
    const auto bit = uint32_t (1) << index;
    const auto& descriptor = pedal->desc().params[index];
    auto& ramp = ramps[static_cast<size_t> (index)];
    if (smoothMs <= 0 || descriptor.has (PF_OwnSmoothing)
        || (descriptor.kind != ParamKind::Continuous && descriptor.kind != ParamKind::Tempo))
    {
        rampMask &= ~bit; ramp.value = ramp.target = value; ramp.coefficient = 0;
        pedal->setParam (index, value); return;
    }
    if ((rampMask & bit) == 0) ramp.value = pedal->getParam (index);
    ramp.target = value;
    ramp.coefficient = std::exp (-1.0 / (std::clamp (static_cast<double> (smoothMs), 0.001, 500.0) * 0.001 * sampleRate));
    rampMask |= bit;
}
void PedalRuntime::advanceParams() noexcept
{
    for (int param = 0; param < pedal->desc().numParams; ++param)
    {
        const auto bit = uint32_t (1) << param;
        if ((rampMask & bit) == 0) continue;
        auto& ramp = ramps[static_cast<size_t> (param)];
        ramp.value = ramp.target + ramp.coefficient * (ramp.value - ramp.target);
        if (std::abs (ramp.value - ramp.target) < 1.e-7) { ramp.value = ramp.target; rampMask &= ~bit; }
        if (std::abs (ramp.value) < 1.e-15) ramp.value = 0;
        pedal->setParam (param, static_cast<float> (ramp.value));
    }
}
void PedalRuntime::collectMidi (int sampleOffset, int samples) noexcept
{
    MidiEvent output[64]; const int count = std::clamp (pedal->drainMidiOut (output, 64), 0, 64);
    for (int i = 0; i < count; ++i)
    {
        if (numRenderedMidi == static_cast<int> (renderedMidi.size())) { ++midiDropped; continue; }
        output[i].offset = sampleOffset + std::clamp (output[i].offset, 0, samples - 1);
        renderedMidi[static_cast<size_t> (numRenderedMidi++)] = output[i];
    }
}
void PedalRuntime::reset() noexcept
{
    pedal->reset();
    for (auto& line : dry) line.reset();
    enableMix = 0.0;
}
bool PedalPool::setAssetStore (std::shared_ptr<IAssetStore> store)
{
    for (const auto& runtime : objects) if (! runtime.expired()) return false;
    assets = std::move (store); assetStoreConfigured = true; return true;
}
void PedalPool::setPreparationFormat (double sampleRate, int maxBlock) noexcept
{ rate = sampleRate; block = maxBlock; objects.clear(); }
void PedalPool::prepare (double sampleRate, int maxBlock, const std::vector<PedalInstance>& captured, const std::vector<PedalInstance>& stored)
{
    setPreparationFormat (sampleRate, maxBlock); preparedExtras = captured; preparedSources = stored;
}
bool PedalPool::preflightFresh (const std::vector<PedalInstance>& instances, juce::String& error) const
{
    if (! std::isfinite (rate) || rate < 8000 || rate > 1000000 || block < 1)
    { error = "Unsupported pedal preparation sample rate/block"; return false; }
    uint64_t retained = memoryBudget.used(), scratch = 0;
    for (const auto& instance : instances)
    {
        if (instance.missing) continue;
        const auto* desc = PedalRegistry::find (instance.modelId.c_str());
        if (! desc) { error = "Unknown pedal model"; return false; }
        const auto requirement = PedalRegistry::memoryRequirement (instance.modelId.c_str(), rate, block, 2, instance.params.data());
        if (requirement.ownedBytes == kUnknownPreparedMemoryBytes || requirement.ownedBytes < sizeof (IPedal))
        { error = "Prepared memory requirement unavailable for " + juce::String (instance.modelId); return false; }
        const uint64_t dry = 2 * (static_cast<uint64_t> (desc->maxLatencySamples) + 1) * sizeof (double);
        if (! addPreparedBytes (retained, sizeof (PedalRuntime), retained)
            || ! addPreparedBytes (retained, dry, retained)
            || ! addPreparedBytes (retained, requirement.ownedBytes, retained))
        { error = "Prepared memory requirement overflow"; return false; }
        scratch = std::max (scratch, requirement.temporaryBytes);
    }
    uint64_t peak;
    if (! addPreparedBytes (retained, scratch, peak) || peak > kPreparedPedalMemoryLimit)
    { error = "Prepared pedal memory budget reached (64000000 bytes, including retained runtimes and prepare scratch)"; return false; }
    return true;
}
std::shared_ptr<PedalRuntime> PedalPool::acquire (const PedalInstance& instance, juce::String& error, const PedalRuntime* active, bool restoreExtras)
try
{
    if (instance.missing) return {};
    if (! std::isfinite (rate) || rate < 8000 || rate > 1000000 || block < 1)
    { error = "Unsupported pedal preparation sample rate/block"; return {}; }
    const auto* desc = PedalRegistry::find (instance.modelId.c_str());
    if (desc == nullptr) { error = "Unknown pedal model: " + juce::String (instance.modelId); return {}; }
    auto extras = instance.extras.clone();
    if (extras.isVoid()) extras = new juce::DynamicObject(); // absent and empty extras have identical DSP meaning
    if (auto* object = extras.getDynamicObject()) object->removeProperty ("midi");
    juce::String extrasKey;
    if (! BoardState::canonicalJson (extras, extrasKey, &error)) return {};
    bool restoreRuntimeState = false;
    if (restoreExtras && active && active->identity == instance.instanceId && active->model == instance.modelId)
    {
        auto captured = extras.clone(); active->pedal->saveExtra (captured);
        auto liveExtras = extras.clone();
        if (auto* object = liveExtras.getDynamicObject(); object && captured.getDynamicObject())
            for (const auto& property : captured.getDynamicObject()->getProperties()) setExactExtraProperty (*object, property.name, property.value);
        else liveExtras = captured.clone();
        if (auto* object = liveExtras.getDynamicObject()) object->removeProperty ("midi");
        juce::String liveKey;
        if (! BoardState::canonicalJson (liveExtras, liveKey, &error)) return {};
        restoreRuntimeState = liveKey != extrasKey;
    }
    for (auto& weak : objects)
        if (auto existing = weak.lock())
        {
            bool matches = ! restoreRuntimeState && (! active || existing.get() == active) && existing->identity == instance.instanceId && existing->model == instance.modelId
                && existing->revision == instance.modelRevision && existing->extrasKey == extrasKey;
            for (int i = 0; matches && i < desc->numParams; ++i)
                if (desc->params[i].has (PF_Config) && existing->config[static_cast<size_t> (i)] != instance.params[static_cast<size_t> (i)])
                    matches = false;
            if (matches) return existing;
        }
    // E69: reserve the whole peak before factory construction or prepare. A
    // reused shared runtime above already owns its single retained reservation.
    const auto requirement = PedalRegistry::memoryRequirement (instance.modelId.c_str(), rate, block, 2, instance.params.data());
    if (requirement.ownedBytes == kUnknownPreparedMemoryBytes || requirement.ownedBytes < sizeof (IPedal))
    { error = "Prepared memory requirement unavailable for " + juce::String (instance.modelId); return {}; }
    const uint64_t dryBound = 2 * (static_cast<uint64_t> (desc->maxLatencySamples) + 1) * sizeof (double);
    uint64_t retainedBound = 0, peak = 0;
    if (! addPreparedBytes (sizeof (PedalRuntime), dryBound, retainedBound)
        || ! addPreparedBytes (retainedBound, requirement.ownedBytes, retainedBound)
        || ! addPreparedBytes (retainedBound, requirement.temporaryBytes, peak))
    { error = "Prepared memory requirement overflow for " + juce::String (instance.modelId); return {}; }
    PreparedMemoryBudget::Reservation reservation;
    if (! memoryBudget.reserve (peak, reservation))
    { error = "Prepared pedal memory budget reached (64000000 bytes, including retained runtimes and prepare scratch)"; return {}; }
    auto loadExtras = extras.clone();
    if (! restoreExtras)
    for (auto saved = preparedExtras.begin(); saved != preparedExtras.end(); ++saved)
        if (saved->instanceId == instance.instanceId && saved->modelId == instance.modelId && saved->modelRevision == instance.modelRevision)
        {
            bool matchesSource = preparedSources.empty();
            for (const auto& source : preparedSources)
                if (source.instanceId == instance.instanceId && source.modelId == instance.modelId && source.modelRevision == instance.modelRevision)
                {
                    auto sourceExtras = source.extras.clone();
                    if (sourceExtras.isVoid()) sourceExtras = new juce::DynamicObject();
                    if (auto* object = sourceExtras.getDynamicObject()) object->removeProperty ("midi");
                    juce::String sourceKey;
                    if (! BoardState::canonicalJson (sourceExtras, sourceKey, &error)) return {};
                    matchesSource = sourceKey == extrasKey; break;
                }
            if (matchesSource) loadExtras = saved->extras.clone();
            break;
        }
    if (! restoreExtras && active && active->identity == instance.instanceId && active->model == instance.modelId
        && active->revision == instance.modelRevision && active->extrasKey == extrasKey)
        active->pedal->saveExtra (loadExtras);
    if (auto* object = loadExtras.getDynamicObject()) object->removeProperty ("midi");
    if (! assetStoreConfigured)
    {
        try
        {
            const auto directory = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                .getChildFile ("Luthier").getChildFile ("ReferencePedals").getChildFile ("assets");
            // AssetStore creates directories only on an actual worker/message-thread write.
            assets = std::make_shared<AssetStore> (directory); assetStoreConfigured = true;
        }
        catch (...) { error = "Unable to create persistent pedal asset service"; return {}; }
    }
    auto result = std::make_shared<PedalRuntime>();
    result->memory = std::move (reservation);
    result->assetOwner = assets; result->sampleRate = rate;
    result->pedal = PedalRegistry::make (instance.modelId.c_str());
    if (! result->pedal) { error = "Pedal factory failed"; return {}; }
    result->identity = instance.instanceId; result->model = instance.modelId;
    result->revision = instance.modelRevision; result->config = instance.params; result->extrasKey = extrasKey;
    PedalServices services; services.sampleRate = rate; services.maxBlock = block; services.assets = result->assetOwner.get();
    services.instanceSeed = static_cast<uint32_t> (instance.instanceId.hash());
    result->pedal->setServices (services);
    for (int i = 0; i < desc->numParams; ++i)
        result->pedal->setParam (i, instance.params[static_cast<size_t> (i)]);
    result->pedal->prepare (rate, block, 2);
    result->pedal->loadExtra (loadExtras);
    result->pedal->setEnabled (instance.enabled);
    const int latency = result->pedal->getLatencySamples();
    if (latency < 0 || latency > kMaxLatencySamp || latency > desc->maxLatencySamples)
    { error = "Invalid pedal latency or exceeded declared maximum"; return {}; }
    for (auto& line : result->dry) line.prepare (latency);
    const auto modelBytes = result->pedal->getPreparedMemoryBytes();
    uint64_t actual = sizeof (PedalRuntime);
    if (modelBytes < sizeof (IPedal) || ! addPreparedBytes (actual, modelBytes, actual)
        || ! addPreparedBytes (actual, result->dry[0].preparedMemoryBytes(), actual)
        || ! addPreparedBytes (actual, result->dry[1].preparedMemoryBytes(), actual)
        || modelBytes > requirement.ownedBytes || actual > retainedBound)
    { error = "Prepared memory report unavailable or exceeds declared reservation for " + juce::String (instance.modelId); return {}; }
    result->memory.shrink (actual); // temporary prepare payload and unused dry bound are released off audio
    result->enabled = result->modelEnabled = instance.enabled;
    result->modelParams = instance.params;
    result->inputGain = result->modelInputGain = std::pow (10.f, instance.inDb / 20.f);
    result->outputGain = result->modelOutputGain = std::pow (10.f, instance.outDb / 20.f);
    result->bypassSamples = std::max (1.0, rate * 0.005);
    result->foot.prepare (rate); result->foot.reset (instance.enabled);
    objects.emplace_back (result);
    return result;
}
catch (const std::exception& failure)
{
    error = "Pedal preparation/restore failed: " + juce::String (failure.what()); return {};
}
catch (...)
{
    error = "Pedal preparation/restore failed with an unknown error"; return {};
}
void PedalPool::reclaim()
{
    objects.erase (std::remove_if (objects.begin(), objects.end(), [] (const auto& p) { return p.expired(); }), objects.end());
}
}
