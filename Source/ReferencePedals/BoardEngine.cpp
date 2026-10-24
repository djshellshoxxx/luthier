// OWNER: WP2
#include "BoardEngine.h"
#include "BoardLimiter.h"
#include "PedalRegistry.h"
#include "Routing/RoutingNodes.h"
#include <limits>
#include <cstring>
namespace luthier::reference
{
static_assert (std::atomic<const RenderPlan*>::is_always_lock_free, "Audio plan pointer must be lock-free");
static_assert (std::atomic<uint64_t>::is_always_lock_free && std::atomic<double>::is_always_lock_free && std::atomic<float>::is_always_lock_free, "Pedal meters require lock-free atomics");
static_assert (std::atomic<uint32_t>::is_always_lock_free, "Audio epoch must be lock-free");
BoardEngine::BoardEngine (BoardModel& source) : model (source)
{
    PedalRegistry::registerAll();
    prepare (48000, 4096);
    model.setCompiler ([this] (const auto& instances, const auto& settings, auto& info, auto& error, const BoardCommand* intent, bool hostEcho)
        { return publish (instances, settings, info, error, intent, hostEcho); });
    model.setExtraCapture ([this] (std::vector<PedalInstance>& instances)
    {
        // Shared references are copied only on the message thread and pin each
        // runtime even if a model's message-thread save callback reenters.
        const auto pinned = latest ? latest->slots : decltype (latest->slots) {};
        if (! latest)
            for (auto& instance : instances)
                for (size_t i = 0; i < releasedStored.size() && i < releasedCaptured.size(); ++i)
                    if (instance.instanceId == releasedStored[i].instanceId && instance.modelId == releasedStored[i].modelId
                        && juce::JSON::toString (instance.extras) == juce::JSON::toString (releasedStored[i].extras))
                    { instance.extras = releasedCaptured[i].extras.clone(); break; }
        for (auto& instance : instances)
            for (const auto& runtime : pinned)
                if (runtime && runtime->identity == instance.instanceId && runtime->model == instance.modelId)
                {
                    auto extra = instance.extras.clone(); runtime->pedal->saveExtra (extra);
                    if (auto* target = instance.extras.getDynamicObject(); target && extra.getDynamicObject())
                        for (const auto& property : extra.getDynamicObject()->getProperties()) target->setProperty (property.name, property.value.clone());
                    else instance.extras = extra.clone();
                    break;
                }
    });
}
BoardEngine::~BoardEngine() { model.setExtraCapture ({}); model.setCompiler ({}); }
void BoardEngine::prepare (double sampleRate, int maxBlock)
{
    std::vector<PedalInstance> captured, instances; BoardSettings settings;
    model.snapshot (captured, settings); model.snapshotStored (instances, settings);
    releaseResources();
    rate = std::isfinite (sampleRate) && sampleRate >= 8000 ? sampleRate : 48000;
    block = std::max (1, maxBlock);
    dipSamples = std::max (1, static_cast<int> (rate * 0.006));
    mixAlpha = 1.0 - std::exp (-1.0 / (0.020 * rate));
    pool.prepare (rate, block, captured);
    extendedModelSerials.fill (0); extendedHostSerials.fill (0);
    hostBoardSerials.fill (0); modelBoardSerials.fill (0);
    feedback.reset(); feedbackRearmSeen = feedbackKillSeen = 0; feedbackFault.store (false); feedbackTripped.store (false);
    paramPulses.clear(); numPendingPulses = 0; numRawMidi = numMidiOutput = 0;
    for (auto& segment : segments)
    {
        segment = {};
        for (auto& line : segment.dry) line.prepare (kMaxLatencySamp);
        for (auto& line : segment.lanes) line.prepare (kMaxLatencySamp);
        for (auto& loop : segment.loops) loop.prepare (kMaxLatencySamp);
    }
    TopologyInfo info; juce::String error;
    publish (instances, settings, info, error);
}
void BoardEngine::releaseResources()
{
    if (latest)
    {
        BoardSettings ignored;
        model.snapshotStored (releasedStored, ignored); model.snapshot (releasedCaptured, ignored);
    }
    live.store (nullptr, std::memory_order_release);
    for (auto& segment : segments) { segment.current = segment.pending = nullptr; }
    retired.reclaimAll(); latest.reset();
    audioEpoch.store (0, std::memory_order_release);
    running.store (false); targetActive.store (false); latencySamples.store (0);
}
bool BoardEngine::publish (const std::vector<PedalInstance>& instances, const BoardSettings& settings,
    TopologyInfo& info, juce::String& error, const BoardCommand* intent, bool hostEcho)
{
    auto plan = TopologyCompiler::compile (instances, settings, pool, version + 1, latest.get(), error, intent, hostEcho);
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
    if (! hostEcho && intent)
        if (const auto* write = std::get_if<cmd::SetParam> (intent))
            for (const auto& runtime : latest->slots)
                if (runtime && runtime->identity == write->id && write->paramIdx < runtime->pedal->desc().numParams)
                {
                    const auto& descriptor = runtime->pedal->desc();
                    const auto kind = descriptor.params[write->paramIdx].kind;
                    if (kind == ParamKind::Trigger || kind == ParamKind::Momentary)
                        paramPulses.push ({ write->id, descriptor.modelId, write->paramIdx, write->norm });
                }
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
void BoardEngine::publishPedalMeters (const RenderPlan& plan) noexcept
{
    for (int slot = 0; slot < kMaxSlots; ++slot)
    {
        auto& meter = pedalMeters[static_cast<size_t> (slot)];
        const auto* runtime = plan.slots[static_cast<size_t> (slot)].get();
        const auto sequence = meter.sequence.load (std::memory_order_relaxed);
        meter.sequence.store (sequence + 1, std::memory_order_seq_cst);
        uint64_t identity[2] {};
        PedalUiMeters ui;
        if (runtime)
        {
            std::memcpy (identity, runtime->identity.getRawData(), sizeof (identity));
            ui = runtime->pedal->readUiMeters();
        }
        meter.identity0.store (identity[0], std::memory_order_relaxed); meter.identity1.store (identity[1], std::memory_order_relaxed);
        meter.present.store (runtime != nullptr, std::memory_order_relaxed);
        meter.enabled.store (runtime && runtime->enabled, std::memory_order_relaxed);
        meter.tail.store (runtime ? runtime->pedal->getTailEnergyDb() : -120.f, std::memory_order_relaxed);
        meter.available.store (ui.available, std::memory_order_relaxed); meter.transport.store (ui.transport, std::memory_order_relaxed);
        meter.loopLength.store (ui.loopLength, std::memory_order_relaxed); meter.playhead.store (ui.playhead, std::memory_order_relaxed);
        meter.bpm.store (ui.bpm, std::memory_order_relaxed); meter.progress.store (ui.progress, std::memory_order_relaxed);
        meter.faults.store (ui.faults, std::memory_order_relaxed);
        meter.sequence.store (sequence + 2, std::memory_order_release);
    }
}
PedalMeters BoardEngine::readPedalMeters (int slot) const noexcept
{
    if (slot < 0 || slot >= kMaxSlots) return {};
    const auto& meter = pedalMeters[static_cast<size_t> (slot)];
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        const auto before = meter.sequence.load (std::memory_order_acquire);
        if (before & 1u) continue;
        PedalMeters result;
        uint64_t identity[2] { meter.identity0.load (std::memory_order_relaxed), meter.identity1.load (std::memory_order_relaxed) };
        result.identity = reinterpret_cast<const uint8_t*> (identity);
        result.present = meter.present.load (std::memory_order_relaxed); result.enabled = meter.enabled.load (std::memory_order_relaxed);
        result.tailEnergyDb = meter.tail.load (std::memory_order_relaxed);
        result.model.available = meter.available.load (std::memory_order_relaxed); result.model.transport = meter.transport.load (std::memory_order_relaxed);
        result.model.loopLength = meter.loopLength.load (std::memory_order_relaxed); result.model.playhead = meter.playhead.load (std::memory_order_relaxed);
        result.model.bpm = meter.bpm.load (std::memory_order_relaxed); result.model.progress = meter.progress.load (std::memory_order_relaxed);
        result.model.faults = meter.faults.load (std::memory_order_relaxed);
        std::atomic_thread_fence (std::memory_order_acquire);
        if (before == meter.sequence.load (std::memory_order_relaxed)) return result;
    }
    return {}; // contention is unavailable, never a fabricated stale identity
}
void BoardEngine::syncRouting (midi::MidiRouter& router) const noexcept
{
    if (const auto* plan = live.load (std::memory_order_acquire))
    {
        router.setModelEnabled (plan->settings.enabled, plan->boardSerials[0]);
        router.setModelTempoBpm (plan->settings.tempoBpm, plan->boardSerials[2]);
        router.setModelTempoSource (plan->settings.tempoSource, plan->extended.serials[static_cast<size_t> (ExtendedHostField::TempoSource)]);
    }
}
BoardMeters BoardEngine::meters() const noexcept
{
    BoardMeters result; result.inPeakDb = inputPeak.load(); result.outPeakDb = outputPeak.load();
    result.fbFault = feedbackFault.load(); result.fbTripped = feedbackTripped.load();
    const auto availability = auxAvailable.load();
    result.auxBAvailable = (availability & 1) != 0; result.auxWetAvailable = (availability & 2) != 0; result.auxDryAvailable = (availability & 4) != 0;
    result.latencySamples = latency(); result.sidechainAvailable = sidechainConnected.load();
    result.midiDropped = engageEcho.dropped() + paramPulses.dropped() + rawDropped.load() + outputDropped.load(); return result;
}
void BoardEngine::beginRawMidi (const juce::MidiBuffer& input, int samples) noexcept
{
    numRawMidi = 0;
    if (numMidiOutput > 0) { outputDropped.fetch_add (static_cast<uint32_t> (numMidiOutput), std::memory_order_relaxed); numMidiOutput = 0; }
    for (const auto metadata : input)
    {
        if (metadata.samplePosition < 0 || metadata.samplePosition >= samples) continue;
        if (metadata.numBytes < 1 || metadata.numBytes > 3 || numRawMidi == static_cast<int> (rawMidi.size()))
        { rawDropped.fetch_add (1, std::memory_order_relaxed); continue; }
        rawMidi[static_cast<size_t> (numRawMidi++)] = { metadata.data[0],
            static_cast<uint8_t> (metadata.numBytes > 1 ? metadata.data[1] : 0),
            static_cast<uint8_t> (metadata.numBytes > 2 ? metadata.data[2] : 0), metadata.samplePosition };
    }
}
int BoardEngine::drainMidiOut (MidiEvent* destination, int capacity) noexcept
{
    if (! destination || capacity <= 0) return 0;
    const int count = std::min (capacity, numMidiOutput);
    std::copy_n (midiOutput.data(), count, destination);
    std::move (midiOutput.begin() + count, midiOutput.begin() + numMidiOutput, midiOutput.begin());
    numMidiOutput -= count;
    return count;
}
void BoardEngine::applyExtended (const RenderPlan& plan) noexcept
{
    const bool wasFeedback = extended (ExtendedHostField::FeedbackOn) >= 0.5f;
    bool killRequested = false;
    for (size_t index = 0; index < extendedValues.size(); ++index)
    {
        if (extendedModelSerials[index] != plan.extended.serials[index])
        { extendedValues[index] = plan.extended.values[index]; extendedModelSerials[index] = plan.extended.serials[index]; }
        const auto serial = extendedHost.serials[index];
        if (serial == 0 || serial == extendedHostSerials[index]) continue;
        extendedHostSerials[index] = serial;
        const auto& descriptor = extendedHostParameters[index];
        if (descriptor.structural || ! std::isfinite (extendedHost.values[index])) continue;
        float value = std::clamp (extendedHost.values[index], descriptor.minimum, descriptor.maximum);
        if (descriptor.interval > 0) value = std::round (value);
        if (index == static_cast<size_t> (ExtendedHostField::FeedbackKill))
        {
            if (value >= 0.5f) killRequested = true;
            continue; // host momentary is a pulse; held model kill remains independent
        }
        if (index == static_cast<size_t> (ExtendedHostField::AbSelect) && value == 3.f)
            value = extendedValues[index] == 0.f ? 1.f : 0.f;
        extendedValues[index] = value;
    }
    if (! wasFeedback && extended (ExtendedHostField::FeedbackOn) >= 0.5f) feedback.reset();
    if (killRequested && plan.feedbackAvailable)
    {
        auto config = plan.topology.fb; config.on = extended (ExtendedHostField::FeedbackOn) >= 0.5f;
        feedback.configure (config, false, rate);
        feedback.killSamples = std::max (1, static_cast<int> (rate * 0.005));
    }
}
void BoardEngine::beginEvents (const midi::RouteOut& routed) noexcept
{
    events = &routed;
    const auto* plan = live.load (std::memory_order_acquire);
    if (! plan) return;
    if (feedbackRearmSeen != plan->feedbackRearm) { feedback.reset(); feedbackRearmSeen = plan->feedbackRearm; }
    if (feedbackKillSeen != plan->feedbackKill)
    {
        feedbackKillSeen = plan->feedbackKill;
        feedback.configure (plan->topology.fb, plan->settings.fb.kill, rate);
        feedback.killSamples = std::max (1, static_cast<int> (rate * 0.005));
    }
    applyExtended (*plan);
    int retained = 0;
    for (int i = 0; i < numPendingPulses; ++i)
    {
        const auto& pulse = pendingPulses[static_cast<size_t> (i)];
        for (const auto& runtime : plan->slots)
            if (runtime && runtime->identity == pulse.identity && runtime->pedal->desc().modelId == pulse.modelId)
            { pendingPulses[static_cast<size_t> (retained++)] = pulse; break; }
    }
    numPendingPulses = retained;
    while (numPendingPulses < static_cast<int> (pendingPulses.size())
        && paramPulses.pop (pendingPulses[static_cast<size_t> (numPendingPulses)])) ++numPendingPulses;
    if (modelBoardSerials[0] != plan->boardSerials[0]) effectiveEnabled = modelEnabled = plan->settings.enabled;
    if (modelBoardSerials[1] != plan->boardSerials[1]) effectiveMix = modelMix = plan->settings.mix;
    if (modelBoardSerials[2] != plan->boardSerials[2]) effectiveTempo = modelTempo = plan->settings.tempoBpm;
    modelBoardSerials = plan->boardSerials;
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
    latencySamples.store (effectiveEnabled && extended (ExtendedHostField::ReportLatency) >= 0.5f ? plan->topology.latencyTotal : 0);
}
void BoardEngine::process (double* left, double* right, int n, Placement placement) noexcept
{
    if (! left || ! right || n <= 0 || ! isValidPlacement (static_cast<int> (placement))) return;
    const auto* newest = live.load (std::memory_order_acquire);
    if (! newest) return;
    if (! events) applyExtended (*newest);
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
    const int p = static_cast<int> (placement);
    auto& s = segments[static_cast<size_t> (p)];
    if (resetRequested.exchange (false, std::memory_order_acq_rel))
    {
        for (int slot = 0; slot < kMaxSlots; ++slot)
            if (newest->slots[static_cast<size_t> (slot)]) newest->slots[static_cast<size_t> (slot)]->reset();
        for (auto& seg : segments)
        { for (auto& d : seg.dry) d.reset(); for (auto& d : seg.lanes) d.reset(); for (auto& loop : seg.loops) loop.reset(); seg.ab.reset(); }
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
    { s.current = newest; s.pending = nullptr; s.phase = Segment::Idle; s.gain = 1.0; s.enable = 0.0; publishPedalMeters (*s.current); acknowledge(); return; }
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
        for (int nodeIndex = topology.segBegin[p]; nodeIndex < topology.segEnd[p]; ++nodeIndex)
            if (topology.nodes[nodeIndex].kind == NodeKind::FbSend)
            {
                auto config = topology.fb; config.on = extended (ExtendedHostField::FeedbackOn) >= 0.5f;
                config.runawayOk = extended (ExtendedHostField::FeedbackRunawayOk) >= 0.5f;
                config.gain = std::min (extended (ExtendedHostField::FeedbackGain), config.runawayOk ? 1.05f : 0.98f);
                config.lpHz = extended (ExtendedHostField::FeedbackLpHz);
                feedback.configure (config, extended (ExtendedHostField::FeedbackKill) >= 0.5f, rate); break;
            }
        if (events)
            for (int event = 0; event < events->nWrites; ++event)
            {
                const auto& write = events->writes[event];
                if (write.offset != offset || write.slot != midi::kBoardTargetSlot) continue;
                switch (static_cast<midi::BoardTarget> (write.paramIdx))
                {
                    case midi::BoardTarget::Mix: localMix = write.norm; break;
                    case midi::BoardTarget::Tempo: break; // canonical router clock owns source priority and glide
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
        for (int event = 0; event < numRawMidi; ++event)
            if (rawMidi[static_cast<size_t> (event)].offset > offset)
                count = std::min (count, rawMidi[static_cast<size_t> (event)].offset - offset);
        if (s.phase != Segment::Idle) count = std::min (count, dipSamples - s.ramp);
        const bool hasNodes = topology.segBegin[p] != topology.segEnd[p] || (p == 0 && extended (ExtendedHostField::ReampEnable) >= 0.5f && topology.n > 0);
        if (! hasNodes && ! s.pending) { s.phase = Segment::Idle; s.gain = 1.0; offset += count; continue; }
        const bool hasSidechain = sidechain[0] && sidechain[1] && offset + count <= sidechainSamples;
        if (hasSidechain)
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < count; ++i)
                    sidechainScratch[static_cast<size_t> (c)][static_cast<size_t> (i)] = BoardLimiter::process (sidechain[c][offset + i], false, 1.0);
        const bool reamp = p == 0 && (extended (ExtendedHostField::ReampEnable) >= 0.5f) && hasSidechain;
        const double reampGain = reamp ? (extended (ExtendedHostField::ReampGainDb) <= -120.f ? 0.0 : std::pow (10.0, extended (ExtendedHostField::ReampGainDb) / 20.0)) : 0.0;
        double dry[2][kSubBlock] {};
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < count; ++i)
            {
                double in = (c == 0 ? left : right)[offset + i];
                if (reamp) in = (extended (ExtendedHostField::ReampReplace) >= 0.5f ? 0.0 : in)
                    + reampGain * sidechainScratch[static_cast<size_t> (c)][static_cast<size_t> (i)];
                peakIn = std::max (peakIn, std::abs (std::isfinite (in) ? in : 0.0));
                dry[c][i] = s.dry[static_cast<size_t> (c)].process (in, plan.latency[static_cast<size_t> (p)]);
                buses[0][static_cast<size_t> (c)][static_cast<size_t> (i)] = ! localEnabled && s.enable == 0.0 ? 0.0 : BoardLimiter::process (in, false, 1.0);
            }
        const bool wetPaused = localMix == 0.f && s.mix == 0.0 && extended (ExtendedHostField::MixTail) < 0.5f;
        if (wetPaused) for (auto& channel : buses[0]) std::fill_n (channel.data(), count, 0.0);
        ExecCtx context; context.n = count; context.topo = &topology;
        context.pedals = topology.pedals; context.sampleRate = rate;
        for (int bus = 0; bus < kMaxBuses; ++bus)
            for (int c = 0; c < 2; ++c) context.bus[bus][c] = buses[static_cast<size_t> (bus)][static_cast<size_t> (c)].data();
        std::array<PedalNodeState, kMaxSlots> pedalStates;
        for (int index = topology.segBegin[p]; index < topology.segEnd[p]; ++index)
        {
            auto node = topology.nodes[index]; context.nodeIndex = index;
            const auto dbGain = [] (float db) { return db <= -120.f ? 0.f : std::pow (10.f, db / 20.f); };
            if (node.kind == NodeKind::Limiter)
            { node.i0 = extended (ExtendedHostField::LimiterOn) >= 0.5f ? 1 : 0; node.f0 = dbGain (extended (ExtendedHostField::LimiterCeilDb)); }
            if (node.kind == NodeKind::LaneGain)
                node.f0 = dbGain (extendedValues[static_cast<size_t> (25 + 2 * node.lane)]) * (extendedValues[static_cast<size_t> (26 + 2 * node.lane)] >= 0.5f ? -1.f : 1.f);
            if (node.kind == NodeKind::Sum && node.i0 != 0)
            { const auto selection = extended (ExtendedHostField::AbSelect); node.f1 = selection == 0.f ? 0.f : selection == 1.f ? 1.f : 0.5f; }
            if (node.kind == NodeKind::SendTap || node.kind == NodeKind::ReturnInject)
            {
                const int base = 33 + 8 * node.aux; const bool enabled = extendedValues[static_cast<size_t> (base + 1)] >= 0.5f;
                if (node.kind == NodeKind::SendTap) node.f0 = enabled ? dbGain (extendedValues[static_cast<size_t> (base + 4)]) : 0.f;
                else
                {
                    node.f0 = ! enabled ? 0.f : topology.loops[node.aux].kind == LoopKind::Parallel ? extendedValues[static_cast<size_t> (base + 3)] : 1.f;
                    node.f1 = dbGain (extendedValues[static_cast<size_t> (base + 5)]);
                }
            }
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
                            && state.runtime->modelParamSerials[static_cast<size_t> (param)] != state.control->paramSerials[static_cast<size_t> (param)])
                        {
                            state.runtime->modelParamSerials[static_cast<size_t> (param)] = state.control->paramSerials[static_cast<size_t> (param)];
                            state.runtime->modelParams[static_cast<size_t> (param)] = state.control->params[static_cast<size_t> (param)];
                            pedal.setParam (param, state.control->params[static_cast<size_t> (param)]);
                        }
                    if (state.runtime->modelEnabledSerial != state.control->enabledSerial)
                    {
                        state.runtime->modelEnabledSerial = state.control->enabledSerial;
                        state.runtime->modelEnabled = state.runtime->enabled = state.control->enabled;
                        state.runtime->bypassSamples = std::max (1.0, 0.005 * rate);
                        state.runtime->foot.reset (state.control->enabled); pedal.setEnabled (state.control->enabled);
                    }
                    if (state.runtime->modelInputSerial != state.control->inputSerial)
                    { state.runtime->modelInputSerial = state.control->inputSerial; state.runtime->inputGain = state.runtime->modelInputGain = state.control->inputGain; }
                    if (state.runtime->modelOutputSerial != state.control->outputSerial)
                    { state.runtime->modelOutputSerial = state.control->outputSerial; state.runtime->outputGain = state.runtime->modelOutputGain = state.control->outputGain; }
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
                    if (localEnabled)
                        for (int i = 0; i < numPendingPulses; ++i)
                        {
                            auto& pulse = pendingPulses[static_cast<size_t> (i)];
                            if (pulse.identity == state.runtime->identity && pulse.modelId == pedal.desc().modelId)
                            { pedal.setParam (pulse.param, pulse.value); pulse.identity = InstanceId::null(); }
                        }
                    auto time = blockTime;
                    time.sample += offset; if (time.valid) time.ppq += offset * time.bpm / (60.0 * rate);
                    pedal.setTime (time);
                    if (localEnabled && pedal.desc().wantsRawMidi)
                        for (int event = 0; event < numRawMidi; ++event)
                            if (rawMidi[static_cast<size_t> (event)].offset == offset) pedal.onMidi (rawMidi[static_cast<size_t> (event)]);
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
                                        state.runtime->bypassSamples = std::max (1.0, rate * (foot.press ? 25.0 - 23.0 * std::clamp (static_cast<double> (foot.speed), 0.0, 1.0) : 12.0) * 0.001);
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
            else if (node.kind == NodeKind::FbSend || node.kind == NodeKind::FbReturn) context.state[index] = &feedback;
            else if (node.kind == NodeKind::Sum && node.i0 != 0) context.state[index] = &s.ab;
            else if (node.kind == NodeKind::Delay) context.state[index] = &s.lanes[node.aux];
            else if (node.kind == NodeKind::SendTap || node.kind == NodeKind::ReturnInject) context.state[index] = &s.loops[node.aux];
            if (! wetPaused) routing::nodeTable()[static_cast<int> (node.kind)] (node, context);
            if (node.kind == NodeKind::Pedal)
            {
                auto* runtime = plan.slots[node.slot].get();
                if (runtime)
                {
                    MidiEvent outgoing[64];
                    const int drained = std::clamp (runtime->pedal->drainMidiOut (outgoing, 64), 0, 64);
                    if (localEnabled)
                        for (int event = 0; event < drained; ++event)
                        {
                            auto value = outgoing[event]; value.offset = offset + std::clamp (value.offset, 0, count - 1);
                            if (numMidiOutput == static_cast<int> (midiOutput.size()))
                            { outputDropped.fetch_add (1, std::memory_order_relaxed); continue; }
                            int insertion = numMidiOutput++;
                            while (insertion > 0 && midiOutput[static_cast<size_t> (insertion - 1)].offset > value.offset)
                            { midiOutput[static_cast<size_t> (insertion)] = midiOutput[static_cast<size_t> (insertion - 1)]; --insertion; }
                            midiOutput[static_cast<size_t> (insertion)] = value;
                        }
                }
                if (runtime && runtime->pedal->consumeEngageToggle())
                {
                    runtime->enabled = ! runtime->enabled; runtime->pedal->setEnabled (runtime->enabled);
                    runtime->foot.reset (runtime->enabled);
                    engageEcho.push ({ runtime->identity, node.slot, runtime->enabled, runtime->modelEnabled });
                }
            }
        }
        const auto sendAux = [&] (const BoardAuxOutputs::Stereo& destination, const double* l, const double* r)
        {
            if (! destination.left || ! destination.right || offset + count > destination.samples) return;
            for (int i = 0; i < count; ++i)
            {
                destination.left[offset + i] = static_cast<float> (BoardLimiter::process (destination.left[offset + i] + l[i], false, 1.0));
                destination.right[offset + i] = static_cast<float> (BoardLimiter::process (destination.right[offset + i] + r[i], false, 1.0));
            }
        };
        if (localEnabled)
        {
            sendAux (auxOutputs.wet, buses[0][0].data(), buses[0][1].data());
            sendAux (auxOutputs.dry, dry[0], dry[1]);
            if (plan.settings.mode == Mode::AB && plan.settings.abOut == AbOutMode::Dual)
                sendAux (auxOutputs.pedalB, buses[1][0].data(), buses[1][1].data());
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
                const double result = mixer.render (dry[c][i], wet, s.mix * s.enable, static_cast<MixLaw> (static_cast<int> (extended (ExtendedHostField::MixLaw))));
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
        if (extended (ExtendedHostField::Trails) >= 0.5f)
            for (const auto& slot : newest->slots)
                if (slot && slot->pedal->desc().hasTails && slot->pedal->getTailEnergyDb() > -90.f) silent = false;
        if (silent) running.store (false);
    }
    if (newest->topology.n == 0 && s.pending == nullptr) running.store (false);
    inputPeak.store (static_cast<float> (20.0 * std::log10 (std::max (1.e-6, peakIn))));
    outputPeak.store (static_cast<float> (20.0 * std::log10 (std::max (1.e-6, peakOut))));
    feedbackFault.store (feedback.fault); feedbackTripped.store (feedback.tripped);
    publishPedalMeters (*s.current);
    acknowledge();
}
}
