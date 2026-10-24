// OWNER: WP13
#include "ehx_green_russian.h"
namespace luthier::reference::models
{
namespace
{
double pot(double x) { return std::expm1(std::log(100.) * std::clamp(x, 0., 1.)) / 99; }
} // namespace
const PedalDesc &EhxGreenRussian::descriptor() noexcept
{
    static const char *const stereo[] = {"Dual mono", "Sum to mono"};
    static const std::array<ParamDesc, 8> p = []
    {
        std::array<ParamDesc, 8> a{
            knob("ehx_green_russian_sustain", "SUSTAIN", 14, 30),
            knob("ehx_green_russian_tone", "TONE", 15, 30),
            knob("ehx_green_russian_volume", "VOLUME", 16, 30),
            common::engage("ehx_green_russian_bypass", 17),
            common::choice("ehx_green_russian_stereo_mode", "STEREO", stereo, 2, 0, -1, PF_Config),
            common::choice("ehx_green_russian_os_factor", "OVERSAMPLING", common::qualityNames, 3, 1, -1,
                           PF_Config),
            common::continuous("ehx_green_russian_at_to_sustain", "PRESSURE SUSTAIN", "", 0, 1, 0, -1),
            toggle("ehx_green_russian_input_load", "100K LOAD (unavailable)", false)};
        a[6].flags = PF_Ext | PF_OwnSmoothing;
        return a;
    }();
    static const PedalDesc d = {"ehx-green-russian",
                                "ehx_green_russian_",
                                1,
                                "Green Russian Fuzz style",
                                "Fuzz",
                                "EHX Green Russian Big Muff",
                                ChannelMode::Stereo,
                                BypassMode::True,
                                8,
                                p.data(),
                                140,
                                false,
                                true,
                                true,
                                1500,
                                1000};
    return d;
}
EhxGreenRussian::EhxGreenRussian() : CircuitPedal(descriptor()) {}
void EhxGreenRussian::prepare(double rate, int, int policy)
{
    prepareCircuit(rate, 1 << (1 + std::clamp(std::min(policy, (int)value(5)), 0, 2)));
    pressureCoefficient = -std::expm1(-1 / (sr * .02));
    for (int c = 0; c < 2; ++c)
    {
        auto &v = channels[c];
        const double osr = os[c].rate();
        v.inHp.setLowpass(osr, 16);
        v.q1Dc.setLowpass(osr, 10);
        v.coupling2.setLowpass(osr, 16);
        v.feedback1.setLowpass(osr, 1 / (2 * common::pi * 470000 * 470e-12));
        v.feedback2.setLowpass(osr, 1 / (2 * common::pi * 470000 * 470e-12));
        v.toneLp.setLowpass(osr, 1 / (2 * common::pi * 22000 * 3.9e-9));
        v.toneHp.setLowpass(osr, 1 / (2 * common::pi * 22000 * 10e-9));
        v.outDc.setLowpass(osr, 10);
    }
    reset();
}
void EhxGreenRussian::reset() noexcept
{
    resetCircuit();
    pressure = pressureTarget = 0;
    smoothedSustain = std::log(std::max(1e-5, pot(value(0))));
    smoothedVolume = std::log(std::max(1e-5, pot(value(2))));
    for (auto &v : channels)
    {
        v.inHp.reset();
        v.q1Dc.reset();
        v.coupling1.reset();
        v.coupling2.reset();
        v.feedback1.reset();
        v.feedback2.reset();
        v.toneLp.reset();
        v.toneHp.reset();
        v.outDc.reset();
    }
    update();
}
void EhxGreenRussian::onMidi(const MidiEvent &e) noexcept
{
    if ((e.status & 0xf0) == 0xd0)
        pressureTarget = std::min(127, (int)e.d1) / 127.;
    if ((e.status & 0xf0) == 0xa0)
        pressureTarget = std::min(127, (int)e.d2) / 127.;
}
void EhxGreenRussian::update() noexcept
{
    const double sustain = std::exp(smoothedSustain);
    driveGain = 1 + 99 * sustain;
    volumeGain = 4 * std::max(0., std::exp(smoothedVolume) - 1e-5);
    toneLoss = common::dbGain(-9 - 44 * p[1] * (1 - p[1]));
    for (int c = 0; c < 2; ++c)
        channels[c].coupling1.setLowpass(os[c].rate(), 100 - 90 * sustain);
}
void EhxGreenRussian::process(double *l, double *r, int n) noexcept
{
    if (!l || !r)
        return;
    const double control = -std::expm1(-1 / (sr * .03));
    for (int i = 0; i < n; ++i, ++clock)
    {
        tick();
        pressure += pressureCoefficient * (pressureTarget - pressure);
        smoothedSustain +=
            control *
            (std::log(std::max(1e-5, pot(std::clamp((double)value(0) + .3 * pressure * p[6], 0., 1.)))) -
             smoothedSustain);
        smoothedVolume += control * (std::log(std::max(1e-5, pot(value(2)))) - smoothedVolume);
        if ((clock & 31) == 0)
            update();
        double input[] = {bounded(l[i]), bounded(r[i])};
        if (p[4] >= .5)
            input[0] = input[1] = .5 * (input[0] + input[1]);
        double *dest[] = {l, r};
        for (int c = 0; c < 2; ++c)
        {
            auto &v = channels[c];
            const double y = os[c].process(
                input[c],
                [&](double x)
                {
                    x = v.q1Dc.highpass(std::tanh(10 * v.inHp.highpass(x) + .15) - std::tanh(.15));
                    x = v.coupling1.highpass(x);
                    x = v.feedback1.process(1.9 * .02585 * std::asinh(x * driveGain / (2 * 2.5e-9 * 470000)));
                    x = v.coupling2.highpass(x);
                    x = v.feedback2.process(1.9 * .02585 * std::asinh(x * 47 / (2 * 2.5e-9 * 470000)));
                    x = ((1 - p[1]) * v.toneLp.process(x) + p[1] * v.toneHp.highpass(x)) * toneLoss;
                    return v.outDc.highpass(std::tanh(10 * x + .15) - std::tanh(.15)) * volumeGain;
                });
            dest[c][i] = bounded(y);
        }
    }
}
} // namespace luthier::reference::models
