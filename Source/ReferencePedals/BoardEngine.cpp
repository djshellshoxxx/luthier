// OWNER: WP2
#include "BoardEngine.h"
#include "BoardLimiter.h"
#include "PedalRegistry.h"
#include "Routing/RoutingNodes.h"
#include <limits>
namespace luthier::reference
{
static_assert (std::atomic<const RenderPlan*>::is_always_lock_free, "Audio plan pointer must be lock-free");
static_assert (std::atomic<uint32_t>::is_always_lock_free, "Audio epoch must be lock-free");
BoardEngine::BoardEngine (BoardModel& source) : model (source)
{
    PedalRegistry::registerAll();
    prepare (48000, 4096);
    model.setCompiler ([this] (const auto& instances, const auto& settings, auto& info, auto& error)
        { return publish (instances, settings, info, error); });
}
BoardEngine::~BoardEngine() { model.setCompiler ({}); }
void BoardEngine::prepare (double sampleRate, int maxBlock)
{
    releaseResources();
    rate = std::isfinite (sampleRate) && sampleRate >= 8000 ? sampleRate : 48000;
    block = std::max (1, maxBlock);
    dipSamples = std::max (1, static_cast<int> (rate * 0.006));
    mixAlpha = 1.0 - std::exp (-1.0 / (0.020 * rate));
    pool.prepare (rate, block);
    for (auto& segment : segments)
    {
        segment = {};
        for (auto& line : segment.dry) line.prepare (kMaxLatencySamp);
        for (auto& line : segment.lanes) line.prepare (kMaxLatencySamp);
    }
    std::vector<PedalInstance> instances; BoardSettings settings;
    model.snapshot (instances, settings);
    TopologyInfo info; juce::String error;
    publish (instances, settings, info, error);
}
void BoardEngine::releaseResources()
{
    live.store (nullptr, std::memory_order_release);
    for (auto& segment : segments) { segment.current = segment.pending = nullptr; }
    retired.reclaimAll(); latest.reset();
    audioEpoch.store (0, std::memory_order_release);
    running.store (false); targetActive.store (false); latencySamples.store (0);
}
bool BoardEngine::publish (const std::vector<PedalInstance>& instances, const BoardSettings& settings,
    TopologyInfo& info, juce::String& error)
{
    auto plan = TopologyCompiler::compile (instances, settings, pool, version + 1, latest.get(), error);
    if (! plan) return false;
    ++version;
    info.version = version; info.mode = settings.mode; info.pedalCount = plan->topology.n;
    info.latencyTotal = plan->topology.latencyTotal; info.dryDelay = plan->topology.dryDelay;
    info.cpuUnits = plan->topology.cpuUnits;
    const bool active = settings.enabled && plan->topology.n > 0;
    targetActive.store (active, std::memory_order_release);
    latencySamples.store (active && settings.reportLatency ? plan->topology.latencyTotal : 0, std::memory_order_release);
    auto old = std::move (latest); latest = std::move (plan);
    live.store (latest.get(), std::memory_order_release);
    if (old) { const auto oldVersion = old->topology.version; retired.retire (std::move (old), oldVersion); }
    reclaim();
    return true;
}
int BoardEngine::reclaim() noexcept
{
    const int freed = retired.reclaim (audioEpoch.load (std::memory_order_acquire));
    pool.reclaim(); return freed;
}
void BoardEngine::drainControlEcho()
{
    std::array<EngageEcho, kMaxSlots> last;
    std::array<bool, kMaxSlots> seen {};
    EngageEcho event;
    for (int i = 0; i < 256 && engageEcho.pop (event); ++i)
        if (event.slot < kMaxSlots) { last[event.slot] = event; seen[event.slot] = true; }
    for (int slot = 0; slot < kMaxSlots; ++slot)
        if (seen[static_cast<size_t> (slot)])
        {
            const auto& echo = last[static_cast<size_t> (slot)];
            const auto instance = model.getInstance (echo.identity);
            if (instance && instance->slot == slot && instance->enabled == echo.baseline && instance->enabled != echo.enabled)
                model.apply (cmd::SetEnabled { echo.identity, echo.enabled });
        }
}
void BoardEngine::acknowledge() noexcept
{
    uint32_t oldest = 0;
    for (const auto& segment : segments)
        if (segment.current && (oldest == 0 || static_cast<int32_t> (segment.current->topology.version - oldest) < 0))
            oldest = segment.current->topology.version;
    if (oldest) audioEpoch.store (oldest, std::memory_order_release);
}
BoardMeters BoardEngine::meters() const noexcept
{
    BoardMeters result; result.inPeakDb = inputPeak.load(); result.outPeakDb = outputPeak.load();
    result.latencySamples = latency(); result.sidechainAvailable = sidechainConnected.load();
    result.midiDropped = engageEcho.dropped(); return result;
}
void BoardEngine::beginEvents (const midi::RouteOut& routed) noexcept
{
    events = &routed;
    const auto* plan = live.load (std::memory_order_acquire);
    if (! plan) return;
    if (modelEnabled != plan->settings.enabled) effectiveEnabled = modelEnabled = plan->settings.enabled;
    if (modelMix != plan->settings.mix) effectiveMix = modelMix = plan->settings.mix;
    if (modelTempo != plan->settings.tempoBpm) effectiveTempo = modelTempo = plan->settings.tempoBpm;
    for (int index = 0; index < 3; ++index)
        if (hostAutomation.serials[static_cast<size_t> (index)] != 0
            && hostAutomation.serials[static_cast<size_t> (index)] != hostBoardSerials[static_cast<size_t> (index)])
        {
            hostBoardSerials[static_cast<size_t> (index)] = hostAutomation.serials[static_cast<size_t> (index)];
            const auto value = hostAutomation.values[static_cast<size_t> (index)];
            if (! std::isfinite (value)) continue;
            if (index == 0) effectiveEnabled = value >= 0.5f;
            if (index == 1) effectiveMix = std::clamp (value, 0.f, 1.f);
            if (index == 2) effectiveTempo = std::clamp (static_cast<double> (value), 20.0, 300.0);
        }
    if (effectiveEnabled && plan->topology.n > 0) running.store (true);
    blockEnabled = effectiveEnabled; blockMix = effectiveMix; blockTempo = effectiveTempo;
    for (int i = 0; i < routed.nWrites; ++i)
    {
        const auto& write = routed.writes[i];
        if (write.slot != midi::kBoardTargetSlot) continue;
        switch (static_cast<midi::BoardTarget> (write.paramIdx))
        {
            case midi::BoardTarget::Mix: effectiveMix = write.norm; break;
            case midi::BoardTarget::Tempo: effectiveTempo = 20.0 + 280.0 * write.norm; break;
            case midi::BoardTarget::Enabled:
                effectiveEnabled = write.norm >= 0.5f;
                if (effectiveEnabled && plan->topology.n > 0) running.store (true);
                break;
        }
    }
    latencySamples.store (effectiveEnabled && plan->settings.reportLatency ? plan->topology.latencyTotal : 0);
}
void BoardEngine::process (double* left, double* right, int n, Placement placement) noexcept
{
    if (! left || ! right || n <= 0 || ! isValidPlacement (static_cast<int> (placement))) return;
    const auto* newest = live.load (std::memory_order_acquire);
    if (! newest) return;
    // Empty segments do not need a transition and must not pin a retired plan.
    for (int p = 0; p < kNumPlacements; ++p)
    {
        auto& s = segments[static_cast<size_t> (p)];
        if (! s.current || (s.current->topology.segBegin[p] == s.current->topology.segEnd[p]
            && newest->topology.segBegin[p] == newest->topology.segEnd[p]))
        { s.current = newest; s.pending = nullptr; s.phase = Segment::Idle; }
    }
    bool localEnabled = events ? blockEnabled : newest->settings.enabled;
    float localMix = events ? blockMix : newest->settings.mix;
    double localTempo = events ? blockTempo : newest->settings.tempoBpm;
    const int p = static_cast<int> (placement);
    auto& s = segments[static_cast<size_t> (p)];
    if (resetRequested.exchange (false, std::memory_order_acq_rel))
    {
        for (int slot = 0; slot < kMaxSlots; ++slot)
            if (newest->slots[static_cast<size_t> (slot)]) newest->slots[static_cast<size_t> (slot)]->reset();
        for (auto& seg : segments)
        { for (auto& d : seg.dry) d.reset(); for (auto& d : seg.lanes) d.reset(); }
    }
    if (s.current != newest)
    {
        if (s.current->structure == newest->structure && s.phase == Segment::Idle)
            s.current = newest;
        else if (s.pending != newest)
        {
            s.pending = newest;
            if (s.phase != Segment::Out)
            { s.phase = Segment::Out; s.ramp = 0; s.rampStart = s.gain; }
        }
    }
    if (! localEnabled && ! running.load())
    { s.current = newest; s.pending = nullptr; s.phase = Segment::Idle; s.gain = 1.0; s.enable = 0.0; acknowledge(); return; }
    if (localEnabled && newest->topology.n > 0) running.store (true);
    double peakIn = 0, peakOut = 0;
    for (int offset = 0; offset < n;)
    {
        if (s.phase == Segment::Out && (s.ramp >= dipSamples || s.mix == 0.0))
        {
            s.current = s.pending; s.pending = nullptr;
            s.phase = Segment::In; s.ramp = 0; s.gain = 0.0;
        }
        const auto& plan = *s.current; const auto& topology = plan.topology;
        if (events)
            for (int event = 0; event < events->nWrites; ++event)
            {
                const auto& write = events->writes[event];
                if (write.offset != offset || write.slot != midi::kBoardTargetSlot) continue;
                switch (static_cast<midi::BoardTarget> (write.paramIdx))
                {
                    case midi::BoardTarget::Mix: localMix = write.norm; break;
                    case midi::BoardTarget::Tempo: localTempo = 20.0 + 280.0 * write.norm; break;
                    case midi::BoardTarget::Enabled: localEnabled = write.norm >= 0.5f; break;
                }
            }
        int count = std::min ({ kSubBlock, block, n - offset });
        if (events)
        {
            const auto split = [&] (int eventOffset) { if (eventOffset > offset) count = std::min (count, eventOffset - offset); };
            for (int event = 0; event < events->nWrites; ++event) split (events->writes[event].offset);
            for (int event = 0; event < events->nFoot; ++event) split (events->foot[event].offset);
            for (int event = 0; event < events->nTr; ++event) split (events->trOff[event]);
        }
        if (s.phase != Segment::Idle) count = std::min (count, dipSamples - s.ramp);
        const bool hasNodes = topology.segBegin[p] != topology.segEnd[p];
        if (! hasNodes && ! s.pending) { s.phase = Segment::Idle; s.gain = 1.0; offset += count; continue; }
        const bool hasSidechain = sidechain[0] && sidechain[1] && offset + count <= sidechainSamples;
        if (hasSidechain)
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < count; ++i)
                    sidechainScratch[static_cast<size_t> (c)][static_cast<size_t> (i)] = BoardLimiter::process (sidechain[c][offset + i], false, 1.0);
        const bool reamp = p == 0 && plan.settings.reampEnable && hasSidechain;
        const double reampGain = reamp ? std::pow (10.0, plan.settings.reampGainDb / 20.0) : 0.0;
        double dry[2][kSubBlock] {};
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < count; ++i)
            {
                double in = (c == 0 ? left : right)[offset + i];
                if (reamp) in = (plan.settings.reampReplace ? 0.0 : in)
                    + reampGain * sidechainScratch[static_cast<size_t> (c)][static_cast<size_t> (i)];
                peakIn = std::max (peakIn, std::abs (std::isfinite (in) ? in : 0.0));
                dry[c][i] = s.dry[static_cast<size_t> (c)].process (in, plan.latency[static_cast<size_t> (p)]);
                buses[0][static_cast<size_t> (c)][static_cast<size_t> (i)] = ! localEnabled && s.enable == 0.0 ? 0.0 : BoardLimiter::process (in, false, 1.0);
            }
        ExecCtx context; context.n = count; context.topo = &topology;
        context.pedals = topology.pedals; context.sampleRate = rate;
        for (int bus = 0; bus < kMaxBuses; ++bus)
            for (int c = 0; c < 2; ++c) context.bus[bus][c] = buses[static_cast<size_t> (bus)][static_cast<size_t> (c)].data();
        std::array<PedalNodeState, kMaxSlots> pedalStates;
        for (int index = topology.segBegin[p]; index < topology.segEnd[p]; ++index)
        {
            const auto& node = topology.nodes[index]; context.nodeIndex = index;
            if (node.kind == NodeKind::Pedal)
            {
                auto& state = pedalStates[node.slot]; state.runtime = plan.slots[node.slot].get(); state.control = &plan.controls[node.slot];
                context.state[index] = &state;
                if (state.runtime)
                {
                    auto& pedal = *state.runtime->pedal;
                    pedal.setSidechain (hasSidechain ? sidechainScratch[0].data() : nullptr,
                                        hasSidechain ? sidechainScratch[1].data() : nullptr, hasSidechain ? count : 0);
                    for (int param = 0; param < pedal.desc().numParams; ++param)
                        if (! pedal.desc().params[param].has (PF_Engage)
                            && state.runtime->modelParams[static_cast<size_t> (param)] != state.control->params[static_cast<size_t> (param)])
                        {
                            state.runtime->modelParams[static_cast<size_t> (param)] = state.control->params[static_cast<size_t> (param)];
                            pedal.setParam (param, state.control->params[static_cast<size_t> (param)]);
                        }
                    if (state.runtime->modelEnabled != state.control->enabled)
                    {
                        state.runtime->modelEnabled = state.runtime->enabled = state.control->enabled;
                        state.runtime->bypassSamples = std::max (1.0, 0.005 * rate);
                        state.runtime->foot.reset (state.control->enabled); pedal.setEnabled (state.control->enabled);
                    }
                    if (state.runtime->modelInputGain != state.control->inputGain)
                        state.runtime->inputGain = state.runtime->modelInputGain = state.control->inputGain;
                    if (state.runtime->modelOutputGain != state.control->outputGain)
                        state.runtime->outputGain = state.runtime->modelOutputGain = state.control->outputGain;
                    const int hostBase = 3 + 20 * node.slot;
                    if (! state.runtime->hostInitialised)
                    {
                        const auto& previous = hostSlotSources[node.slot];
                        if (! previous.identity.isNull() && (previous.identity != state.runtime->identity
                            || previous.modelId != pedal.desc().modelId))
                            for (int index = 0; index < 19; ++index)
                                state.runtime->hostSerials[static_cast<size_t> (index)] = hostAutomation.serials[static_cast<size_t> (hostBase + index)];
                        hostSlotSources[node.slot] = { state.runtime->identity, pedal.desc().modelId };
                        state.runtime->hostInitialised = true;
                    }
                    for (int index = 0; index < 19; ++index)
                    {
                        const auto serial = hostAutomation.serials[static_cast<size_t> (hostBase + index)];
                        if (serial == 0 || serial == state.runtime->hostSerials[static_cast<size_t> (index)]) continue;
                        state.runtime->hostSerials[static_cast<size_t> (index)] = serial;
                        const float value = hostAutomation.values[static_cast<size_t> (hostBase + index)];
                        if (! std::isfinite (value)) continue;
                        if (index == 0)
                        { state.runtime->enabled = value >= 0.5f; pedal.setEnabled (state.runtime->enabled); state.runtime->foot.reset (state.runtime->enabled); }
                        else if (index <= 16)
                        {
                            const int param = state.control->laneToParam[static_cast<size_t> (index - 1)];
                            if (param >= 0) pedal.setParam (param, std::clamp (value, 0.f, 1.f));
                        }
                        else if (index == 17) state.runtime->inputGain = std::pow (10.f, std::clamp (value, -12.f, 12.f) / 20.f);
                        else state.runtime->outputGain = std::pow (10.f, std::clamp (value, -12.f, 12.f) / 20.f);
                    }
                    auto time = blockTime;
                    if (localTempo != modelTempo) time.bpm = localTempo;
                    time.sample += offset; if (time.valid) time.ppq += offset * time.bpm / (60.0 * rate);
                    pedal.setTime (time);
                    if (events)
                    {
                        for (int event = 0; event < events->nTr; ++event)
                            if (events->trOff[event] == offset) pedal.onTransport (events->tr[event], offset);
                        for (int event = 0; event < events->nFoot; ++event)
                            if (events->footSlot[event] == node.slot && events->foot[event].offset == offset)
                            {
                                const auto& foot = events->foot[event];
                                pedal.onFoot (foot);
                                if (pedal.footControlsEngagement())
                                {
                                    const bool value = foot.press ? state.runtime->foot.press (blockTime.sample + offset)
                                        : state.runtime->foot.release (blockTime.sample + offset);
                                    if (value != state.runtime->enabled)
                                    {
                                        state.runtime->enabled = value; pedal.setEnabled (value);
                                        state.runtime->bypassSamples = std::max (1.0, rate * (12.0 - 10.0 * std::clamp (static_cast<double> (foot.speed), 0.0, 1.0)) * 0.001);
                                        engageEcho.push ({ state.runtime->identity, node.slot, value, state.runtime->modelEnabled });
                                    }
                                }
                            }
                        if (localEnabled)
                            for (int event = 0; event < events->nWrites; ++event)
                            {
                                const auto& write = events->writes[event];
                                if (write.slot != node.slot || write.offset != offset || write.paramIdx >= pedal.desc().numParams) continue;
                                if (pedal.desc().params[write.paramIdx].has (PF_Engage))
                                {
                                    state.runtime->enabled = write.norm >= 0.5f; pedal.setEnabled (state.runtime->enabled);
                                    state.runtime->foot.reset (state.runtime->enabled);
                                    engageEcho.push ({ state.runtime->identity, node.slot, state.runtime->enabled, state.runtime->modelEnabled });
                                }
                                else pedal.setParam (write.paramIdx, write.norm);
                            }
                    }
                }
            }
            else if (node.kind == NodeKind::Delay) context.state[index] = &s.lanes[node.aux];
            routing::nodeTable()[static_cast<int> (node.kind)] (node, context);
            if (node.kind == NodeKind::Pedal)
            {
                auto* runtime = plan.slots[node.slot].get();
                if (runtime && runtime->pedal->consumeEngageToggle())
                {
                    runtime->enabled = ! runtime->enabled; runtime->pedal->setEnabled (runtime->enabled);
                    runtime->foot.reset (runtime->enabled);
                    engageEcho.push ({ runtime->identity, node.slot, runtime->enabled, runtime->modelEnabled });
                }
            }
        }
        const double targetMix = localMix;
        const double enableStep = 1.0 / std::max (1.0, 0.020 * rate);
        for (int i = 0; i < count; ++i)
        {
            s.enable = std::clamp (s.enable + (localEnabled ? enableStep : -enableStep), 0.0, 1.0);
            if (s.enable < 1.e-12) s.enable = 0.0;
            if (s.enable > 1.0 - 1.e-12) s.enable = 1.0;
            s.mix += mixAlpha * (targetMix - s.mix);
            if (std::abs (s.mix - targetMix) < 1.e-10) s.mix = targetMix;
            if (s.phase != Segment::Idle)
            {
                const double progress = static_cast<double> (++s.ramp) / dipSamples;
                const double shape = mixer.render (0.0, 1.0, progress, MixLaw::EqualPower);
                s.gain = s.phase == Segment::Out ? s.rampStart * (1.0 - shape * shape) : shape * shape;
                if (s.phase == Segment::In && s.ramp == dipSamples) { s.phase = Segment::Idle; s.gain = 1.0; }
            }
            for (int c = 0; c < 2; ++c)
            {
                const double wet = dry[c][i] + s.gain * (buses[0][static_cast<size_t> (c)][static_cast<size_t> (i)] - dry[c][i]);
                const double result = mixer.render (dry[c][i], wet, s.mix * s.enable, newest->settings.mixLaw);
                (c == 0 ? left : right)[offset + i] = BoardLimiter::process (result, false, 1.0);
                peakOut = std::max (peakOut, std::abs ((c == 0 ? left : right)[offset + i]));
            }
        }
        offset += count;
    }
    if (! localEnabled)
    {
        bool silent = true;
        for (int segment = 0; segment < kNumPlacements; ++segment)
            if (newest->topology.segBegin[segment] != newest->topology.segEnd[segment]
                && segments[static_cast<size_t> (segment)].enable > 0.0) silent = false;
        if (newest->settings.trails)
            for (const auto& slot : newest->slots)
                if (slot && slot->pedal->desc().hasTails && slot->pedal->getTailEnergyDb() > -90.f) silent = false;
        if (silent) running.store (false);
    }
    if (newest->topology.n == 0 && s.pending == nullptr) running.store (false);
    inputPeak.store (static_cast<float> (20.0 * std::log10 (std::max (1.e-6, peakIn))));
    outputPeak.store (static_cast<float> (20.0 * std::log10 (std::max (1.e-6, peakOut))));
    acknowledge();
}
}
