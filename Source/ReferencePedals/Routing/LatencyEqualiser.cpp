// OWNER: WP3
#include "RoutingNodes.h"
#include "LatencyEqualiser.h"
namespace luthier::reference::routing
{
void nodeDelay (const Node& node, ExecCtx& ctx) noexcept
{
    auto* delay = static_cast<LatencyEqualiser*> (ctx.state[ctx.nodeIndex]);
    if (! delay) return;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < ctx.n; ++i)
            ctx.bus[node.lane][c][i] = delay->channels[static_cast<size_t> (c)].process (ctx.bus[node.lane][c][i], node.i0);
}
}
