// OWNER: WP2
#include "PedalPool.h"
#include "PedalRegistry.h"
#include <algorithm>
namespace luthier::reference
{
void PedalRuntime::reset() noexcept
{
    pedal->reset();
    for (auto& line : dry) line.reset();
    enableMix = 0.0;
}
void PedalPool::prepare (double sampleRate, int maxBlock)
{
    rate = sampleRate; block = maxBlock; objects.clear();
}
std::shared_ptr<PedalRuntime> PedalPool::acquire (const PedalInstance& instance, juce::String& error)
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
            bool matches = existing->identity == instance.instanceId && existing->model == instance.modelId
                && existing->revision == instance.modelRevision && existing->extrasKey == extrasKey;
            for (int i = 0; matches && i < desc->numParams; ++i)
                if (desc->params[i].has (PF_Config) && existing->config[static_cast<size_t> (i)] != instance.params[static_cast<size_t> (i)])
                    matches = false;
            if (matches) return existing;
        }
    auto result = std::make_shared<PedalRuntime>();
    result->pedal = PedalRegistry::make (instance.modelId.c_str());
    if (! result->pedal) { error = "Pedal factory failed"; return {}; }
    result->identity = instance.instanceId; result->model = instance.modelId;
    result->revision = instance.modelRevision; result->config = instance.params; result->extrasKey = extrasKey;
    PedalServices services; services.sampleRate = rate; services.maxBlock = block;
    services.instanceSeed = static_cast<uint32_t> (instance.instanceId.hash());
    result->pedal->setServices (services);
    for (int i = 0; i < desc->numParams; ++i)
        result->pedal->setParam (i, instance.params[static_cast<size_t> (i)]);
    result->pedal->prepare (rate, block, 2);
    result->pedal->loadExtra (extras);
    result->pedal->setEnabled (instance.enabled);
    const int latency = result->pedal->getLatencySamples();
    if (latency < 0 || latency > kMaxLatencySamp) { error = "Invalid pedal latency"; return {}; }
    for (auto& line : result->dry) line.prepare (latency);
    result->enabled = instance.enabled;
    objects.emplace_back (result);
    return result;
}
void PedalPool::reclaim()
{
    objects.erase (std::remove_if (objects.begin(), objects.end(), [] (const auto& p) { return p.expired(); }), objects.end());
}
}
