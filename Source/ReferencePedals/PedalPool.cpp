// OWNER: WP2
#include "PedalPool.h"
#include "PedalRegistry.h"
#include "Services/AssetStore.h"
#include <algorithm>
namespace luthier::reference
{
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
void PedalPool::prepare (double sampleRate, int maxBlock, const std::vector<PedalInstance>& captured)
{
    rate = sampleRate; block = maxBlock; objects.clear(); preparedExtras = captured;
}
std::shared_ptr<PedalRuntime> PedalPool::acquire (const PedalInstance& instance, juce::String& error, const PedalRuntime* active)
{
    if (instance.missing) return {};
    const auto* desc = PedalRegistry::find (instance.modelId.c_str());
    if (desc == nullptr) { error = "Unknown pedal model: " + juce::String (instance.modelId); return {}; }
    auto extras = instance.extras.clone();
    if (auto* object = extras.getDynamicObject()) object->removeProperty ("midi");
    const auto extrasKey = juce::JSON::toString (extras);
    for (auto& weak : objects)
        if (auto existing = weak.lock())
        {
            bool matches = (! active || existing.get() == active) && existing->identity == instance.instanceId && existing->model == instance.modelId
                && existing->revision == instance.modelRevision && existing->extrasKey == extrasKey;
            for (int i = 0; matches && i < desc->numParams; ++i)
                if (desc->params[i].has (PF_Config) && existing->config[static_cast<size_t> (i)] != instance.params[static_cast<size_t> (i)])
                    matches = false;
            if (matches) return existing;
        }
    auto loadExtras = extras.clone();
    for (auto saved = preparedExtras.begin(); saved != preparedExtras.end(); ++saved)
        if (saved->instanceId == instance.instanceId && saved->modelId == instance.modelId && saved->modelRevision == instance.modelRevision)
        { loadExtras = saved->extras.clone(); preparedExtras.erase (saved); break; }
    if (active && active->identity == instance.instanceId && active->model == instance.modelId
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
    result->assetOwner = assets;
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
    if (latency < 0 || latency > kMaxLatencySamp) { error = "Invalid pedal latency"; return {}; }
    for (auto& line : result->dry) line.prepare (latency);
    result->enabled = result->modelEnabled = instance.enabled;
    result->modelParams = instance.params;
    result->inputGain = result->modelInputGain = std::pow (10.f, instance.inDb / 20.f);
    result->outputGain = result->modelOutputGain = std::pow (10.f, instance.outDb / 20.f);
    result->bypassSamples = std::max (1.0, rate * 0.005);
    result->foot.prepare (rate); result->foot.reset (instance.enabled);
    objects.emplace_back (result);
    return result;
}
void PedalPool::reclaim()
{
    objects.erase (std::remove_if (objects.begin(), objects.end(), [] (const auto& p) { return p.expired(); }), objects.end());
}
}
