// OWNER: WP2
#include "TopologyCompiler.h"
#include <cmath>
namespace luthier::reference
{
namespace
{
float gain (float db) { return std::pow (10.f, db / 20.f); }
bool sameStructure (const RenderPlan& a, const RenderPlan& b)
{
    const auto& x = a.topology; const auto& y = b.topology;
    if (x.mode != y.mode || x.n != y.n || x.numNodes != y.numNodes || a.latency != b.latency) return false;
    for (int i = 0; i < kMaxSlots; ++i)
        if (a.slots[static_cast<size_t> (i)].get() != b.slots[static_cast<size_t> (i)].get()) return false;
    for (int i = 0; i < x.n; ++i)
        if (x.order[i] != y.order[i] || x.placement[i] != y.placement[i]) return false;
    for (int i = 0; i < x.numNodes; ++i)
        if (x.nodes[i].kind != y.nodes[i].kind || x.nodes[i].slot != y.nodes[i].slot
            || x.nodes[i].lane != y.nodes[i].lane || x.nodes[i].aux != y.nodes[i].aux
            || x.nodes[i].i0 != y.nodes[i].i0) return false;
    return true;
}
}
std::unique_ptr<RenderPlan> TopologyCompiler::compile (const std::vector<PedalInstance>& instances,
    const BoardSettings& settings, PedalPool& pool, uint32_t version, const RenderPlan* previous, juce::String& error)
{
    error.clear();
    const auto reject = [&] (const char* message) -> std::unique_ptr<RenderPlan> { error = message; return {}; };
    if (instances.size() > kMaxSlots) return reject ("Board exceeds 16 pedal slots");
    if (settings.fb.on) return reject ("Feedback routing is not implemented yet");
    if (settings.mode == Mode::Loop || settings.mode == Mode::AB)
        return reject ("This routing mode is not implemented yet");
    if (settings.reampEnable) return reject ("Sidechain reamp requires host routing integration");
    for (const auto& loop : settings.loops)
        if (loop.used) return reject ("Send/return routing is not implemented yet");
    auto result = std::make_unique<RenderPlan>();
    result->settings = settings;
    auto& t = result->topology;
    t.version = version; t.mode = settings.mode;
    t.numLanes = static_cast<uint8_t> (settings.mode == Mode::Parallel ? settings.lanes.numLanes : 1);
    if (t.numLanes < 1 || t.numLanes > kMaxBranches) return reject ("Invalid lane count");
    std::array<bool, kMaxSlots> occupied {};
    for (const auto& instance : instances)
    {
        if (instance.slot < 0 || instance.slot >= kMaxSlots || occupied[static_cast<size_t> (instance.slot)])
            return reject ("Invalid or duplicate pedal slot");
        occupied[static_cast<size_t> (instance.slot)] = true;
        if (settings.mode == Mode::Parallel && (instance.lane < 0 || instance.lane >= t.numLanes))
            return reject ("Pedal lane exceeds active lane count");
        auto object = pool.acquire (instance, error);
        if (! object && ! instance.missing) return {};
        const auto slot = static_cast<size_t> (instance.slot);
        result->slots[slot] = object;
        t.pedals[slot] = object ? object->pedal.get() : nullptr;
        t.slotLatency[slot] = object ? object->pedal->getLatencySamples() : 0;
        if (object) t.cpuUnits += object->pedal->desc().cpuUnits;
        t.order[t.n] = static_cast<uint8_t> (slot);
        t.placement[t.n++] = static_cast<uint8_t> (instance.placement);
        auto& control = result->controls[slot];
        control.enabled = instance.enabled; control.params = instance.params;
        control.inputGain = gain (instance.inDb); control.outputGain = gain (instance.outDb);
        const int lane = settings.mode == Mode::Parallel ? instance.lane : 0;
        auto& laneList = t.lanes[lane]; laneList.slots[laneList.count++] = static_cast<uint8_t> (slot);
    }
    const auto emit = [&] (NodeKind kind, int slot, int lane, int aux, int integer, float f0, float f1)
    {
        auto& node = t.nodes[t.numNodes++]; node.kind = kind;
        node.slot = static_cast<uint8_t> (slot); node.lane = static_cast<uint8_t> (lane);
        node.aux = static_cast<uint8_t> (aux); node.i0 = integer; node.f0 = f0; node.f1 = f1;
    };
    for (int segment = 0; segment < kNumPlacements; ++segment)
    {
        t.segBegin[segment] = t.numNodes;
        std::array<int, kMaxBranches> latency {};
        bool present = false;
        for (const auto& instance : instances)
            if (static_cast<int> (instance.placement) == segment)
            {
                present = true;
                const int lane = settings.mode == Mode::Parallel ? instance.lane : 0;
                latency[static_cast<size_t> (lane)] += t.slotLatency[instance.slot];
            }
        if (! present) { t.segEnd[segment] = t.numNodes; continue; }
        const int longest = *std::max_element (latency.begin(), latency.begin() + t.numLanes);
        result->latency[static_cast<size_t> (segment)] = longest;
        t.latencyTotal += longest;
        if (t.latencyTotal > kMaxLatencySamp) return reject ("Board latency exceeds 8192 samples");
        if (settings.mode == Mode::Parallel) emit (NodeKind::Split, 0, 0, t.numLanes, 0, 1.f, 0.f);
        for (int lane = 0; lane < t.numLanes; ++lane)
        {
            for (const auto& instance : instances)
                if (static_cast<int> (instance.placement) == segment
                    && (settings.mode != Mode::Parallel || instance.lane == lane))
                    emit (NodeKind::Pedal, instance.slot, lane, 0, 0, 1.f, 0.f);
            if (settings.mode == Mode::Parallel)
            {
                const int delay = longest - latency[static_cast<size_t> (lane)];
                t.laneDelay[lane] = std::max (t.laneDelay[lane], delay);
                emit (NodeKind::Delay, 0, lane, lane, delay, 1.f, 0.f);
                emit (NodeKind::LaneGain, 0, lane, 0, 0,
                    gain (settings.lanes.gainDb[static_cast<size_t> (lane)]) * (settings.lanes.phaseInvert[static_cast<size_t> (lane)] ? -1.f : 1.f), 0.f);
            }
        }
        if (settings.mode == Mode::Parallel)
        {
            const float norm = settings.lanes.norm == ParallelNorm::Sqrt ? 1.f / std::sqrt (static_cast<float> (t.numLanes))
                : settings.lanes.norm == ParallelNorm::Unity ? 1.f / t.numLanes : 1.f;
            emit (NodeKind::Sum, 0, 0, t.numLanes, 0, norm, 0.f);
        }
        emit (NodeKind::Limiter, 0, 0, 0, settings.limiterOn ? 1 : 0, gain (settings.limiterCeilDb), 0.f);
        t.segEnd[segment] = t.numNodes;
    }
    t.dryDelay = t.latencyTotal;
    result->structure = previous && sameStructure (*previous, *result) ? previous->structure : version;
    return result;
}
}
