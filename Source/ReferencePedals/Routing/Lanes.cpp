// OWNER: WP3
#include "RoutingNodes.h"
#include "AbSwitch.h"
#include <algorithm>
namespace luthier::reference::routing
{
void nodeSplit (const Node& node, ExecCtx& ctx) noexcept
{
    for (int lane = 1; lane < node.aux && lane < kMaxBranches; ++lane)
        for (int c = 0; c < 2; ++c) std::copy_n (ctx.bus[0][c], ctx.n, ctx.bus[lane][c]);
}
void nodeSum (const Node& node, ExecCtx& ctx) noexcept
{
    if (node.i0 != 0) { mixAb (node, ctx); return; }
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < ctx.n; ++i)
        {
            double sum = 0;
            for (int lane = 0; lane < node.aux && lane < kMaxBranches; ++lane) sum += ctx.bus[lane][c][i];
            ctx.bus[0][c][i] = sum * node.f0;
        }
}
void nodeLaneGain (const Node& node, ExecCtx& ctx) noexcept
{
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < ctx.n; ++i) ctx.bus[node.lane][c][i] *= node.f0;
}
}
