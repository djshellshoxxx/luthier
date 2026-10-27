// OWNER: WP12
#include "mxr_dynacomp.h"
#include "../Common/EnvelopeFollower.h"
#include "../Common/MildTanh.h"
namespace luthier::reference::models
{
PreparedMemoryRequirement MxrDynaComp::prepareMemory(double sr, int block, int policy, const float *) noexcept
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000 || block <= 0 || policy < 0 || policy > 2)
        return {};
    return {sizeof(MxrDynaComp) + 2 * (23 + 16) * sizeof(double), 0};
}
uint64_t MxrDynaComp::getPreparedMemoryBytes() const noexcept
{
    return sizeof(*this) + dry[0].ownedMemoryBytes() + dry[1].ownedMemoryBytes();
}

namespace
{
const char *const attackModes[] = {"Fast", "Slow"};
const char *const variants[] = {"M102", "Mini", "Script76"};
const char *const qualities[] = {"2x", "4x", "Off"};
const char *const expressions[] = {"Sensitivity", "Output", "Off"};
const char *const channels[] = {"Dual mono", "Mono sum"};
const char *const switches[] = {"Off", "On"};
const ParamDesc params[] = {
    common::continuous("mxr_dynacomp_sensitivity", "SENSITIVITY", "", 0, 1, .5, -1, Taper::Skew, .5),
    common::continuous("mxr_dynacomp_output", "OUTPUT", "", 0, 1, .5, -1, Taper::Skew, .5),
    common::choice("mxr_dynacomp_attack", "ATTACK", attackModes, 2, 0, -1),
    common::engage("mxr_dynacomp_bypass", -1),
    common::choice("mxr_dynacomp_variant", "VARIANT", variants, 3, 0, -1, PF_Config),
    common::choice("mxr_dynacomp_noise", "NOISE", switches, 2, 1, -1, PF_Config),
    common::choice("mxr_dynacomp_quality", "QUALITY", qualities, 3, 0, -1, PF_Config),
    common::choice("mxr_dynacomp_expr_target", "EXPRESSION", expressions, 3, 0, -1, PF_Ext),
    common::continuous("mxr_dynacomp_mix", "MIX", "", 0, 1, 1, -1),
    common::choice("mxr_dynacomp_stereo_mode", "STEREO", channels, 2, 0, -1, PF_Config)};
const PedalDesc description = {"mxr-dynacomp",
                               "mxr_dynacomp_",
                               1,
                               "Dyna Comp-style (estimated)",
                               "Dynamics",
                               "MXR Dyna Comp",
                               ChannelMode::Mono,
                               BypassMode::True,
                               10,
                               params,
                               23,
                               false,
                               true,
                               false,
                               500,
                               1000};
} // namespace
const PedalDesc &MxrDynaComp::descriptor() noexcept { return description; }
MxrDynaComp::MxrDynaComp() : Processor(description) {}
void MxrDynaComp::prepare(double rate, int, int policy)
{
    prepareCommon(rate, policy, false);
    const int quality = (int)value(6);
    for (auto &o : otaOs)
        o.prepareFactor(sr, quality == 2 ? 1 : quality == 1 && policy > 0 ? 4 : 2);
    releaseAtZero = common::EnvelopeFollower::coefficient(otaOs[0].rate(), 1000.0);
    releaseAtFloor = common::EnvelopeFollower::coefficient(otaOs[0].rate(), 300.0);
    for (int c = 0; c < 2; ++c)
    {
        hp[c].setLowpass(sr, 5);
        lp[c].setLowpass(sr, 18000);
        dc[c].prepare(sr, 1);
        dry[c].prepare(23);
    }
    reset();
}
void MxrDynaComp::reset() noexcept
{
    resetCommon();
    for (auto &o : otaOs)
        o.reset();
    env.fill(0);
    previousAttack = previousOutput = -1;
    reduction.store(0, std::memory_order_relaxed);
    for (int c = 0; c < 2; ++c)
    {
        hp[c].reset();
        lp[c].reset();
        dc[c].reset();
        dry[c].reset();
        noise[c].reset(seed);
    }
}
void MxrDynaComp::setExpression(int jack, float v) noexcept
{
    if (jack != 0 || !std::isfinite(v))
        return;
    const int target = (int)value(7);
    if (target < 2)
        setParam(target, std::clamp(v, 0.0f, 1.0f));
}
void MxrDynaComp::process(double *l, double *r, int n) noexcept
{
    if (!l || !r)
        return;
    double minimumControl = 1;
    for (int i = 0; i < n; ++i)
    {
        tick();
        if ((clock & 7) == 0)
        { // DOCUMENTED-secondary 5ms; Mini Slow 15ms ESTIMATED.
            const double ms = 5 + 10 * p[2] * (value(4) == 1 ? 1 : 0);
            if (ms != previousAttack)
            {
                attack = common::EnvelopeFollower::coefficient(otaOs[0].rate(), ms);
                previousAttack = ms;
            }
        }
        const double sens = 1 + 99 * p[0] * p[0], kc = 20 + 40 * p[0]; // ESTIMATED §4.
        if (p[1] != previousOutput)
        {
            outputGain = p[1] <= 0 ? 0 : common::dbGain(-60 + 80 * p[1]);
            previousOutput = p[1];
        }
        double inputPair[] = {input(l[i]), input(r[i])};
        if (value(9) >= .5)
            inputPair[0] = inputPair[1] = (inputPair[0] + inputPair[1]) * .5;
        double *output[] = {l, r};
        for (int c = 0; c < 2; ++c)
        {
            const double x = inputPair[c];
            dry[c].push(x);
            const double clean = getLatencySamples() ? dry[c].read(getLatencySamples()) : x;
            const double y = otaOs[c].process(
                hp[c].highpass(x),
                [&](double v)
                {
                    const double control = std::max(common::dbGain(-36), 1 / (1 + kc * env[c]));
                    // 2% asymmetry ESTIMATED. Subtraction keeps silence exactly zero.
                    const double linear = common::mildTanh(1.5 * v * control), bias = std::tanh(.02);
                    const double ota = linear * (1 - bias * bias) / (1 + linear * bias) / 1.5;
                    const double rect = std::max(0.0, std::abs(ota * sens) - .6);
                    // Secondary-source 1s recovery; program-dependent shortening ESTIMATED.
                    const double releaseMs = std::max(300.0, 1000 / (1 + 4 * rect));
                    // Exactly the same coefficient at the two frequent endpoints.
                    // The continuously varying middle range keeps the original exp.
                    const double release =
                        releaseMs == 1000.0 ? releaseAtZero
                        : releaseMs == 300.0
                            ? releaseAtFloor
                            : common::EnvelopeFollower::coefficient(otaOs[c].rate(), releaseMs);
                    const double coeff = rect > env[c] ? attack : release;
                    env[c] = std::clamp(common::safe(rect + coeff * (env[c] - rect)), 0.0, 1000.0);
                    minimumControl = std::min(minimumControl, control);
                    return ota;
                });
            double wet = dc[c].process(lp[c].process(y * outputGain));
            if (value(5) >= .5)
                wet += noise[c].next() * common::dbGain(-90 + 15 * p[0]);
            output[c][i] = guard(clean + (wet - clean) * p[8]);
        }
        advance();
    }
    reduction.store(-20 * std::log10(minimumControl), std::memory_order_relaxed);
}
} // namespace luthier::reference::models
