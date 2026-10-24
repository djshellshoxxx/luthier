// OWNER: WP13
#include "ehx_deluxe_muff.h"
namespace luthier::reference::models
{
PreparedMemoryRequirement EhxDeluxeMuff::prepareMemory(double sr, int block, int policy,
                                                       const float *params) noexcept
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000 || block <= 0)
        return {};
    const auto &hq = descriptor().params[13];
    // Fresh PedalBase ignores nonfinite setters and retains the descriptor default.
    const double quality = params && std::isfinite(params[13]) ? hq.fromNormalised(params[13]) : hq.def;
    const int factor = policy >= 2 ? 8 : (quality < .5 || policy == 0 ? 2 : 4);
    const uint64_t latency = factor == 8 ? 140 : factor == 4 ? 136 : 128;
    return {sizeof(EhxDeluxeMuff) + 4 * (latency + 18) * sizeof(double), 0};
}
uint64_t EhxDeluxeMuff::getPreparedMemoryBytes() const noexcept
{
    uint64_t bytes = sizeof(*this);
    for (const auto &v : channels)
        bytes += v.attackDelay.ownedMemoryBytes() + v.detectorDelay.ownedMemoryBytes();
    return bytes;
}

const PedalDesc &EhxDeluxeMuff::descriptor() noexcept
{
    static const char *const q[] = {"LO", "HI"};
    static const char *const stereo[] = {"Dual mono", "Sum to mono"};
    static const char *const revision[] = {"Deluxe guitar estimate"};
    static const std::array<ParamDesc, 18> p = []
    {
        std::array<ParamDesc, 18> a{
            common::engage("ehx_deluxe_muff_bypass", 23),
            knob("ehx_deluxe_muff_volume", "VOLUME", 7, 10),
            knob("ehx_deluxe_muff_tone", "TONE", 15, 10),
            knob("ehx_deluxe_muff_sustain", "SUSTAIN", 14, 10),
            common::continuous("ehx_deluxe_muff_attack", "ATTACK", "", 0, 1, 0, 16),
            common::continuous("ehx_deluxe_muff_gate", "GATE", "", 0, 1, 0, 17),
            toggle("ehx_deluxe_muff_mids_on", "MIDS ON", false, PF_Automatable),
            common::continuous("ehx_deluxe_muff_mids_level", "MIDS LEVEL", "dB", -10, 10, 0, 18),
            common::continuous("ehx_deluxe_muff_mids_freq", "MIDS FREQ", "Hz", 310, 5000, 1245, 19,
                               Taper::LogA),
            common::choice("ehx_deluxe_muff_mids_q", "Q", q, 2, 0, 20),
            toggle("ehx_deluxe_muff_bass_boost", "BASS BOOST", false, PF_Automatable),
            common::continuous("ehx_deluxe_muff_exp", "EXPRESSION", "", 0, 1, 0, 4),
            toggle("ehx_deluxe_muff_exp_connected", "EXPRESSION CONNECTED", false, PF_Ext),
            toggle("ehx_deluxe_muff_hq", "HQ", true),
            toggle("ehx_deluxe_muff_true_bypass", "TRUE BYPASS", true),
            toggle("ehx_deluxe_muff_gate_link", "GATE LINK", false, PF_Automatable),
            common::choice("ehx_deluxe_muff_stereo_mode", "STEREO", stereo, 2, 0, -1, PF_Config),
            common::choice("ehx_deluxe_muff_revision", "REVISION", revision, 1, 0, -1,
                           PF_Config | PF_Hidden)};
        for (int i : {4, 5, 7})
            a[i].smoothMs = 10;
        a[11].flags = PF_Ext | PF_OwnSmoothing;
        a[11].smoothMs = 5;
        a[6].midiCcDefault = 22;
        a[10].midiCcDefault = 21;
        return a;
    }();
    static const PedalDesc d = {"ehx-deluxe-muff",
                                "ehx_deluxe_muff_",
                                1,
                                "Deluxe Muff (Big Muff style)",
                                "Fuzz",
                                "EHX Deluxe Big Muff Pi",
                                ChannelMode::Stereo,
                                BypassMode::True,
                                18,
                                p.data(),
                                140,
                                false,
                                true,
                                false,
                                2500};
    return d;
}
EhxDeluxeMuff::EhxDeluxeMuff() : CircuitPedal(descriptor()) {}
void EhxDeluxeMuff::prepare(double rate, int, int policy)
{
    prepareCircuit(rate, policy >= 2 ? 8 : (value(13) < .5 || policy == 0 ? 2 : 4));
    midsSwitch.prepare(sr, 10);
    bassSwitch.prepare(sr, 20);
    gateAttack = -std::expm1(-1 / (sr * .002));
    gateRelease = -std::expm1(-1 / (sr * .08));
    expressionCoefficient = -std::expm1(-1 / (sr * .005));
    for (int c = 0; c < 2; ++c)
    {
        auto &v = channels[c];
        v.inBuffer.setLowpass(sr, 2);
        v.bypassHp.setLowpass(sr, 2);
        v.outHp.setLowpass(sr, 5);
        v.attackHp.setLowpass(sr, 250);
        v.boostHp.setLowpass(os[c].rate(), 30);
        v.coupling1.setLowpass(os[c].rate(), 20);
        v.coupling2.setLowpass(os[c].rate(), 20);
        v.clipLp1.setLowpass(os[c].rate(), 1 / (2 * common::pi * 100000 * 470e-12));
        v.clipLp2.setLowpass(os[c].rate(), 1 / (2 * common::pi * 100000 * 470e-12));
        v.bass.setShelf(sr, 150, 8, false, .7);
        v.detector.prepare(sr, 1, 15);
        v.fast.prepare(sr, 1, 30);
        v.slow.prepare(sr, 20, 20);
        v.attackDelay.prepare(getLatencySamples() + 2);
        v.detectorDelay.prepare(getLatencySamples() + 2);
    }
    // Missing unpublished resistance values are explicit candidate estimates:
    // 10k source resistance, 100k shunt load. No hardware equivalence claim.
    diode.configure(2 * 2.52e-9 * 10000 / (1 + 10000. / 100000), 1.752 * .02585);
    reset();
}
void EhxDeluxeMuff::reset() noexcept
{
    resetCircuit();
    midsSwitch.reset(value(6));
    bassSwitch.reset(value(10));
    midsAmount = value(6);
    bassAmount = value(10);
    expressionFrequency = value(12) >= .5 ? 310 * std::pow(5000. / 310, value(11)) : value(8);
    for (auto &v : channels)
    {
        v.inBuffer.reset();
        v.bypassHp.reset();
        v.boostHp.reset();
        v.coupling1.reset();
        v.coupling2.reset();
        v.clipLp1.reset();
        v.clipLp2.reset();
        v.tone.reset();
        v.bass.reset();
        v.mids.reset();
        v.outHp.reset();
        v.attackHp.reset();
        v.detector.reset();
        v.fast.reset();
        v.slow.reset();
        v.attackDelay.reset();
        v.detectorDelay.reset();
        v.open = true;
        v.below = 0;
        v.gateGain = 1;
    }
    update();
}
void EhxDeluxeMuff::setExpression(int jack, float v) noexcept
{
    if (jack < 0 || jack > 1 || !std::isfinite(v))
        return;
    setParam(11, v);
    setParam(12, 1);
}
void EhxDeluxeMuff::update() noexcept
{
    boostGain = common::dbGain(6 + 26 * p[3] * p[3]);
    for (auto &v : channels)
    {
        v.tone.configure(sr, p[2], 39000, 10e-9, 22000, 3.9e-9);
        v.mids.setPeak(sr, expressionFrequency, p[7] * midsAmount, value(9) >= .5 ? 2.5 : .7);
    }
}
void EhxDeluxeMuff::processBypass(double *l, double *r, int n) noexcept
{
    if (!l || !r)
        return;
    for (int i = 0; i < n; ++i)
    {
        const double a = channels[0].bypassHp.highpass(bounded(l[i])),
                     b = channels[1].bypassHp.highpass(bounded(r[i]));
        if (value(14) < .5)
        {
            l[i] = a;
            r[i] = b;
        }
    }
}
void EhxDeluxeMuff::process(double *l, double *r, int n) noexcept
{
    if (!l || !r)
        return;
    for (int i = 0; i < n; ++i, ++clock)
    {
        tick();
        midsSwitch.setTarget(value(6));
        bassSwitch.setTarget(value(10));
        midsAmount = midsSwitch.next();
        bassAmount = bassSwitch.next();
        const double targetFrequency = value(12) >= .5 ? 310 * std::pow(5000. / 310, p[11]) : p[8];
        expressionFrequency += expressionCoefficient * (targetFrequency - expressionFrequency);
        if ((clock & 15) == 0)
            update();
        double input[] = {bounded(l[i]), bounded(r[i])};
        if (p[16] >= .5)
            input[0] = input[1] = .5 * (input[0] + input[1]);
        double detector[2]{}, attack[2]{}, buffer[2]{};
        for (int c = 0; c < 2; ++c)
        {
            auto &v = channels[c];
            buffer[c] = v.inBuffer.highpass(input[c]);
            const double fast = v.fast.process(buffer[c]), slow = v.slow.process(buffer[c]);
            const double transient = std::clamp((fast - slow) / std::max(slow, 1e-12), 0., 1.);
            v.attackDelay.push(std::tanh(4 * v.attackHp.highpass(.5 * buffer[c])) * transient);
            attack[c] = v.attackDelay.read(getLatencySamples());
            v.detectorDelay.push(v.detector.process(buffer[c]));
            detector[c] = v.detectorDelay.read(getLatencySamples());
        }
        if (p[15] >= .5)
            detector[0] = detector[1] = std::max(detector[0], detector[1]);
        double *dest[] = {l, r};
        for (int c = 0; c < 2; ++c)
        {
            auto &v = channels[c];
            const double threshold = common::dbGain(-75 + 45 * p[5]);
            if (p[5] <= 1e-12)
            {
                v.open = true;
                v.below = 0;
                v.gateGain = 1;
            }
            else
            {
                if (detector[c] >= threshold)
                {
                    v.open = true;
                    v.below = 0;
                }
                else if (detector[c] < threshold * common::dbGain(-4))
                {
                    if (++v.below >= std::lround(.02 * sr))
                    {
                        v.open = false;
                        v.below = std::lround(.02 * sr);
                    }
                }
                else
                    v.below = 0;
                const double target = v.open ? 1 : .001;
                v.gateGain += (target > v.gateGain ? gateAttack : gateRelease) * (target - v.gateGain);
            }
            double y = os[c].process(.5 * buffer[c],
                                     [&](double x)
                                     {
                                         x = v.boostHp.highpass(x) * boostGain;
                                         x = 4.5 * std::tanh((x + .06 * x * x) / 4.5);
                                         x = diode.process(v.clipLp1.process(v.coupling1.highpass(x)) *
                                                           31.6227766017 / (1 + 10000. / 100000));
                                         return diode.process(v.clipLp2.process(v.coupling2.highpass(x)) *
                                                              31.6227766017 / (1 + 10000. / 100000));
                                     });
            y = v.tone.process(y);
            const double boosted = v.bass.process(y);
            y += bassAmount * (boosted - y);
            y = 4.5 * std::tanh(12.5892541179 * y / 4.5);
            y = v.mids.process(y) * v.gateGain;
            y += std::pow(p[4], 1.5) * .7 * attack[c];
            dest[c][i] = bounded(2 * v.outHp.highpass(y * 10 * p[1] * p[1]));
        }
    }
}
} // namespace luthier::reference::models
