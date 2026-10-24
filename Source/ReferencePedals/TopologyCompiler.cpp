// OWNER: WP2
#include "TopologyCompiler.h"
#include "PedalRegistry.h"
#include "CpuBudget.h"
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
    const BoardSettings& settings, PedalPool& pool, uint32_t version, const RenderPlan* previous, juce::String& error, const BoardCommand* intent, bool hostEcho)
{
    error.clear();
    const auto reject = [&] (const char* message) -> std::unique_ptr<RenderPlan> { error = message; return {}; };
    if (instances.size() > kMaxSlots) return reject ("Board exceeds 16 pedal slots");
    if (estimatedCpuUnits (instances) > static_cast<uint64_t> (settings.cpuCapPct))
        return reject ("Board CPU budget reached (estimated descriptor units)");
    auto result = std::make_unique<RenderPlan>();
    result->settings = settings;
    result->feedbackRearm = ! previous || (! hostEcho && ! previous->settings.fb.on && settings.fb.on) ? version : previous->feedbackRearm;
    result->feedbackKill = previous ? previous->feedbackKill : 0;
    if (! hostEcho && settings.fb.kill && (! previous || ! previous->settings.fb.kill)) result->feedbackKill = version;
    const bool branched = settings.mode == Mode::Parallel || settings.mode == Mode::AB;
    result->abResolved = settings.abSelect == AbSelect::Toggle ? (previous ? previous->abResolved : AbSelect::A) : settings.abSelect;
    if (settings.abSelect == AbSelect::Toggle && intent)
        if (const auto* scalar = std::get_if<cmd::SetBoardParam> (intent); scalar && scalar->key == "abSelect")
            result->abResolved = previous && previous->abResolved == AbSelect::A ? AbSelect::B : AbSelect::A;
    if (previous) result->boardSerials = previous->boardSerials;
    if (! previous || ! intent) result->boardSerials.fill (version);
    else if (! hostEcho)
    {
        if (settings.enabled != previous->settings.enabled || std::holds_alternative<cmd::SetBoardEnabled> (*intent)) result->boardSerials[0] = version;
        if (settings.mix != previous->settings.mix || std::holds_alternative<cmd::SetMix> (*intent)) result->boardSerials[1] = version;
        const auto* scalar = std::get_if<cmd::SetBoardParam> (intent);
        if (settings.tempoBpm != previous->settings.tempoBpm || (scalar && scalar->key == "tempoBpm")) result->boardSerials[2] = version;
    }
    result->extended = compileExtendedControls (settings, instances, previous ? &previous->extended : nullptr, intent, hostEcho, version, result->abResolved);
    auto& t = result->topology;
    t.version = version; t.mode = settings.mode;
    t.numLanes = static_cast<uint8_t> (settings.mode == Mode::AB ? 2 : branched ? settings.lanes.numLanes : 1);
    if (t.numLanes < 1 || t.numLanes > kMaxBranches) return reject ("Invalid lane count");
    std::array<int, kMaxSlots> loopForPosition;
    loopForPosition.fill (-1);
    for (int loopIndex = 0; loopIndex < kMaxLoops; ++loopIndex)
    {
        const auto& loop = settings.loops[static_cast<size_t> (loopIndex)];
        if (! loop.used) continue;
        if (settings.mode != Mode::Series && settings.mode != Mode::Loop)
            return reject ("Send/return loops require series or loop mode");
        const auto find = [&] (const InstanceId& id) { for (size_t i = 0; i < instances.size(); ++i) if (instances[i].instanceId == id) return static_cast<int> (i); return -1; };
        const int first = find (loop.first), last = find (loop.last);
        if (first < 0 || last < first) return reject ("Loop endpoints must exist in forward order");
        auto& compiled = t.loops[loopIndex];
        compiled.used = true; compiled.enabled = loop.enabled; compiled.kind = loop.kind;
        compiled.firstPos = static_cast<uint8_t> (first); compiled.lastPos = static_cast<uint8_t> (last);
        compiled.blend = loop.blend; compiled.sendGain = gain (loop.sendDb); compiled.returnGain = gain (loop.returnDb);
        for (int position = first; position <= last; ++position)
        {
            if (instances[static_cast<size_t> (position)].placement != instances[static_cast<size_t> (first)].placement)
                return reject ("A send/return loop cannot cross placement boundaries");
            if (loopForPosition[static_cast<size_t> (position)] >= 0) return reject ("Send/return loops cannot overlap");
            loopForPosition[static_cast<size_t> (position)] = loopIndex;
        }
    }
    if (settings.fb.on || (! settings.fb.src.isNull() && ! settings.fb.dst.isNull()))
    {
        int source = -1, destination = -1;
        for (size_t position = 0; position < instances.size(); ++position)
        {
            if (instances[position].instanceId == settings.fb.src) source = static_cast<int> (position);
            if (instances[position].instanceId == settings.fb.dst) destination = static_cast<int> (position);
        }
        if (source < 0 || destination < 0 || destination > source)
        { if (settings.fb.on) return reject ("Feedback endpoints must exist with destination before source"); }
        else
        {
        const auto& src = instances[static_cast<size_t> (source)]; const auto& dst = instances[static_cast<size_t> (destination)];
        if (src.placement != dst.placement || (branched && src.lane != dst.lane)
            || loopForPosition[static_cast<size_t> (source)] != loopForPosition[static_cast<size_t> (destination)])
            return reject ("Feedback endpoints must share a placement and routing lane");
        result->feedbackAvailable = true; t.fb.on = settings.fb.on; t.fb.srcSlot = static_cast<uint8_t> (src.slot); t.fb.dstSlot = static_cast<uint8_t> (dst.slot);
        t.fb.gain = std::min (settings.fb.gain, settings.fb.runawayOk ? 1.05f : 0.98f);
        t.fb.lpHz = settings.fb.lpHz; t.fb.runawayOk = settings.fb.runawayOk;
        }
    }
    std::array<bool, kMaxSlots> occupied {};
    for (const auto& instance : instances)
    {
        if (instance.slot < 0 || instance.slot >= kMaxSlots || occupied[static_cast<size_t> (instance.slot)])
            return reject ("Invalid or duplicate pedal slot");
        occupied[static_cast<size_t> (instance.slot)] = true;
        if (branched && (instance.lane < 0 || instance.lane >= t.numLanes))
            return reject ("Pedal lane exceeds active lane count");
        auto object = pool.acquire (instance, error, previous ? previous->slots[static_cast<size_t> (instance.slot)].get() : nullptr);
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
        control.footMode = static_cast<FootMode> (std::clamp (static_cast<int> (instance.extras["midi"]["fsMode"]), 0, 2));
        if (object)
            for (int param = 0; param < object->pedal->desc().numParams; ++param)
            {
                const auto& descriptor = object->pedal->desc().params[param];
                if (juce::String (descriptor.id).endsWith ("_fs_mode"))
                    control.footMode = static_cast<FootMode> (std::clamp (static_cast<int> (std::lround (descriptor.fromNormalised (instance.params[static_cast<size_t> (param)]))), 0, 2));
            }
        if (object)
        {
            const auto lanes = PedalRegistry::buildLaneMap (object->pedal->desc());
            for (int lane = 0; lane < lanes.numLanes; ++lane) control.laneToParam[static_cast<size_t> (lane)] = lanes.laneToParam[lane];
        }
        control.inputGain = gain (instance.inDb); control.outputGain = gain (instance.outDb);
        const bool sameInstance = previous && previous->slots[slot] && object
            && previous->slots[slot]->identity == object->identity && previous->slots[slot]->model == object->model;
        if (sameInstance && intent)
        {
            const auto& old = previous->controls[slot];
            control.enabledSerial = old.enabledSerial; control.inputSerial = old.inputSerial; control.outputSerial = old.outputSerial;
            control.paramSerials = old.paramSerials;
            if (! hostEcho)
            {
            if (control.enabled != old.enabled) control.enabledSerial = version;
            if (control.inputGain != old.inputGain) control.inputSerial = version;
            if (control.outputGain != old.outputGain) control.outputSerial = version;
            for (int i = 0; i < kMaxParams; ++i)
                if (control.params[static_cast<size_t> (i)] != old.params[static_cast<size_t> (i)]) control.paramSerials[static_cast<size_t> (i)] = version;
            if (const auto* write = std::get_if<cmd::SetParam> (intent); write && write->id == instance.instanceId)
                control.paramSerials[static_cast<size_t> (write->paramIdx)] = version;
            if (const auto* write = std::get_if<cmd::SetEnabled> (intent); write && write->id == instance.instanceId)
                control.enabledSerial = version;
            if (const auto* write = std::get_if<cmd::SetTrim> (intent); write && write->id == instance.instanceId)
                control.inputSerial = control.outputSerial = version;
            }
        }
        else
        {
            control.enabledSerial = control.inputSerial = control.outputSerial = version;
            control.paramSerials.fill (version);
        }
        const int lane = branched ? instance.lane : 0;
        auto& laneList = t.lanes[lane]; laneList.slots[laneList.count++] = static_cast<uint8_t> (slot);
    }
    const auto emit = [&] (NodeKind kind, int slot, int lane, int aux, int integer, float f0, float f1)
    {
        auto& node = t.nodes[t.numNodes++]; node.kind = kind;
        node.slot = static_cast<uint8_t> (slot); node.lane = static_cast<uint8_t> (lane);
        node.aux = static_cast<uint8_t> (aux); node.i0 = integer; node.f0 = f0; node.f1 = f1;
    };
    const auto emitPedal = [&] (int slot, int lane)
    {
        if (result->feedbackAvailable && slot == t.fb.dstSlot) emit (NodeKind::FbReturn, 0, lane, 0, 0, 0.f, 0.f);
        emit (NodeKind::Pedal, slot, lane, 0, 0, 1.f, 0.f);
        if (result->feedbackAvailable && slot == t.fb.srcSlot) emit (NodeKind::FbSend, 0, lane, 0, 0, 0.f, 0.f);
    };
    for (int segment = 0; segment < kNumPlacements; ++segment)
    {
        t.segBegin[segment] = t.numNodes;
        std::array<int, kMaxBranches> latency {};
        bool present = settings.reampEnable && segment == 0 && ! instances.empty();
        for (const auto& instance : instances)
            if (static_cast<int> (instance.placement) == segment)
            {
                present = true;
                const int lane = branched ? instance.lane : 0;
                latency[static_cast<size_t> (lane)] += t.slotLatency[instance.slot];
            }
        if (! present) { t.segEnd[segment] = t.numNodes; continue; }
        const int longest = *std::max_element (latency.begin(), latency.begin() + t.numLanes);
        result->latency[static_cast<size_t> (segment)] = longest;
        t.latencyTotal += longest;
        if (t.latencyTotal > kMaxLatencySamp) return reject ("Board latency exceeds 8192 samples");
        if (branched) emit (NodeKind::Split, 0, 0, t.numLanes, 0, 1.f, 0.f);
        for (int lane = 0; lane < t.numLanes; ++lane)
        {
            for (size_t position = 0; position < instances.size(); ++position)
            {
                const auto& instance = instances[position];
                if (static_cast<int> (instance.placement) != segment
                    || (branched && instance.lane != lane)) continue;
                const int loopIndex = loopForPosition[position];
                if (loopIndex >= 0)
                {
                    const auto& loop = t.loops[loopIndex];
                    if (position == loop.firstPos)
                    {
                        int loopLatency = 0;
                        for (int pos = loop.firstPos; pos <= loop.lastPos; ++pos) loopLatency += t.slotLatency[instances[static_cast<size_t> (pos)].slot];
                        emit (NodeKind::SendTap, 0, 0, loopIndex, loopLatency, loop.enabled ? loop.sendGain : 0.f, 0.f);
                    }
                    emitPedal (instance.slot, kBusLoopBase + loopIndex);
                    if (position == loop.lastPos)
                        emit (NodeKind::ReturnInject, 0, 0, loopIndex, 0,
                            ! loop.enabled ? 0.f : loop.kind == LoopKind::Parallel ? loop.blend : 1.f, loop.returnGain);
                }
                else emitPedal (instance.slot, lane);
            }
            if (branched)
            {
                const int delay = longest - latency[static_cast<size_t> (lane)];
                t.laneDelay[lane] = std::max (t.laneDelay[lane], delay);
                emit (NodeKind::Delay, 0, lane, lane, delay, 1.f, 0.f);
                emit (NodeKind::LaneGain, 0, lane, 0, 0,
                    gain (settings.lanes.gainDb[static_cast<size_t> (lane)]) * (settings.lanes.phaseInvert[static_cast<size_t> (lane)] ? -1.f : 1.f), 0.f);
            }
        }
        if (branched)
        {
            const float norm = settings.lanes.norm == ParallelNorm::Sqrt ? 1.f / std::sqrt (static_cast<float> (t.numLanes))
                : settings.lanes.norm == ParallelNorm::Unity ? 1.f / t.numLanes : 1.f;
            emit (NodeKind::Sum, 0, 0, t.numLanes,
                settings.mode == Mode::AB ? (settings.abOut == AbOutMode::Dual ? 2 : 1) : 0, norm,
                result->abResolved == AbSelect::A ? 0.f : result->abResolved == AbSelect::B ? 1.f : 0.5f);
        }
        emit (NodeKind::Limiter, 0, 0, 0, settings.limiterOn ? 1 : 0, gain (settings.limiterCeilDb), 0.f);
        t.segEnd[segment] = t.numNodes;
    }
    t.dryDelay = t.latencyTotal;
    result->structure = previous && sameStructure (*previous, *result) ? previous->structure : version;
    return result;
}
}
