// OWNER: WP3
#include "RoutingNodes.h"
#include "SendReturn.h"
#include <algorithm>
#include <cmath>
namespace luthier::reference::routing
{
void nodeSendTap (const Node& node, ExecCtx& ctx) noexcept
{
    auto* state = static_cast<SendReturnState*> (ctx.state[ctx.nodeIndex]);
    if (! state) return;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < ctx.n; ++i)
        {
            const double input = ctx.bus[0][c][i];
            ctx.bus[kBusLoopBase + node.aux][c][i] = input * node.f0;
            ctx.bus[0][c][i] = state->dry.channels[static_cast<size_t> (c)].process (input, node.i0);
        }
}
void nodeReturnInject (const Node& node, ExecCtx& ctx) noexcept
{
    auto* state = static_cast<SendReturnState*> (ctx.state[ctx.nodeIndex]);
    if (! state) return;
    if (! state->initialised) { state->mix = node.f0; state->initialised = true; }
    const double step = 1.0 / std::max (1.0, 0.005 * ctx.sampleRate);
    for (int i = 0; i < ctx.n; ++i)
    {
        state->mix += std::clamp (static_cast<double> (node.f0) - state->mix, -step, step);
        // Loop blend is linear: correlated identity loops remain unity.
        for (int c = 0; c < 2; ++c)
            ctx.bus[0][c][i] = (1.0 - state->mix) * ctx.bus[0][c][i]
                + state->mix * node.f1 * ctx.bus[kBusLoopBase + node.aux][c][i];
    }
}
}
