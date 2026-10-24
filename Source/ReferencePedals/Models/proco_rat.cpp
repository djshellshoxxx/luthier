// OWNER: WP12
#include "proco_rat.h"
namespace luthier::reference::models
{
namespace
{
constexpr double voltsPerFs = 1.228345603;
}
const PedalDesc &ProcoRat::descriptor() noexcept
{
    static const char *const variants[] = {"Classic LM308 estimate", "OP07 estimate", "LED mod"};
    static const char *const modes[] = {"Sum to mono", "Left only", "Right only", "Dual mono"};
    static const char *const targets[] = {"Distortion", "Filter", "Volume"};
    static const std::array<ParamDesc, 11> p = []
    {
        std::array<ParamDesc, 11> a{
            knob("proco_rat_distortion", "DISTORTION", 20),
            knob("proco_rat_filter", "FILTER", 21),
            knob("proco_rat_volume", "VOLUME", 22),
            common::engage("proco_rat_bypass", 23),
            common::choice("proco_rat_variant", "VARIANT", variants, 3, 0, 24, PF_Config),
            common::choice("proco_rat_in_mode", "INPUT MODE", modes, 4, 3, -1, PF_Config),
            toggle("proco_rat_noise", "NOISE", false),
            common::continuous("proco_rat_at_depth", "PRESSURE DEPTH", "", 0, 1, 0, -1),
            common::choice("proco_rat_expr_target", "EXPRESSION TARGET", targets, 3, 0, -1, PF_Ext),
            common::continuous("proco_rat_expr_min", "EXPRESSION HEEL", "", 0, 1, 0, -1),
            common::continuous("proco_rat_expr_max", "EXPRESSION TOE", "", 0, 1, 1, -1)};
        for (int i : {7, 9, 10})
            a[i].flags = PF_Ext | PF_OwnSmoothing;
        return a;
    }();
    static const PedalDesc d = {"proco-rat",
                                "proco_rat_",
                                1,
                                "Rodent Distortion (RAT style)",
                                "Distortion",
                                "Pro Co RAT",
                                ChannelMode::Stereo,
                                BypassMode::True,
                                11,
                                p.data(),
                                140,
                                false,
                                true,
                                true,
                                1200};
    return d;
}
ProcoRat::ProcoRat() : CircuitPedal(descriptor()) {}
void ProcoRat::setParam(int i, float n) noexcept
{
    if (i >= 0 && i < 3 && std::isfinite(n) && n != getParam(i))
        expressionActive = false;
    PedalBase<11>::setParam(i, n);
}
void ProcoRat::prepare(double rate, int, int policy)
{
    prepareCircuit(rate, policy >= 2 ? 8 : 4);
    controlCoefficient = -std::expm1(-1 / (sr * .02));
    for (int c = 0; c < 2; ++c)
    {
        inputHp[c].setLowpass(os[c].rate(), 1 / (2 * common::pi * 1e6 * 22e-9));
        for (auto &v : voices[c])
        {
            v.amplifier.prepare(os[c].rate());
            v.coupling.setLowpass(os[c].rate(), 1 / (2 * common::pi * 1000 * 4.7e-6));
        }
    }
    diode[0].configure(2 * 1000 * 2.52e-9, 1.752 * .02585);
    diode[1].configure(2 * 1000 * 3e-18, 1.95 * .02585);
    reset();
}
void ProcoRat::reset() noexcept
{
    resetCircuit();
    drive = value(0);
    filter = value(1);
    volume = value(2);
    pressure = pressureTarget = 0;
    expressionActive = false;
    variant = (int)value(4);
    weights.fill(0);
    weights[variant] = 1;
    step.fill(0);
    remaining = 0;
    noise.reset(seed);
    for (int c = 0; c < 2; ++c)
    {
        inputHp[c].reset();
        for (auto &v : voices[c])
        {
            v.amplifier.reset();
            v.gbw.reset();
            v.coupling.reset();
            v.tone.reset();
            v.slew = 0;
        }
    }
    update();
}
void ProcoRat::onMidi(const MidiEvent &e) noexcept
{
    if ((e.status & 0xf0) == 0xd0)
        pressureTarget = std::min(127, (int)e.d1) / 127.;
    if ((e.status & 0xf0) == 0xa0)
        pressureTarget = std::min(127, (int)e.d2) / 127.;
}
void ProcoRat::setExpression(int jack, float v) noexcept
{
    if (jack < 0 || jack > 1 || !std::isfinite(v))
        return;
    expression = std::clamp((double)v, 0., 1.);
    expressionActive = true;
}
void ProcoRat::update() noexcept
{
    const double rf = std::max(.001, 1e5 * audioPot(drive)), hfGain = 1 + rf * (1. / 560 + 1. / 47);
    for (int c = 0; c < 2; ++c)
        for (int k = 0; k < 3; ++k)
        {
            auto &v = voices[c][k];
            v.amplifier.setFeedbackResistance(rf);
            v.gbw.setLowpass(os[c].rate(), (k == 1 ? .6e6 : 1e6) / hfGain);
            v.tone.setLowpass(os[c].rate(), toneCutoff(filter));
        }
}
void ProcoRat::process(double *l, double *r, int n) noexcept
{
    if (!l || !r)
        return;
    for (int i = 0; i < n; ++i, ++clock)
    {
        tick();
        pressure += controlCoefficient * (pressureTarget - pressure);
        double targets[] = {value(0), value(1), value(2)};
        if (expressionActive)
            targets[(int)value(8)] = p[9] + expression * (p[10] - p[9]);
        targets[0] = std::clamp(targets[0] + pressure * p[7], 0., 1.);
        drive += controlCoefficient * (targets[0] - drive);
        filter += controlCoefficient * (targets[1] - filter);
        volume += controlCoefficient * (targets[2] - volume);
        if ((clock & 31) == 0)
            update();
        const int target = (int)value(4);
        if (target != variant)
        {
            variant = target;
            remaining = std::max(1, (int)std::lround(sr * .02));
            for (int k = 0; k < 3; ++k)
                step[k] = ((k == variant ? 1. : 0.) - weights[k]) / remaining;
        }
        if (remaining > 0)
        {
            for (int k = 0; k < 3; ++k)
                weights[k] += step[k];
            if (--remaining == 0)
            {
                weights.fill(0);
                weights[variant] = 1;
            }
        }
        double inputs[] = {bounded(l[i]), bounded(r[i])};
        const int mode = (int)value(5);
        if (mode == 0)
            inputs[0] = inputs[1] = .5 * (inputs[0] + inputs[1]);
        else if (mode == 1)
            inputs[1] = inputs[0];
        else if (mode == 2)
            inputs[0] = inputs[1];
        double *channels[] = {l, r};
        const double gain = 4 * audioPot(volume) * .9 / voltsPerFs;
        for (int c = 0; c < 2; ++c)
        {
            double x = inputs[c] * voltsPerFs;
            if (p[6] >= .5)
                x += std::sqrt(3.) * 3.162277660e-6 * noise.next();
            const double y =
                os[c].process(x,
                              [&](double sample)
                              {
                                  const double in = inputHp[c].highpass(sample);
                                  double mixed = 0;
                                  for (int k = 0; k < 3; ++k)
                                  {
                                      auto &v = voices[c][k];
                                      double out = v.gbw.process(v.amplifier.process(in));
                                      v.slew +=
                                          std::clamp(out - v.slew, -.3e6 / os[c].rate(), .3e6 / os[c].rate());
                                      out = 3.5 * std::tanh(v.slew / 3.5);
                                      out = diode[k == 2 ? 1 : 0].process(v.coupling.highpass(out));
                                      mixed += weights[k] * v.tone.process(out);
                                  }
                                  return mixed;
                              });
            channels[c][i] = bounded(y * gain);
        }
    }
}
} // namespace luthier::reference::models
