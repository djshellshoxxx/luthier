// OWNER: WP3
#include "RoutingNodes.h"
namespace luthier::reference::routing
{
void nodeStereoWidth (const Node& node, ExecCtx& ctx) noexcept
{
    for (int i = 0; i < ctx.n; ++i)
    {
        const double mid = 0.5 * (ctx.bus[node.lane][0][i] + ctx.bus[node.lane][1][i]);
        const double side = 0.5 * (ctx.bus[node.lane][0][i] - ctx.bus[node.lane][1][i]) * node.f0;
        ctx.bus[node.lane][0][i] = mid + side; ctx.bus[node.lane][1][i] = mid - side;
    }
}
}
