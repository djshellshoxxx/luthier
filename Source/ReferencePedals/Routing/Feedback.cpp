// OWNER: WP3
#include "RoutingNodes.h"
#include "Feedback.h"
#include <algorithm>
#include <cmath>
namespace luthier::reference::routing
{
void FeedbackState::setTarget (double value, double seconds) noexcept
{
    if (value == target) return;
    target = value; ramp = std::max (1, static_cast<int> (rate * seconds)); step = (target - gain) / ramp;
}
void FeedbackState::configure (const FeedbackCfg& cfg, bool kill, double sampleRate) noexcept
{
    if (! cfg.on) { if (active) reset(); return; }
    if (! active || source != cfg.srcSlot || destination != cfg.dstSlot)
    {
        const bool wasFault = active && fault, wasTripped = active && tripped;
        reset(); active = true; source = cfg.srcSlot; destination = cfg.dstSlot;
        fault = wasFault; tripped = wasTripped;
    }
    rate = sampleRate;
    lp = 1.0 - std::exp (-6.2831853071795864769 * std::clamp (static_cast<double> (cfg.lpHz), 20.0, rate * 0.45) / rate);
    hp = std::exp (-6.2831853071795864769 * 20.0 / rate);
    if (fault || tripped) setTarget (0.0, tripped ? 0.050 : 0.005);
    else setTarget (kill || killSamples > 0 ? 0.0 : cfg.gain, 0.005);
}
void FeedbackState::fail() noexcept
{
    fault = true; gain = target = step = energy = 0.0; ramp = hotSamples = 0;
    for (auto& channel : ring) channel.fill (0.0);
    low.fill (0.0); previous.fill (0.0); dc.fill (0.0);
}
void nodeFbReturn (const Node& node, ExecCtx& ctx) noexcept
{
    auto* state = static_cast<FeedbackState*> (ctx.state[ctx.nodeIndex]);
    if (! state || state->fault) return;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < ctx.n; ++i)
            ctx.bus[node.lane][c][i] += state->ring[static_cast<size_t> (c)][static_cast<size_t> ((state->position + i) % kSubBlock)];
}
void nodeFbSend (const Node& node, ExecCtx& ctx) noexcept
{
    auto* state = static_cast<FeedbackState*> (ctx.state[ctx.nodeIndex]);
    if (! state || state->fault) return;
    const double energyAlpha = 1.0 - std::exp (-1.0 / (0.020 * state->rate));
    for (int i = 0; i < ctx.n; ++i)
    {
        if (state->killSamples > 0) --state->killSamples;
        if (state->ramp > 0) { state->gain += state->step; if (--state->ramp == 0) state->gain = state->target; }
        if (state->gain == 0.0 && state->target == 0.0)
        { state->low.fill (0.0); state->previous.fill (0.0); state->dc.fill (0.0); }
        const double input[2] { ctx.bus[node.lane][0][i] * state->gain, ctx.bus[node.lane][1][i] * state->gain };
        if (! std::isfinite (input[0]) || ! std::isfinite (input[1])) { state->fail(); return; }
        const double power = std::min (1.e12, 0.5 * (input[0] * input[0] + input[1] * input[1]));
        state->energy += energyAlpha * (power - state->energy);
        state->hotSamples = state->energy > 15.848931924611133 ? state->hotSamples + 1 : 0;
        if (! state->tripped && state->hotSamples > static_cast<int> (0.200 * state->rate))
        { state->tripped = true; state->setTarget (0.0, 0.050); }
        for (int c = 0; c < 2; ++c)
        {
            auto& low = state->low[static_cast<size_t> (c)]; low += state->lp * (input[c] - low);
            const double limited = 2.0 * std::tanh (low * 0.5);
            auto& dc = state->dc[static_cast<size_t> (c)];
            dc = limited - state->previous[static_cast<size_t> (c)] + state->hp * dc;
            state->previous[static_cast<size_t> (c)] = limited;
            if (! std::isfinite (dc)) { state->fail(); return; }
            if (std::abs (dc) < 1.e-30) dc = 0.0;
            state->ring[static_cast<size_t> (c)][static_cast<size_t> ((state->position + i) % kSubBlock)] = std::clamp (dc, -2.0, 2.0);
        }
    }
    state->position = (state->position + ctx.n) % kSubBlock;
}
}
