// OWNER: WP8
#include "Overdrive.h"
#include "../Common/SoftClip.h"
namespace luthier::reference::generic
{
namespace
{
const char *const clips[] = {"Soft", "Asym", "Hard"};
const char *const basses[] = {"Off", "300 Hz", "700 Hz"};
const ParamDesc params[] = {
    detail::continuous("gen_overdrive_drive", "DRIVE", "dB", 0, 40, 16, 26),
    detail::continuous("gen_overdrive_tone", "TONE", "Hz", 500, 9000, 3000, 27, Taper::LogA),
    detail::continuous("gen_overdrive_level", "LEVEL", "dB", -60, 12, 0, 28, Taper::Skew, 2),
    detail::choice("gen_overdrive_clip", "CLIP", clips, 3, 0, 29),
    detail::choice("gen_overdrive_bass", "BASS CUT", basses, 3, 1, 30),
    detail::choice("gen_overdrive_quality", "QUALITY", detail::qualityNames, 3, 1, -1, PF_Config),
    detail::engage("gen_overdrive_on", 31)};
const PedalDesc description = {"generic.overdrive",
                               "gen_overdrive_",
                               1,
                               "Overdrive",
                               "Overdrive",
                               nullptr,
                               ChannelMode::Stereo,
                               BypassMode::True,
                               7,
                               params,
                               176,
                               false,
                               true,
                               false,
                               1000};
} // namespace
const PedalDesc &Overdrive::descriptor() noexcept { return description; }
Overdrive::Overdrive() : Processor(description) {}
void Overdrive::prepare(double rate, int, int quality)
{
    prepareCommon(rate, quality, true, 5);
    prepareCalibration();
    for (auto &f : dc)
        f.prepare(sr, 5);
    reset();
}
// E38 DESIGN calibration, not hardware modelling. Reference: -18dBFS,
// 1kHz sine, Soft clipping, Bass300Hz, Tone3kHz, Level0dB. Quadrature
// estimates wet RMS including the bass split, the original compensation and
// the tone fundamental attenuation. The scalar preserves harmonic ratios;
// validation checks actual rendered RMS at all supported rates and drives.
void Overdrive::prepareCalibration() noexcept
{
    constexpr double amplitude = .12589254117941673;
    const double bassMagnitude =
        std::sqrt((1000. * 1000. + .09 * 300. * 300.) / (1000. * 1000. + 300. * 300.));
    const double toneMagnitude = 1 / std::sqrt(1 + 1. / 9.);
    std::array<double, 256> sine{};
    for (size_t i = 0; i < sine.size(); ++i)
        sine[i] = std::sin(2 * common::pi * i / sine.size());
    for (size_t d = 0; d < calibration.size(); ++d)
    {
        double g = common::dbGain(.4 * d), energy = 0;
        for (double x : sine)
        {
            double y = std::tanh(amplitude * bassMagnitude * g * x);
            energy += y * y;
        }
        const double rms = std::sqrt(energy / sine.size()) * toneMagnitude / (1 + .1 * std::sqrt(g));
        calibration[d] = amplitude / std::sqrt(2.) / rms;
    }
}
double Overdrive::calibrationGain(double db) const noexcept
{
    const double position = std::clamp(db * 2.5, 0., 100.);
    const size_t i = (size_t)position;
    return i >= 100 ? calibration[100]
                    : calibration[i] + (position - i) * (calibration[i + 1] - calibration[i]);
}
void Overdrive::reset() noexcept
{
    resetCommon();
    identicalHistory = true;
    previousTone = previousBass = -1;
    for (int c = 0; c < 2; ++c)
    {
        bass[c].reset();
        tone[c].reset();
        dc[c].reset();
    }
}
void Overdrive::process(double *l, double *r, int n) noexcept
{
    if (!l || !r)
        return;
    for (int i = 0; i < n; ++i)
    {
        tick();
        if ((clock & 7) == 0)
        {
            if (p[4] != previousBass)
            {
                for (auto &f : bass)
                    f.setLowpass(sr, detail::modeBlend(p[4], 20, 300, 700));
                previousBass = p[4];
            }
            if (p[1] != previousTone)
            {
                for (auto &f : tone)
                    f.setLowpass(sr, p[1]);
                previousTone = p[1];
            }
        }
        const double g = driveGain.get(p[0]), level = p[2] <= -60 ? 0 : levelGain.get(p[2]),
                     comp = calibrationGain(p[0]) / (1 + .1 * std::sqrt(g));
        double *channels[] = {l, r};
        const double inputs[] = {input(l[i]), input(r[i])};
        // Equal inputs from reset have identical channel state. Materialise the
        // deferred right state once, at the first differing input sample.
        if (identicalHistory && inputs[0] != inputs[1])
        {
            os[1] = os[0];
            bass[1] = bass[0];
            tone[1] = tone[0];
            dc[1] = dc[0];
            identicalHistory = false;
        }

        for (int c = 0; c < (identicalHistory ? 1 : 2); ++c)
        {
            double x = inputs[c], v = (x - .7 * bass[c].process(x)) * g;
            double y = os[c].process(v,
                                     [&](double z)
                                     {
                                         return detail::selectMode(
                                             p[3], [&] { return std::tanh(z); },
                                             [&] { return common::asymmetricClip(z); },
                                             [&] { return common::diodeApprox(z); });
                                     });
            channels[c][i] = mix(c, x, dc[c].process(tone[c].process(y) * comp * level));
        }
        if (identicalHistory)
            r[i] = l[i];
        advance();
    }
}
} // namespace luthier::reference::generic
