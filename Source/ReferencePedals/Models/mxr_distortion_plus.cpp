// OWNER: WP12
#include "mxr_distortion_plus.h"
namespace luthier::reference::models
{
const PedalDesc &MxrDistortionPlus::descriptor() noexcept
{
    static const char *const variants[] = {"M104 reissue estimate", "Silicon mod"};
    static const char *const quality[] = {"Eco (unavailable)", "HQ"};
    static const char *const stereo[] = {"Dual mono", "Sum to mono"};
    static const ParamDesc p[] = {
        common::engage("mxr_distortion_plus_bypass", -1),
        knob("mxr_distortion_plus_dist", "DISTORTION", -1),
        knob("mxr_distortion_plus_out", "OUTPUT", -1),
        common::choice("mxr_distortion_plus_variant", "VARIANT", variants, 2, 0, -1, PF_Config),
        common::choice("mxr_distortion_plus_quality", "QUALITY", quality, 2, 1, -1, PF_Config),
        common::choice("mxr_distortion_plus_stereo_mode", "STEREO", stereo, 2, 0, -1, PF_Config),
        toggle("mxr_distortion_plus_noise", "NOISE", false)};
    static const PedalDesc d = {"mxr-distortion-plus",
                                "mxr_distortion_plus_",
                                1,
                                "Distortion Plus style",
                                "Distortion",
                                "MXR Distortion+",
                                ChannelMode::Stereo,
                                BypassMode::True,
                                7,
                                p,
                                140,
                                false,
                                true,
                                false,
                                800,
                                1000};
    return d;
}
MxrDistortionPlus::MxrDistortionPlus() : CircuitPedal(descriptor()) {}
void MxrDistortionPlus::setParam(int i, float n) noexcept
{
    if (i == 4 && (!std::isfinite(n) || n < .5f))
        return;
    PedalBase<7>::setParam(i, n);
}
void MxrDistortionPlus::prepare(double rate, int, int policy)
{
    prepareCircuit(rate, policy >= 2 ? 8 : 4);
    variant.prepare(sr, 20);
    noiseScale = std::sqrt((20e-9 * 20e-9 + 4 * 1.380649e-23 * 300 * 4700) * sr * .5 * 3);
    for (int c = 0; c < 2; ++c)
    {
        inputHp[c].setLowpass(sr, 30);
        amplifier[c].prepare(os[c].rate(), 4700, 47e-9, 10000, 47e-12);
        postClip[c].setLowpass(os[c].rate(), 1 / (2 * common::pi * 10000 * 1e-9));
    }
    diode[0].configure(2 * 1e-6 * 10000, 1.3 * .02585);
    diode[1].configure(2 * 2.52e-9 * 10000, 1.752 * .02585);
    reset();
}
void MxrDistortionPlus::update() noexcept
{
    const double rf = driveResistance(p[1]);
    for (int c = 0; c < 2; ++c)
    {
        amplifier[c].configure(4700, 47e-9, rf, 47e-12);
        gbw[c].setLowpass(os[c].rate(), 1e6 / (1 + rf / 4700));
    }
}
void MxrDistortionPlus::reset() noexcept
{
    resetCircuit();
    variant.reset(value(3));
    noise.reset(seed);
    for (int c = 0; c < 2; ++c)
    {
        inputHp[c].reset();
        amplifier[c].reset();
        gbw[c].reset();
        postClip[c].reset();
        diode[c].reset();
    }
    update();
}
void MxrDistortionPlus::process(double *l, double *r, int n) noexcept
{
    if (!l || !r)
        return;
    for (int i = 0; i < n; ++i, ++clock)
    {
        tick();
        if ((clock & 31) == 0)
            update();
        variant.setTarget(value(3));
        const double blend = variant.next(), gain = outputGain(p[2]);
        double input[] = {bounded(l[i]), bounded(r[i])};
        if (p[5] >= .5)
            input[0] = input[1] = .5 * (input[0] + input[1]);
        double *channels[] = {l, r};
        for (int c = 0; c < 2; ++c)
        {
            double x = .5 * inputHp[c].highpass(input[c]);
            if (p[6] >= .5)
                x += noiseScale * noise.next();
            double y = os[c].process(x,
                                     [&](double v)
                                     {
                                         v = 3 * std::tanh(gbw[c].process(amplifier[c].process(v)) / 3);
                                         const double ge = diode[0].process(v), si = diode[1].process(v);
                                         return postClip[c].process(ge + (si - ge) * blend);
                                     });
            channels[c][i] = bounded(2 * y * gain);
        }
    }
}
} // namespace luthier::reference::models
