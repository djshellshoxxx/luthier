// OWNER: WP2
#include "PrototypePedals.h"
namespace luthier::reference::models
{
namespace
{
constexpr double pi = 3.14159265358979323846;
#define PARAM(id, label) { id, label, "", ParamKind::Continuous, Taper::Linear, 0.f, 1.f, 0.5f, 1.f, 0, nullptr, PF_Automatable, 20.f, -1 }
const ParamDesc tsParams[] { PARAM ("prototype_ts808_drive", "Drive"), PARAM ("prototype_ts808_tone", "Tone"), PARAM ("prototype_ts808_level", "Level") };
const ParamDesc dsParams[] { PARAM ("prototype_ds1_drive", "Distortion"), PARAM ("prototype_ds1_tone", "Tone"), PARAM ("prototype_ds1_level", "Level") };
const ParamDesc phParams[] { PARAM ("prototype_phase90_speed", "Speed"), PARAM ("prototype_phase90_tone", "Reserved"), PARAM ("prototype_phase90_level", "Reserved") };
#undef PARAM
const PedalDesc descriptions[] {
    { "prototype.ts808", "prototype_ts808_", 1, "TS808-inspired (prototype)", "Prototype", "TS808", ChannelMode::Mono, BypassMode::True, 3, tsParams, 0, false, false, false, 1 },
    { "prototype.ds1", "prototype_ds1_", 1, "DS-1-inspired (prototype)", "Prototype", "DS-1", ChannelMode::Mono, BypassMode::True, 3, dsParams, 0, false, false, false, 1 },
    { "prototype.phase90", "prototype_phase90_", 1, "Phase 90-inspired (prototype)", "Prototype", "Phase 90", ChannelMode::Mono, BypassMode::True, 3, phParams, 0, false, false, false, 1 }
};
}
const PedalDesc& PrototypePedal::description (Model model) { return descriptions[std::clamp (static_cast<int> (model) - 1, 0, 2)]; }
PrototypePedal::PrototypePedal (Model type) : PedalBase (description (type)), model (type) {}
void PrototypePedal::prepare (double sr, int, int)
{
    rate = std::isfinite (sr) && sr >= 8000 ? sr : 48000;
    const auto coefficient = [this] (double hz) { return 1.0 - std::exp (-2.0 * pi * hz / rate); };
    inputAlpha = coefficient (model == Model::ts808Prototype ? 720.0 : 72.0);
    bassAlpha = coefficient (330.0); highAlpha = coefficient (1350.0);
    prepareParams (rate); reset();
}
void PrototypePedal::reset() noexcept { for (auto& channel : channels) channel = {}; phase = 0; controlCounter = 0; }
double PrototypePedal::tick (Channel& state, double input, double drive, double tone, double level) noexcept
{
    input = softGuard (input);
    if (model == Model::phase90Prototype)
    {
        double wet = input;
        for (int stage = 0; stage < 4; ++stage)
        {
            const double next = -ap * wet + state.apX[stage] + ap * state.apY[stage];
            state.apX[stage] = wet; state.apY[stage] = next; wet = next;
        }
        return softGuard (0.5 * (input + wet));
    }
    state.low += inputAlpha * (input - state.low);
    if (model == Model::ts808Prototype)
    {
        const double clipped = std::tanh ((input - state.low) * (2.0 + 38.0 * drive) + state.low * 0.6);
        state.tone += toneAlpha * (clipped - state.tone);
        return softGuard (state.tone * (0.1 + 1.5 * level));
    }
    const double clipped = std::clamp ((input - state.low) * 4.0 * (1.0 + 21.3 * drive), -0.7, 0.7);
    state.bass += bassAlpha * (clipped - state.bass);
    state.high += highAlpha * (clipped - state.high);
    return softGuard ((state.bass * (1.0 - tone) + (clipped - state.high) * tone) * (0.12 + 1.7 * level));
}
void PrototypePedal::process (double* left, double* right, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        const double drive = nextSmoothed (0), tone = nextSmoothed (1), level = nextSmoothed (2);
        if (controlCounter++ == 0)
        {
            toneAlpha = 1.0 - std::exp (-2.0 * pi * std::min (700.0 + 6500.0 * tone, rate * 0.45) / rate);
            const double triangle = 1.0 - 4.0 * std::abs (phase - 0.5);
            const double frequency = 130.0 * std::pow (16.0, (triangle + 1.0) * 0.5);
            const double k = std::tan (pi * frequency / rate); ap = (k - 1.0) / (k + 1.0);
        }
        if (controlCounter == kSubBlock) controlCounter = 0;
        phase += (0.13 + 3.0 * drive) / rate; if (phase >= 1.0) phase -= 1.0;
        left[i] = tick (channels[0], left[i], drive, tone, level);
        right[i] = tick (channels[1], right[i], drive, tone, level);
    }
}
}
