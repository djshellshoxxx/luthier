// OWNER: WP8
#include "Boost.h"
#include "../Common/SoftClip.h"
namespace luthier::reference::generic
{
namespace
{
const char *const modes[] = {"Clean", "Warm", "Fat"};
const ParamDesc params[] = {
    detail::continuous("gen_boost_gain", "GAIN", "dB", 0, 30, 12, 20),
    detail::continuous("gen_boost_tone", "TONE", "dB", -6, 6, 0, 21),
    detail::continuous("gen_boost_level", "LEVEL", "dB", -24, 6, -6, 22),
    detail::choice("gen_boost_mode", "MODE", modes, 3, 0, 23),
    detail::choice("gen_boost_quality", "QUALITY", detail::qualityNames, 3, 1, 24, PF_Config),
    detail::engage("gen_boost_on", 25)};
const PedalDesc description = {"generic.boost",
                               "gen_boost_",
                               1,
                               "Clean Boost",
                               "Boost",
                               nullptr,
                               ChannelMode::Stereo,
                               BypassMode::True,
                               6,
                               params,
                               176,
                               false,
                               true,
                               false,
                               300};
} // namespace
const PedalDesc &Boost::descriptor() noexcept { return description; }
Boost::Boost() : Processor(description) {}
void Boost::prepare(double rate, int, int quality)
{
    prepareCommon(rate, quality, true, 4);
    policy = std::min(quality, (int)value(4));
    for (int c = 0; c < 2; ++c)
    {
        hp[c].setLowpass(sr, 2);
        eco[c].setLowpass(os[c].rate(), 18000);
        warm[c].setShelf(os[c].rate(), 100, 3, false);
        fat1[c].setShelf(os[c].rate(), 120, 4, false);
        fat2[c].set(os[c].rate(), 120, 2, false);
    }
    reset();
}
void Boost::reset() noexcept
{
    resetCommon();
    identicalHistory = true;
    previousTone = 100;
    for (int c = 0; c < 2; ++c)
    {
        hp[c].reset();
        eco[c].reset();
        tone[c].reset();
        warm[c].reset();
        fat1[c].reset();
        fat2[c].reset();
    }
}
void Boost::process(double *l, double *r, int n) noexcept
{
    if (!l || !r)
        return;
    for (int i = 0; i < n; ++i)
    {
        tick();
        if ((clock & 7) == 0 && p[1] != previousTone)
        {
            for (int c = 0; c < 2; ++c)
                tone[c].setShelf(os[c].rate(), 3000, p[1], true);
            previousTone = p[1];
        }
        const double gain = driveGain.get(p[0]), level = levelGain.get(p[2]);
        double *channels[] = {l, r};
        const double inputs[] = {input(l[i]), input(r[i])};
        // Equal inputs from reset have identical channel state. Materialise the
        // deferred right state once, at the first differing input sample.
        if (identicalHistory && inputs[0] != inputs[1])
        {
            os[1] = os[0];
            hp[1] = hp[0];
            eco[1] = eco[0];
            tone[1] = tone[0];
            warm[1] = warm[0];
            fat1[1] = fat1[0];
            fat2[1] = fat2[0];
            identicalHistory = false;
        }

        for (int c = 0; c < (identicalHistory ? 1 : 2); ++c)
        {
            double x = inputs[c];
            double y = os[c].process(
                hp[c].highpass(x) * gain,
                [&](double v)
                {
                    v = tone[c].process(v);
                    double b = warm[c].process(v), f = fat2[c].process(fat1[c].process(v));
                    double z = detail::selectMode(
                        p[3], [&] { return common::cleanClip(v); },
                        [&] { return .5 * b + .5 * std::tanh(b); }, [&] { return std::tanh(1.4 * f); });
                    return policy == 0 ? eco[c].process(z) : z;
                });
            channels[c][i] = mix(c, x, y * level);
        }
        if (identicalHistory)
            r[i] = l[i];
        advance();
    }
}
} // namespace luthier::reference::generic
