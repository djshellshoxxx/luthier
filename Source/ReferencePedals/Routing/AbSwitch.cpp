// OWNER: WP3
#include "AbSwitch.h"
#include <algorithm>
#include <cmath>
namespace luthier::reference::routing
{
void mixAb (const Node& node, ExecCtx& ctx) noexcept
{
    auto* state = static_cast<AbSwitchState*> (ctx.state[ctx.nodeIndex]);
    if (! state) return;
    const double target = static_cast<double> (node.f1) * 1.57079632679489661923;
    if (! state->initialised) { state->angle = state->target = target; state->initialised = true; }
    if (target != state->target)
    {
        state->target = target; state->remaining = std::max (1, static_cast<int> (ctx.sampleRate * 0.010));
        state->step = (target - state->angle) / state->remaining;
    }
    for (int i = 0; i < ctx.n; ++i)
    {
        if (state->remaining > 0 && --state->remaining == 0) state->angle = state->target;
        else if (state->remaining > 0) state->angle += state->step;
        const double a = state->angle <= 0 ? 1.0 : state->angle >= 1.57079632679489661923 ? 0.0 : std::cos (state->angle);
        const double b = state->angle <= 0 ? 0.0 : state->angle >= 1.57079632679489661923 ? 1.0 : std::sin (state->angle);
        for (int c = 0; c < 2; ++c)
        {
            const double laneB = ctx.bus[1][c][i] * b;
            ctx.bus[0][c][i] = ctx.bus[0][c][i] * a + (node.i0 == 1 ? laneB : 0.0);
            ctx.bus[1][c][i] = laneB; // E54: Dual output is the selected B send, never folded into main
        }
    }
}
}
