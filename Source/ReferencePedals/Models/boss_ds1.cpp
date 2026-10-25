// OWNER: WP11
#include "boss_ds1.h"
namespace luthier::reference::models
{
const PedalDesc &BossDs1::descriptor() noexcept
{
    static const char *const stereo[] = {"Dual mono", "Sum to mono"};
    static const char *const revisions[] = {"Modern", "TA7136 (unavailable)"};
    static const ParamDesc params[] = {
        knob("boss_ds1_dist", "DIST", 16),
        knob("boss_ds1_tone", "TONE", 17),
        knob("boss_ds1_level", "LEVEL", 18, 15),
        common::engage("boss_ds1_on", 19),
        toggle("boss_ds1_hq", "HQ", true),
        common::choice("boss_ds1_stereo_mode", "STEREO", stereo, 2, 0, -1, PF_Config),
        common::choice("boss_ds1_revision", "REVISION (unavailable)", revisions, 2, 0, -1,
                       PF_Config | PF_Hidden),
        toggle("boss_ds1_noise", "NOISE", true)};
    static const PedalDesc d = {"boss-ds1",
                                "boss_ds1_",
                                1,
                                "Distortion (DS-1 style)",
                                "Distortion",
                                "DS-1 circuit",
                                ChannelMode::Stereo,
                                BypassMode::Buffered,
                                8,
                                params,
                                140,
                                false,
                                true,
                                false,
                                350};
    return d;
}
BossDs1::BossDs1() : CircuitPedal(descriptor()) {}
void BossDs1::prepare(double rate, int, int policy)
{
    prepareCircuit(rate, policy >= 2 ? 8 : (value(4) < .5f || policy == 0 || rate >= 96000 ? 2 : 4));
    for (int c = 0; c < 2; ++c)
    {
        inputHp[c].setLowpass(sr, 2);
        boosterHp[c].setLowpass(os[c].rate(), 33);
        boosterOutputHp[c].setLowpass(os[c].rate(), 3.3);
        ampHp[c].setLowpass(os[c].rate(), 23);
        amplifier[c].prepare(os[c].rate(), 4700, 1e-6, 100100, 250e-12, true);
        diode[c].configure(2 * 2.52e-9 * 2200 / (1 + 2200. / 15000), 1.752 * .02585);
        postClip[c].setLowpass(os[c].rate(), 20000);
        outputHp[c].setLowpass(sr, 5);
        toneLp[c].setLowpass(sr, 1 / (2 * common::pi * 6800 * .1e-6));
        toneHp[c].setLowpass(sr, 1 / (2 * common::pi * 2200 * .022e-6));
    }
    reset();
}
void BossDs1::update() noexcept
{
    const double pot = p[0] * p[0], rf = 100 + 100000 * pot, rg = 4700 + 100000 * (1 - pot);
    for (auto &a : amplifier)
        a.configure(rg, 1e-6, rf, 250e-12, true);
}
void BossDs1::reset() noexcept
{
    resetCircuit();
    noise.reset(seed);
    for (int c = 0; c < 2; ++c)
    {
        inputHp[c].reset();
        boosterHp[c].reset();
        boosterOutputHp[c].reset();
        ampHp[c].reset();
        amplifier[c].reset();
        diode[c].reset();
        postClip[c].reset();
        toneLp[c].reset();
        toneHp[c].reset();
        outputHp[c].reset();
    }
    update();
}
void BossDs1::process(double *l, double *r, int n) noexcept
{
    if (!l || !r)
        return;
    for (int i = 0; i < n; ++i, ++clock)
    {
        tick();
        if ((clock & 15) == 0)
            update();
        double inputs[] = {bounded(l[i]), bounded(r[i])};
        if (p[5] >= .5)
            inputs[0] = inputs[1] = .5 * (inputs[0] + inputs[1]);
        double *channels[] = {l, r};
        for (int c = 0; c < 2; ++c)
        {
            double x = .5 * inputHp[c].highpass(inputs[c]);
            if (p[7] >= .5 && p[0] > .5)
                x += 3.5e-6 * std::sqrt(3.) * noise.next();
            double y =
                os[c].process(x,
                              [&](double v)
                              {
                                  v = 56 * boosterHp[c].highpass(v);
                                  v = 4.5 * std::tanh((v + .08 * v * v) / 4.5);
                                  v = ampHp[c].highpass(boosterOutputHp[c].highpass(v));
                                  v = 4 * std::tanh(amplifier[c].process(v) / 4);
                                  return postClip[c].process(diode[c].process(v / (1 + 2200. / 15000)));
                              });
            y = (1 - p[1]) * toneLp[c].process(y) + p[1] * toneHp[c].highpass(y);
            channels[c][i] = bounded(2 * outputHp[c].highpass(y * 5 * p[2] * p[2]));
        }
    }
}
} // namespace luthier::reference::models
