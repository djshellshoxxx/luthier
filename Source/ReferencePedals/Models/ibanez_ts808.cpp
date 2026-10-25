// OWNER: WP11
#include "ibanez_ts808.h"
namespace luthier::reference::models
{
const PedalDesc &IbanezTs808::descriptor() noexcept
{
    static const char *const stereo[] = {"Dual mono", "Sum to mono"};
    static const char *const chips[] = {"JRC4558D", "TL072 (unavailable)", "OPA2134 (unavailable)"};
    static const ParamDesc params[] = {
        knob("ibanez_ts808_drive", "DRIVE", 16),
        knob("ibanez_ts808_tone", "TONE", 17),
        knob("ibanez_ts808_level", "LEVEL", 18, 15),
        common::engage("ibanez_ts808_on", 19),
        toggle("ibanez_ts808_hq", "HQ", true),
        common::choice("ibanez_ts808_stereo_mode", "STEREO", stereo, 2, 0, -1, PF_Config),
        common::continuous("ibanez_ts808_xfade_ms", "BYPASS FADE (unavailable)", "ms", 1, 30, 5, -1),
        toggle("ibanez_ts808_noise", "NOISE", true),
        common::choice("ibanez_ts808_chip", "CHIP (unavailable)", chips, 3, 0, -1, PF_Config | PF_Hidden)};
    static const PedalDesc d = {"ibanez-ts808",
                                "ibanez_ts808_",
                                1,
                                "Overdrive (TS808 style)",
                                "Overdrive",
                                "TS808 circuit",
                                ChannelMode::Stereo,
                                BypassMode::Buffered,
                                9,
                                params,
                                140,
                                false,
                                true,
                                false,
                                400};
    return d;
}
IbanezTs808::IbanezTs808() : CircuitPedal(descriptor()) {}
void IbanezTs808::prepare(double rate, int, int policy)
{
    const int factor = policy >= 2 ? 8 : (value(4) < .5f || policy == 0 || rate >= 96000 ? 2 : 4);
    prepareCircuit(rate, factor);
    for (int c = 0; c < 2; ++c)
    {
        inputHp[c].setLowpass(sr, 15.6);
        couplingHp[c].setLowpass(sr, 16);
        outputHp[c].setLowpass(sr, 5);
        postClip[c].setLowpass(os[c].rate(), 20000);
        clip[c].prepare(os[c].rate());
    }
    reset();
}
void IbanezTs808::update() noexcept
{
    const double rf = feedbackResistance(p[0]);
    for (int c = 0; c < 2; ++c)
    {
        clip[c].setFeedbackResistance(rf);
        gbw[c].setLowpass(os[c].rate(), 3e6 / (1 + rf / 4700));
        toneLp[c].setLowpass(sr, 723 * std::pow(3200.0 / 723, p[1]));
        shelf[c].set(sr, 1600, -6 + 12 * p[1], true);
    }
}
void IbanezTs808::reset() noexcept
{
    resetCircuit();
    random = seed ? seed : 1;
    for (int c = 0; c < 2; ++c)
    {
        inputHp[c].reset();
        couplingHp[c].reset();
        gbw[c].reset();
        postClip[c].reset();
        toneLp[c].reset();
        outputHp[c].reset();
        shelf[c].reset();
        clip[c].reset();
    }
    update();
}
void IbanezTs808::process(double *l, double *r, int n) noexcept
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
            double x = .5 * std::pow(10.0, -.1 / 20) * couplingHp[c].highpass(inputHp[c].highpass(inputs[c]));
            if (p[7] >= .5 && p[0] > .5)
            {
                random ^= random << 13;
                random ^= random >> 17;
                random ^= random << 5;
                x += .5 * 5.477225575051661e-5 * (2 * (double)random / 4294967295.0 - 1);
            }
            double y = os[c].process(
                x, [&](double v)
                { return postClip[c].process(gbw[c].process(3.8 * std::tanh(clip[c].process(v) / 3.8))); });
            y = shelf[c].process(toneLp[c].process(y)) * 4 * p[2] * p[2];
            channels[c][i] = bounded(2 * outputHp[c].highpass(3 * std::tanh(y / 3)));
        }
    }
}
} // namespace luthier::reference::models
