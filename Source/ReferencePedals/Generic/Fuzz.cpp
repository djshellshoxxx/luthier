// OWNER: WP8
#include "Fuzz.h"
namespace luthier::reference::generic
{
namespace
{
const char *const types[] = {"Silicon", "Germanium", "Octave-ish"};
// CC32 is reserved bank-select: omit the published conflicting local assignment.
const ParamDesc params[] = {
    detail::continuous("gen_fuzz_fuzz", "FUZZ", "dB", 6, 60, 44, -1),
    detail::continuous("gen_fuzz_tone", "TONE", "dB", -12, 12, 0, 33),
    detail::continuous("gen_fuzz_volume", "VOLUME", "dB", -60, 12, -6, 34, Taper::Skew, 2),
    detail::continuous("gen_fuzz_bias", "BIAS", "", 0, 1, .5, 35),
    detail::choice("gen_fuzz_type", "TYPE", types, 3, 0, 36),
    detail::continuous("gen_fuzz_gate", "GATE", "", 0, 1, 0, 37),
    detail::choice("gen_fuzz_quality", "QUALITY", detail::qualityNames, 3, 1, -1, PF_Config),
    detail::engage("gen_fuzz_on", 38)};
const PedalDesc description = {"generic.fuzz",
                               "gen_fuzz_",
                               1,
                               "Fuzz",
                               "Fuzz",
                               nullptr,
                               ChannelMode::Stereo,
                               BypassMode::True,
                               8,
                               params,
                               176,
                               false,
                               true,
                               false,
                               1200,
                               1000};
double silicon(double x) noexcept { return x >= 0 ? std::tanh(1.3 * x) : 1.1 * std::tanh(.9 * x); }
double shape(double x, double type, double bias) noexcept
{
    if (type <= 0 || type >= 2)
        return silicon(x);
    double b = .2 * (1 - bias), ge = std::tanh(1.8 * (x + b)) - std::tanh(1.8 * b);
    if (type == 1)
        return ge;
    double si = silicon(x);
    return detail::modeBlend(type, si, ge, si);
}
} // namespace
const PedalDesc &Fuzz::descriptor() noexcept { return description; }
Fuzz::Fuzz() : Processor(description) {}
void Fuzz::prepare(double rate, int, int quality)
{
    prepareCommon(rate, quality, true, 6);
    policy = std::min(quality, (int)value(6));
    for (int c = 0; c < 2; ++c)
    {
        hp[c].setLowpass(sr, 60);
        germanium[c].setLowpass(sr, 9000);
        tilt[c].setLowpass(sr, 1000);
        eco[c].setLowpass(sr, 14000);
        dc[c].prepare(sr, 10);
        octave[c].prepare(os[c].rate(), 80);
        sag[c].prepare(sr, 5, 120);
        gate[c].prepare(sr, 1, 80);
        gateGain[c].prepare(sr, 1);
    }
    reset();
}
void Fuzz::reset() noexcept
{
    resetCommon();
    identicalHistory = true;
    previousType = -1;
    for (int c = 0; c < 2; ++c)
    {
        hp[c].reset();
        germanium[c].reset();
        tilt[c].reset();
        eco[c].reset();
        dc[c].reset();
        octave[c].reset();
        sag[c].reset();
        gate[c].reset();
        gateGain[c].reset(1);
        hold[c] = 0;
    }
}
void Fuzz::process(double *l, double *r, int n) noexcept
{
    if (!l || !r)
        return;
    for (int i = 0; i < n; ++i)
    {
        tick();
        const double g = std::sqrt(driveGain.get(p[0])), t = toneGain.get(p[1]),
                     volume = p[2] <= -60 ? 0 : levelGain.get(p[2]);
        double *channels[] = {l, r};
        const double inputs[] = {input(l[i]), input(r[i])};
        // Equal inputs from reset have identical channel state. Materialise the
        // deferred right state once, at the first differing input sample.
        if (identicalHistory && inputs[0] != inputs[1])
        {
            os[1] = os[0];
            hp[1] = hp[0];
            germanium[1] = germanium[0];
            tilt[1] = tilt[0];
            eco[1] = eco[0];
            dc[1] = dc[0];
            octave[1] = octave[0];
            sag[1] = sag[0];
            gate[1] = gate[0];
            gateGain[1] = gateGain[0];
            hold[1] = hold[0];
            identicalHistory = false;
        }

        if ((clock & 7) == 0 && p[4] != previousType)
        {
            for (auto &f : hp)
                f.setLowpass(sr, detail::modeBlend(p[4], 60, 90, 60));
            previousType = p[4];
        }
        for (int c = 0; c < (identicalHistory ? 1 : 2); ++c)
        {
            double x = inputs[c], v = hp[c].highpass(x), ge = germanium[c].process(v);
            double geWeight = std::max(0.0, 1 - std::abs(p[4] - 1));
            v += (ge - v) * geWeight;
            const double env = std::min(1.0, sag[c].process(x)),
                         gain = g * std::sqrt(1 - .2 * env * geWeight);
            double y = os[c].process(v,
                                     [&](double z)
                                     {
                                         double y1 = shape(gain * z, p[4], p[3]),
                                                y2 = shape(gain * y1 + (.5 - p[3]) * .6, p[4], p[3]);
                                         double starve =
                                             std::clamp((std::abs(y1) + .1 * p[3]) / .1, 0.0, 1.0);
                                         y2 *= starve * starve * (3 - 2 * starve);
                                         double oct = octave[c].process(.6 * y2 + .4 * std::abs(y2));
                                         return y2 + (oct - y2) * std::max(0.0, p[4] - 1);
                                     });
            y = dc[c].process(y);
            double low = tilt[c].process(y);
            y = low / t + (y - low) * t;
            double detected = gate[c].process(x), threshold = thresholdGain.get(-90 + 50 * p[5]), target = 1;
            if (p[5] > 0)
            {
                if (detected >= threshold)
                    hold[c] = (int)(sr * .03);
                else if (hold[c] > 0)
                    --hold[c];
                else
                    target = std::max(.001, std::pow(detected / threshold, 3));
            }
            gateGain[c].setTarget(target);
            y *= gateGain[c].next() * volume;
            if (policy == 0)
                y = eco[c].process(y);
            channels[c][i] = mix(c, x, std::clamp(y, -common::dbGain(12), common::dbGain(12)));
        }
        if (identicalHistory)
            r[i] = l[i];
        advance();
    }
}
} // namespace luthier::reference::generic
