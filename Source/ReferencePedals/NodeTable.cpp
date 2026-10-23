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
    auto& runtime = *state->runtime; const auto& control = *state->control;
    auto* left = ctx.bus[node.lane][0]; auto* right = ctx.bus[node.lane][1];
    double dry[2][kSubBlock] {};
    const int latency = ctx.topo->slotLatency[node.slot];
    for (int i = 0; i < ctx.n; ++i)
    {
        dry[0][i] = runtime.dry[0].process (left[i], latency);
        dry[1][i] = runtime.dry[1].process (right[i], latency);
        left[i] *= control.inputGain; right[i] *= control.inputGain;
    }
    if (control.enabled || runtime.enableMix > 0.0 || runtime.pedal->getTailEnergyDb() > -90.f)
        runtime.pedal->process (left, right, ctx.n);
    const double step = 1.0 / std::max (1.0, 0.005 * ctx.sampleRate);
    for (int i = 0; i < ctx.n; ++i)
    {
        runtime.enableMix = std::clamp (runtime.enableMix + (control.enabled ? step : -step), 0.0, 1.0);
        left[i] = BoardLimiter::process (dry[0][i] + runtime.enableMix * (left[i] * control.outputGain - dry[0][i]), false, 1.0);
        right[i] = BoardLimiter::process (dry[1][i] + runtime.enableMix * (right[i] * control.outputGain - dry[1][i]), false, 1.0);
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
