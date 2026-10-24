// OWNER: WP1
#include "PedalRegistry.h"
#include <cstring>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "BoardTypes.h"
#include "Registration.h"

namespace luthier::reference
{

namespace
{
    struct Entry
    {
        PedalDesc desc;          // copy; `params` and the strings stay static storage owned by the model
        PedalFactory factory = nullptr;
        LaneMap lanes;
        PreparedMemoryEstimator memory = nullptr;
    };

    // Descriptors live in a deque so the pointers handed out by find() / all()
    // stay valid while later models are added.
    struct State
    {
        std::deque<Entry> entries;
        std::vector<const PedalDesc*> view;
        std::unordered_map<std::string, size_t> byId;
        std::unordered_map<std::string, size_t> byPrefix;
        bool registered = false;
    };

    State& state()
    {
        static State s;
        return s;
    }

    std::recursive_mutex& stateMutex()
    {
        static std::recursive_mutex m;
        return m;
    }

    bool isSnake (const char* s) noexcept
    {
        if (s == nullptr || *s == 0)
            return false;
        for (; *s != 0; ++s)
            if (! ((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '_'))
                return false;
        return true;
    }

    bool fail (juce::String* reason, const juce::String& text)
    {
        if (reason != nullptr)
            *reason = text;
        return false;
    }

    const LaneMap& emptyLaneMap()
    {
        static const LaneMap empty;
        return empty;
    }
}

//==============================================================================
bool PedalRegistry::validate (const PedalDesc& d, juce::String* reason)
{
    if (d.modelId == nullptr || *d.modelId == 0)
        return fail (reason, "empty modelId");
    const juce::String id (d.modelId);
    if (d.displayName == nullptr || *d.displayName == 0)
        return fail (reason, id + ": empty displayName");
    if (d.revision < 1)
        return fail (reason, id + ": revision must be >= 1");
    if (d.paramPrefix == nullptr || *d.paramPrefix == 0)
        return fail (reason, id + ": empty paramPrefix");

    const size_t prefixLen = std::strlen (d.paramPrefix);
    if (! isSnake (d.paramPrefix) || d.paramPrefix[prefixLen - 1] != '_')
        return fail (reason, id + ": paramPrefix must be snake_case and end with '_'");

    if (d.numParams < 0 || d.numParams > kMaxParams)
        return fail (reason, id + ": numParams out of range");
    if (d.numParams > 0 && d.params == nullptr)
        return fail (reason, id + ": params is null");
    if (d.maxLatencySamples < 0 || d.maxLatencySamples > kMaxLatencySamp)
        return fail (reason, id + ": maxLatencySamples out of range");

    int engage = 0;
    for (int i = 0; i < d.numParams; ++i)
    {
        const auto& p = d.params[i];
        if (p.id == nullptr || std::strncmp (p.id, d.paramPrefix, prefixLen) != 0 || p.id[prefixLen] == 0)
            return fail (reason, id + ": param " + juce::String (i) + " id must start with the prefix");
        if (! isSnake (p.id))
            return fail (reason, id + ": param id not snake_case: " + juce::String (p.id));
        if (p.label == nullptr)
            return fail (reason, id + ": param " + juce::String (p.id) + " has no label");

        for (int j = 0; j < i; ++j)
            if (std::strcmp (d.params[j].id, p.id) == 0)
                return fail (reason, id + ": duplicate param id " + juce::String (p.id));

        switch (p.kind)
        {
            case ParamKind::Continuous:
            case ParamKind::Tempo:
                if (! (p.min < p.max))
                    return fail (reason, id + ": " + juce::String (p.id) + " needs min < max");
                break;
            case ParamKind::Discrete:
            case ParamKind::Enum:
                if (p.steps < 2)
                    return fail (reason, id + ": " + juce::String (p.id) + " needs steps >= 2");
                if (p.kind == ParamKind::Enum && p.choices == nullptr)
                    return fail (reason, id + ": " + juce::String (p.id) + " is an Enum without choices");
                break;
            case ParamKind::Toggle:
            case ParamKind::Momentary:
            case ParamKind::Trigger:
                if (p.min != 0.0f || p.max != 1.0f)
                    return fail (reason, id + ": " + juce::String (p.id) + " switch params are 0..1");
                break;
        }

        if (! (p.def >= std::min (p.min, p.max) && p.def <= std::max (p.min, p.max)))
            return fail (reason, id + ": " + juce::String (p.id) + " default outside range");

        if (p.has (PF_Engage))
        {
            ++engage;
            if (p.kind != ParamKind::Toggle)
                return fail (reason, id + ": the Engage param must be a Toggle");
        }
    }

    if (engage > 1)
        return fail (reason, id + ": more than one Engage param");

    return true;
}

LaneMap PedalRegistry::buildLaneMap (const PedalDesc& d)
{
    LaneMap map;
    for (int i = 0; i < d.numParams && i < kMaxParams; ++i)
    {
        const auto& p = d.params[i];
        if (p.has (PF_Engage))
        {
            if (map.engageParam < 0)
                map.engageParam = i;
            continue;
        }
        if (p.isLaneExcluded())
            continue;

        if (map.numLanes < kMaxLaneParams)
        {
            map.laneToParam[map.numLanes] = i;
            map.paramToLane[i] = map.numLanes;
            ++map.numLanes;
        }
        else
        {
            ++map.numOverflow;
        }
    }
    return map;
}

int PedalRegistry::findParam (const PedalDesc& d, const char* paramId)
{
    if (paramId == nullptr)
        return -1;
    for (int i = 0; i < d.numParams; ++i)
        if (std::strcmp (d.params[i].id, paramId) == 0)
            return i;
    return -1;
}

//==============================================================================
bool PedalRegistry::add (const PedalDesc& desc, PedalFactory factory, PreparedMemoryEstimator memory)
{
    // Quiet on purpose: T-REG feeds it invalid descriptors. Register*.cpp files
    // check the result; RefPedalApi.registryContentValid fails loudly in CI.
    if (factory == nullptr || ! validate (desc, nullptr))
        return false;

    const std::lock_guard<std::recursive_mutex> lock (stateMutex());
    auto& s = state();

    if (s.byId.count (desc.modelId) != 0 || s.byPrefix.count (desc.paramPrefix) != 0)
        return false;   // duplicate modelId or paramPrefix

    s.entries.push_back ({ desc, factory, buildLaneMap (desc), memory });
    const size_t index = s.entries.size() - 1;
    s.byId[desc.modelId] = index;
    s.byPrefix[desc.paramPrefix] = index;
    s.view.push_back (&s.entries.back().desc);
    return true;
}

PreparedMemoryRequirement PedalRegistry::memoryRequirement (const char* modelId, double rate, int block,
    int policy, const float* params)
{
    PreparedMemoryEstimator estimate = nullptr;
    {
        const std::lock_guard<std::recursive_mutex> lock (stateMutex());
        const auto& s = state();
        const auto found = s.byId.find (modelId ? modelId : "");
        if (found != s.byId.end()) estimate = s.entries[found->second].memory;
    }
    return estimate ? estimate (rate, block, policy, params) : PreparedMemoryRequirement {};
}

void PedalRegistry::registerAll()
{
    const std::lock_guard<std::recursive_mutex> lock (stateMutex());
    auto& s = state();
    if (s.registered)
        return;
    s.registered = true;   // set first: a register*() that calls registerAll() must not recurse

    // Catalog order. Generic pedals first, then branded models, prototypes last.
    registerGenericA();
    registerGenericB();
    registerGenericC();
    registerDriveA();
    registerDriveB();
    registerMuffs();
    registerModDelay();
    registerLoopers();
    registerRc5();
    registerPrototypes();
}

std::unique_ptr<IPedal> PedalRegistry::make (const char* modelId)
{
    if (modelId == nullptr)
        return nullptr;
    PedalFactory factory = nullptr;
    {
        const std::lock_guard<std::recursive_mutex> lock (stateMutex());
        auto& s = state();
        const auto it = s.byId.find (modelId);
        if (it == s.byId.end())
            return nullptr;
        factory = s.entries[it->second].factory;
    }
    return factory();
}

const PedalDesc* PedalRegistry::find (const char* modelId)
{
    if (modelId == nullptr)
        return nullptr;
    const std::lock_guard<std::recursive_mutex> lock (stateMutex());
    auto& s = state();
    const auto it = s.byId.find (modelId);
    return it == s.byId.end() ? nullptr : &s.entries[it->second].desc;
}

juce::Span<const PedalDesc* const> PedalRegistry::all()
{
    const std::lock_guard<std::recursive_mutex> lock (stateMutex());
    auto& s = state();
    return { s.view.data(), s.view.size() };
}

const LaneMap& PedalRegistry::laneMap (const char* modelId)
{
    if (modelId == nullptr)
        return emptyLaneMap();
    const std::lock_guard<std::recursive_mutex> lock (stateMutex());
    auto& s = state();
    const auto it = s.byId.find (modelId);
    return it == s.byId.end() ? emptyLaneMap() : s.entries[it->second].lanes;
}

//==============================================================================
struct PedalRegistry::TestScope::Saved
{
    State previous;
};

PedalRegistry::TestScope::TestScope() : saved (std::make_unique<Saved>())
{
    const std::lock_guard<std::recursive_mutex> lock (stateMutex());
    std::swap (saved->previous, state());   // leaves an empty registry with registered == false
}

PedalRegistry::TestScope::~TestScope()
{
    const std::lock_guard<std::recursive_mutex> lock (stateMutex());
    std::swap (saved->previous, state());
    // The view holds pointers into the deque. Moving a std::deque keeps element
    // addresses valid, but swapping State objects must preserve them: rebuild
    // the view from the restored entries to be safe.
    auto& s = state();
    s.view.clear();
    for (auto& e : s.entries)
        s.view.push_back (&e.desc);
}

} // namespace luthier::reference
