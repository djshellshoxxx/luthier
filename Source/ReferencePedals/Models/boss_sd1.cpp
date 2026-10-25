// OWNER: WP11
#include "boss_sd1.h"
namespace luthier::reference::models
{
const PedalDesc &BossSd1::descriptor() noexcept
{
    static const char *const quality[] = {"Low CPU (unavailable)", "High", "Eco (unavailable)"};
    static const char *const diodes[] = {"Stock 1S2473 estimate", "1N4148", "Germanium mod", "LED mod"};
    static const char *const at[] = {"Off", "Drive", "Tone", "Level"};
    static const char *const exp[] = {"Drive", "Tone", "Level"};
    static const std::array<ParamDesc, 15> params = []
    {
        std::array<ParamDesc, 15> a{
            common::continuous("boss_sd1_drive", "DRIVE", "", 0, 10, 3, 14),
            common::continuous("boss_sd1_tone", "TONE", "", 0, 10, 5, 15),
            common::continuous("boss_sd1_level", "LEVEL", "", 0, 10, 5, 16),
            common::engage("boss_sd1_bypass", 17),
            common::choice("boss_sd1_quality", "QUALITY", quality, 3, 1, -1, PF_Config),
            common::choice("boss_sd1_diode_set", "DIODES", diodes, 4, 0, -1),
            common::continuous("boss_sd1_lowcut_hz", "LOW CUT", "Hz", 340, 720, 720, -1, Taper::LogA),
            toggle("boss_sd1_buffer_colour", "BUFFER COLOUR (unavailable)", true, PF_Automatable),
            common::continuous("boss_sd1_vel_to_level", "VELOCITY LEVEL", "dB", -12, 12, 0, -1),
            common::continuous("boss_sd1_vel_to_drive", "VELOCITY DRIVE", "", -1, 1, 0, -1),
            common::choice("boss_sd1_at_target", "PRESSURE TARGET", at, 4, 0, -1, PF_Ext),
            common::continuous("boss_sd1_at_depth", "PRESSURE DEPTH", "", -1, 1, 0, -1),
            common::choice("boss_sd1_exp_target", "EXPRESSION TARGET", exp, 3, 0, -1, PF_Ext),
            common::continuous("boss_sd1_exp_min", "EXPRESSION HEEL", "", 0, 1, 0, -1),
            common::continuous("boss_sd1_exp_max", "EXPRESSION TOE", "", 0, 1, 1, -1)};
        for (int i : {0, 1, 2, 5, 6})
            a[i].flags |= PF_OwnSmoothing;
        for (int i : {8, 9, 11, 13, 14})
            a[i].flags = PF_Ext | PF_OwnSmoothing;
        a[2].smoothMs = 10;
        a[5].smoothMs = 50;
        return a;
    }();
    static const PedalDesc d = {"boss-sd1",
                                "boss_sd1_",
                                1,
                                "Asymmetric Overdrive (SD-1 style)",
                                "Overdrive",
                                "SD-1 circuit",
                                ChannelMode::Stereo,
                                BypassMode::Buffered,
                                15,
                                params.data(),
                                140,
                                false,
                                true,
                                true,
                                1200};
    return d;
}
BossSd1::BossSd1() : CircuitPedal(descriptor()) {}
void BossSd1::setParam(int index, float norm) noexcept
{
    if (index == 4 && (!std::isfinite(norm) || std::lround(std::clamp(norm, 0.f, 1.f) * 2) != 1))
        return;
    if (index >= 0 && index < 3 && std::isfinite(norm) && norm != getParam(index))
        expressionActive = false;
    PedalBase<15>::setParam(index, norm);
}
void BossSd1::prepare(double rate, int, int policy)
{
    // High circuit only. The separate lowcpu ADAA and eco LUT paths are absent.
    prepareCircuit(rate, policy >= 2 ? 8 : (rate >= 176400 ? 1 : (rate >= 88200 ? 2 : 4)));
    coefficient20 = -std::expm1(-1 / (sr * .020));
    coefficient10 = -std::expm1(-1 / (sr * .010));
    coefficient15 = -std::expm1(-1 / (sr * .015));
    coefficient80 = -std::expm1(-1 / (sr * .080));
    for (int c = 0; c < 2; ++c)
    {
        inputHp[c].setLowpass(sr, 7);
        outputHp[c].setLowpass(sr, 15);
        postClip[c].setLowpass(os[c].rate(), 12000);
        for (auto &stage : clip[c])
            stage.prepare(os[c].rate(), Sd1CircuitParams::gainLegR, Sd1CircuitParams::stockGainLegC,
                          Sd1CircuitParams::feedbackC);
    }
    reset();
}
void BossSd1::reset() noexcept
{
    resetCircuit();
    rf = 27000 + 1e6 * value(0) / 10;
    tone = value(1) / 10;
    levelGain = std::expm1(3 * value(2) / 10) / std::expm1(1.65);
    velocity = heldVelocity = pressure = pressureTarget = 0;
    expressionActive = false;
    fadeRemaining = 0;
    const int selected = (int)value(5);
    diodeTarget = selected < 2 ? 0 : selected - 1;
    weights.fill(0);
    weights[diodeTarget] = 1;
    weightStep.fill(0);
    for (int c = 0; c < 2; ++c)
    {
        inputHp[c].reset();
        outputHp[c].reset();
        gbw[c].reset();
        postClip[c].reset();
        shelf[c].reset();
        for (auto &stage : clip[c])
            stage.reset();
    }
    update();
}
void BossSd1::onFoot(const FootEvent &e) noexcept
{
    heldVelocity = e.press ? 2 * (std::clamp((double)common::safe(e.speed), 0., 1.) - .5) : 0;
}
void BossSd1::onMidi(const MidiEvent &e) noexcept
{
    const int type = e.status & 0xf0;
    if (type == 0xd0 || type == 0xa0)
    {
        const double next = std::min(127, (int)(type == 0xd0 ? e.d1 : e.d2)) / 127.;
        if (std::abs(next - pressureTarget) >= 2. / 127)
            pressureTarget = next;
    }
}
void BossSd1::setExpression(int jack, float value) noexcept
{
    if (jack < 0 || jack > 1 || !std::isfinite(value))
        return;
    expression = std::clamp((double)value, 0., 1.);
    expressionActive = true;
}
void BossSd1::controls() noexcept
{
    tick();
    velocity += (heldVelocity != 0 ? coefficient10 : coefficient80) * (heldVelocity - velocity);
    pressure += coefficient15 * (pressureTarget - pressure);
    double targets[] = {value(0) / 10., value(1) / 10., value(2) / 10.};
    if (expressionActive)
        targets[(int)value(12)] = p[13] + expression * (p[14] - p[13]);
    targets[0] += velocity * p[9];
    const int at = (int)value(10);
    if (at > 0)
        targets[at - 1] += pressure * p[11];
    for (auto &target : targets)
        target = std::clamp(target, 0., 1.);
    rf += coefficient20 * (27000 + 1e6 * targets[0] - rf);
    tone += coefficient20 * (targets[1] - tone);
    const double levelTarget =
        std::expm1(3 * targets[2]) / std::expm1(1.65) * common::dbGain(.5 * velocity * p[8]);
    levelGain += coefficient10 * (levelTarget - levelGain);
    const int selected = (int)value(5), next = selected < 2 ? 0 : selected - 1;
    if (next != diodeTarget)
    {
        diodeTarget = next;
        fadeRemaining = std::max(1, (int)std::lround(.05 * sr));
        for (int k = 0; k < 3; ++k)
            weightStep[k] = ((k == next ? 1. : 0.) - weights[k]) / fadeRemaining;
    }
    if (fadeRemaining > 0)
    {
        for (int k = 0; k < 3; ++k)
            weights[k] += weightStep[k];
        if (--fadeRemaining == 0)
        {
            weights.fill(0);
            weights[diodeTarget] = 1;
        }
    }
}
void BossSd1::update() noexcept
{
    const double cg = 1 / (2 * common::pi * 4700 * p[6]);
    for (int c = 0; c < 2; ++c)
    {
        for (int k = 0; k < 3; ++k)
        {
            const auto diode = Sd1CircuitParams::diodes[k];
            clip[c][k].configure(cg, rf, diode.is, diode.n * .02585);
        }
        shelf[c].set(os[c].rate(), 1200 + 800 * tone, (tone - .5) * 18, true);
        gbw[c].setLowpass(os[c].rate(), 3e6 / (1 + rf / 4700));
    }
}
void BossSd1::process(double *l, double *r, int n) noexcept
{
    if (!l || !r)
        return;
    for (int i = 0; i < n; ++i, ++clock)
    {
        controls();
        if ((clock & 15) == 0)
            update();
        double *channels[] = {l, r};
        for (int c = 0; c < 2; ++c)
        {
            const double x = inputHp[c].highpass(std::clamp(common::safe(channels[c][i]) * 2, -8., 8.));
            double y = os[c].process(x,
                                     [&](double v)
                                     {
                                         double z = 0;
                                         for (int k = 0; k < 3; ++k)
                                             z += weights[k] * clip[c][k].process(v);
                                         z = 4.2 * std::tanh(z / 4.2);
                                         return postClip[c].process(shelf[c].process(gbw[c].process(z)));
                                     });
            channels[c][i] = bounded(.5 * outputHp[c].highpass(y * levelGain));
        }
    }
}
} // namespace luthier::reference::models
