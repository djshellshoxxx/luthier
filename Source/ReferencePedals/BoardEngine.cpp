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
    result.latencySamples = latency(); return result;
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
    if (! newest->settings.enabled && ! running.load()) { s.current = newest; s.pending = nullptr; acknowledge(); return; }
    if (newest->settings.enabled) running.store (true);
    double peakIn = 0, peakOut = 0;
    for (int offset = 0; offset < n;)
    {
        if (s.phase == Segment::Out && (s.ramp >= dipSamples || s.mix == 0.0))
        {
            s.current = s.pending; s.pending = nullptr;
            s.phase = Segment::In; s.ramp = 0; s.gain = 0.0;
        }
        const auto& plan = *s.current; const auto& topology = plan.topology;
        int count = std::min ({ kSubBlock, block, n - offset });
        if (s.phase != Segment::Idle) count = std::min (count, dipSamples - s.ramp);
        const bool hasNodes = topology.segBegin[p] != topology.segEnd[p];
        if (! hasNodes && ! s.pending) { offset += count; continue; }
        double dry[2][kSubBlock] {};
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < count; ++i)
            {
                const double in = (c == 0 ? left : right)[offset + i];
                peakIn = std::max (peakIn, std::abs (std::isfinite (in) ? in : 0.0));
                dry[c][i] = s.dry[static_cast<size_t> (c)].process (in, plan.latency[static_cast<size_t> (p)]);
                buses[0][static_cast<size_t> (c)][static_cast<size_t> (i)] = BoardLimiter::process (in, false, 1.0);
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
                    for (int param = 0; param < pedal.desc().numParams; ++param)
                        if (! pedal.desc().params[param].has (PF_Engage)
                            && pedal.getParam (param) != state.control->params[static_cast<size_t> (param)])
                            pedal.setParam (param, state.control->params[static_cast<size_t> (param)]);
                    if (state.runtime->enabled != state.control->enabled)
                    { state.runtime->enabled = state.control->enabled; pedal.setEnabled (state.control->enabled); }
                    pedal.setTime (blockTime);
                }
            }
            else if (node.kind == NodeKind::Delay) context.state[index] = &s.lanes[node.aux];
            routing::nodeTable()[static_cast<int> (node.kind)] (node, context);
        }
        const double targetMix = newest->settings.enabled ? newest->settings.mix : 0.0;
        for (int i = 0; i < count; ++i)
        {
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
                const double result = mixer.render (dry[c][i], wet, s.mix, newest->settings.mixLaw);
                (c == 0 ? left : right)[offset + i] = BoardLimiter::process (result, false, 1.0);
                peakOut = std::max (peakOut, std::abs ((c == 0 ? left : right)[offset + i]));
            }
        }
        offset += count;
    }
    if (! newest->settings.enabled)
    {
        bool silent = true;
        for (int segment = 0; segment < kNumPlacements; ++segment)
            if (newest->topology.segBegin[segment] != newest->topology.segEnd[segment]
                && segments[static_cast<size_t> (segment)].mix > 1.e-10) silent = false;
        if (silent) running.store (false);
    }
    inputPeak.store (static_cast<float> (20.0 * std::log10 (std::max (1.e-6, peakIn))));
    outputPeak.store (static_cast<float> (20.0 * std::log10 (std::max (1.e-6, peakOut))));
    acknowledge();
}
}
