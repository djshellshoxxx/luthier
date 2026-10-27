// OWNER: WP2
#include "Routing/RoutingNodes.h"
#include "BoardEngine.h"
#include "BoardLimiter.h"
namespace luthier::reference::routing
{
void nodePedal (const Node& node, ExecCtx& ctx) noexcept
{
    auto* state = static_cast<PedalNodeState*> (ctx.state[ctx.nodeIndex]);
    if (! state || ! state->runtime) return; // persisted unknown model is a transparent placeholder
    auto& runtime = *state->runtime;
    auto* left = ctx.bus[node.lane][0]; auto* right = ctx.bus[node.lane][1];
    double dry[2][kSubBlock] {}, wetResidual[2][kSubBlock] {};
    bool residualAvailable[kSubBlock] {};
    const auto& description = runtime.pedal->desc();
    const bool buffered = description.bypass == BypassMode::Buffered;
    const bool tailContract = description.bypass == BypassMode::DSP || (buffered && description.hasTails);
    const bool equalPower = !tailContract && description.bypassFade == BypassFade::EqualPower;
    const int latency = ctx.topo->slotLatency[node.slot];
    if (state->processWet)
        for (int i = 0; i < ctx.n; ++i)
        {
            dry[0][i] = runtime.dry[0].process (left[i], latency);
            dry[1][i] = runtime.dry[1].process (right[i], latency);
            left[i] *= runtime.inputGain; right[i] *= runtime.inputGain;
        }
    if (state->processWet)
    {
        if (buffered) runtime.pedal->processBypass (dry[0], dry[1], ctx.n);
    }
    const midi::ParamWrite* writes[midi::kMaxRouteWrites]; int numWrites = 0;
    if (state->eventsEnabled && state->events)
        for (int event = 0; event < state->events->nWrites; ++event)
        {
            const auto& write = state->events->writes[event];
            if (write.slot != node.slot || write.paramIdx >= runtime.pedal->desc().numParams
                || write.offset < state->offset || write.offset >= state->offset + ctx.n) continue;
            const auto& param = runtime.pedal->desc().params[write.paramIdx];
            if (param.has (PF_Engage) || (param.kind != ParamKind::Continuous && param.kind != ParamKind::Tempo)) continue;
            int at = numWrites++;
            while (at > 0 && writes[at - 1]->offset > write.offset) { writes[at] = writes[at - 1]; --at; }
            writes[at] = &write;
        }
    const MidiEvent* raw[256]; int numRaw = 0;
    if (state->eventsEnabled && runtime.pedal->desc().wantsRawMidi)
        for (int event = 0; event < state->numRaw; ++event)
            if (state->raw[event].offset >= state->offset && state->raw[event].offset < state->offset + ctx.n)
                raw[numRaw++] = &state->raw[event];
    const bool render = state->processWet && (runtime.enabled || runtime.enableMix > 0.0 || runtime.pedal->getTailEnergyDb() > -90.f);
    const auto processOnce = [&] (int offset, int samples) noexcept
    {
        if (! render) return;
        if (tailContract)
        {
            const bool available = runtime.pedal->processWithWetResidual (left + offset, right + offset,
                wetResidual[0] + offset, wetResidual[1] + offset, samples);
            std::fill_n (residualAvailable + offset, samples, available);
            runtime.tailResidualAvailable = available;
        }
        else if (equalPower) runtime.pedal->processForBoard (left + offset, right + offset, samples);
        else runtime.pedal->process (left + offset, right + offset, samples);
    };
    if (runtime.rampMask != 0 || numWrites > 0 || numRaw > 0)
    {
        int writeIndex = 0, rawIndex = 0;
        for (int i = 0; i < ctx.n; ++i)
        {
            while (writeIndex < numWrites && writes[writeIndex]->offset == state->offset + i)
            {
                const auto& write = *writes[writeIndex++]; runtime.writeParam (write.paramIdx, write.norm, write.smoothMs);
            }
            auto time = state->time; time.sample += i;
            if (time.valid) time.ppq += i * time.bpm / (60.0 * ctx.sampleRate);
            runtime.pedal->setTime (time);
            while (rawIndex < numRaw && raw[rawIndex]->offset == state->offset + i) runtime.pedal->onMidi (*raw[rawIndex++]);
            runtime.advanceParams();
            runtime.pedal->setSidechain (state->sidechain[0] ? state->sidechain[0] + i : nullptr,
                                        state->sidechain[1] ? state->sidechain[1] + i : nullptr, state->sidechain[0] ? 1 : 0);
            processOnce (i, 1);
            runtime.collectMidi (i, 1);
        }
    }
    else
    {
        processOnce (0, ctx.n);
        runtime.collectMidi (0, ctx.n);
    }
    if (! state->processWet) return;
    const double step = 1.0 / std::max (1.0, runtime.bypassSamples);
    for (int i = 0; i < ctx.n; ++i)
    {
        runtime.enableMix = std::clamp (runtime.enableMix + (runtime.enabled ? step : -step), 0.0, 1.0);
        double dryGain = 1.0, wetGain = 0.0;
        if (equalPower)
        {
            if (runtime.enableMix >= 1.0) { dryGain = 0.0; wetGain = 1.0; }
            else if (runtime.enableMix > 0.0)
            {
                constexpr double halfPi = 1.57079632679489661923;
                dryGain = std::cos (halfPi * runtime.enableMix);
                wetGain = std::sin (halfPi * runtime.enableMix);
            }
        }
        for (int channel = 0; channel < 2; ++channel)
        {
            auto& output = channel == 0 ? left[i] : right[i];
            const double processed = output * runtime.outputGain;
            // E68: the explicit wet term survives bypass. Inferring dry from the
            // slot input is invalid when a model guards/clips its dry path or sum.
            const bool validResidual = residualAvailable[i] && std::isfinite (wetResidual[channel][i]);
            if (residualAvailable[i] && ! validResidual) runtime.tailResidualAvailable = false;
            const double bypass = dry[channel][i] + (validResidual ? wetResidual[channel][i] * runtime.outputGain : 0.0);
            // An unavailable residual uses ordinary fading, without a second render.
            const double result = !render ? dry[channel][i] : equalPower
                ? (runtime.enableMix <= 0.0 ? bypass : runtime.enableMix >= 1.0 ? processed
                    : dryGain * bypass + wetGain * processed)
                : bypass + runtime.enableMix * (processed - bypass);
            output = BoardLimiter::process (result, false, 1.0);
        }
    }
}
void nodeLimiter (const Node& node, ExecCtx& ctx) noexcept
{
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < ctx.n; ++i)
            ctx.bus[0][c][i] = BoardLimiter::process (ctx.bus[0][c][i], node.i0 != 0, node.f0);
}
const NodeFn* nodeTable() noexcept
{
    static const NodeFn table[kNodeKindCount] {
        &nodePedal, &nodeSplit, &nodeSum, &nodeLaneGain, &nodeDelay, &nodeSendTap,
        &nodeReturnInject, &nodeFbSend, &nodeFbReturn, &nodeLimiter, &nodeStereoWidth
    };
    return table;
}
}
