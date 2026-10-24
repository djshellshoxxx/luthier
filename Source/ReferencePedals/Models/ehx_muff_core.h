// OWNER: WP13
#pragma once
#include "../Common/OnePole.h"
#include "../Common/PreparedDiodeRoot.h"
#include "../Common/Smoothers.h"
#include <memory>
namespace luthier::reference::models
{
// Component equations from the NYC specification, whose values are still
// partly estimated. Shared by NYC and Wicker only, not a generic fuzz alias.
struct MuffConsts
{
    static constexpr double rf = 470000, rbe = 25000, a = 150, is = 2.52e-9, vt = 1.752 * .02585;
};
class MuffTone
{
  public:
    void configure(double rate, double tone, double rl = 39000, double cl = 10e-9, double rh = 22000,
                   double ch = 4e-9, double rp = 100000) noexcept
    {
        const double k = 2 * common::sampleRate(rate), gl = 1 / rl, gh = 1 / rh, gp = 1 / rp,
                     t = std::clamp(common::safe(tone), 0., 1.);
        const double d2 = cl * ch, d1 = cl * (gh + gp) + ch * (gl + gp), d0 = gl * gh + gl * gp + gp * gh;
        const double n2 = t * cl * ch, n1 = ch * (gl + gp), n0 = (1 - t) * gl * (gh + gp) + t * gp * gl,
                     den = d2 * k * k + d1 * k + d0;
        b0 = (n2 * k * k + n1 * k + n0) / den;
        b1 = 2 * (n0 - n2 * k * k) / den;
        b2 = (n2 * k * k - n1 * k + n0) / den;
        a1 = 2 * (d0 - d2 * k * k) / den;
        a2 = (d2 * k * k - d1 * k + d0) / den;
    }
    double process(double x) noexcept
    {
        x = common::safe(x);
        const double y = common::safe(b0 * x + s1);
        s1 = common::safe(b1 * x - a1 * y + s2);
        s2 = common::safe(b2 * x - a2 * y);
        return y;
    }
    void reset() noexcept { s1 = s2 = 0; }

  private:
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, s1 = 0, s2 = 0;
};
class MuffClipStage
{
  public:
    // The caller counts this immutable allocation once across all sharing stages.
    uint64_t rootMemoryBytes() const noexcept { return root ? sizeof(*root) + root->ownedMemoryBytes() : 0; }
    using RootTable = std::shared_ptr<const common::PreparedDiodeRoot>;
    static RootTable makeRootTable(double rate)
    {
        const double sr = common::sampleRate(rate);
        // Prepared physical range covers NYC/Wicker's 10k..35k source and
        // 22pF..470pF feedback capacitor. Other values use the helper fallback.
        const double minimumG =
            (1. / 35000 + 1 / MuffConsts::rbe) / (1 + MuffConsts::a) + 1 / MuffConsts::rf + 2 * sr * 22e-12;
        const double maximumG =
            (1. / 10000 + 1 / MuffConsts::rbe) / (1 + MuffConsts::a) + 1 / MuffConsts::rf + 2 * sr * 470e-12;
        auto table = std::make_shared<common::PreparedDiodeRoot>();
        table->prepare(2 * MuffConsts::is, MuffConsts::vt, minimumG, maximumG, .01);
        return table;
    }
    void prepare(double rate, const RootTable &table = {})
    {
        sr = common::sampleRate(rate);
        // Ownership changes only during off-thread prepare/destruction. Audio
        // processing reads immutable coefficients without reference-count ops.
        root = table ? table : makeRootTable(sr);
        configure(10000, 470e-12);
        reset();
    }
    void configure(double rs, double cf) noexcept
    {
        inputG = 1 / std::clamp(common::safe(rs), 1., 1e9);
        capG = 2 * sr * std::clamp(common::safe(cf), 0., 1e-3);
        linearG = (inputG + 1 / MuffConsts::rbe) / (1 + MuffConsts::a) + 1 / MuffConsts::rf + capG;
    }
    void reset() noexcept { vd = capCurrent = residual = 0; }
    double process(double input) noexcept
    {
        if (!root)
            return 0;
        const double x = std::clamp(common::safe(input), -16., 16.), history = capG * vd + capCurrent,
                     rhs = x * inputG + history;
        const auto solution = root->solve(rhs, linearG);
        const double next = solution.voltage;
        residual = solution.residual;
        capCurrent = common::safe(capG * (next - vd) - capCurrent);
        vd = next;
        return common::safe(-MuffConsts::a * vd / (1 + MuffConsts::a));
    }
    double lastResidualAmps() const noexcept { return residual; }

  private:
    RootTable root;
    double sr = 48000, inputG = 1e-4, capG = 0, linearG = 0, vd = 0, capCurrent = 0, residual = 0;
};
class MuffCore
{
  public:
    static double audioPot(double x) noexcept
    {
        return std::expm1(4.394 * std::clamp(x, 0., 1.)) / std::expm1(4.394);
    }
    uint64_t rootMemoryBytes() const noexcept { return clip1.rootMemoryBytes(); }
    void prepare(double rate, const MuffClipStage::RootTable &sharedTable = {})
    {
        sr = common::sampleRate(rate);
        inputHp.setLowpass(sr, 23);
        outputHp.setLowpass(sr, 10);
        outputLp.setLowpass(sr, 20000);
        const auto table = sharedTable ? sharedTable : MuffClipStage::makeRootTable(sr);
        clip1.prepare(sr, table);
        clip2.prepare(sr, table);
        configure(.5, .5, .5, 9, 0, 1, 0, false);
        reset();
    }
    void configure(double sustain, double t, double volume, double supply, double wicker = 0,
                   double toneIn = 1, double trimDb = 0, bool withWicker = false) noexcept
    {
        const double g = std::max(.005, audioPot(sustain)), rth = 1e5 * g * (1 - g),
                     w = std::clamp(wicker, 0., 1.);
        // Explicit loaded-divider choice allowed in NYC §3.2.
        sustainGain = g * 25000 / (25000 + 100000 * (1 - g) + 10000);
        // NYC section3.4 explicitly calls for Gcal calibration. Fixed from the
        // 48k/4x, 1kHz100mVpeak, controls.5/supply9 loaded-divider reference.
        volumeGain = 0.88895192336804296 * audioPot(volume);
        rail = 3 * std::clamp(supply, 5., 9.6) / 9;
        coupling1.setLowpass(sr, 1 / (2 * common::pi * (10000 + rth + 25000) * 100e-9));
        coupling2.setLowpass(sr, 1 / (2 * common::pi * 35000 * 100e-9));
        const double cf = 22e-12 + (470e-12 - 22e-12) * (1 - w);
        clip1.configure(10000 + rth, cf);
        clip2.configure(10000, cf);
        tone.configure(sr, t);
        toneMix = std::clamp(toneIn, 0., 1.);
        toneOutGain = common::dbGain(trimDb);
        wickerPath = withWicker;
        wickerLp.setLowpass(sr, 6000 * std::pow(10., w));
    }
    double process(double x) noexcept
    {
        double y = rail * std::tanh(10 * inputHp.highpass(x) / rail);
        y = rail * std::tanh(clip1.process(coupling1.highpass(y * sustainGain)) / rail);
        y = rail * std::tanh(clip2.process(coupling2.highpass(y)) / rail);
        if (wickerPath)
            y = wickerLp.process(y);
        const double filtered = tone.process(y);
        y = (1 - toneMix) * y * toneOutGain + toneMix * filtered;
        y = rail * std::tanh(10 * y / rail);
        return outputLp.process(outputHp.highpass(y * volumeGain));
    }
    void reset() noexcept
    {
        inputHp.reset();
        coupling1.reset();
        coupling2.reset();
        outputHp.reset();
        outputLp.reset();
        wickerLp.reset();
        clip1.reset();
        clip2.reset();
        tone.reset();
    }

  private:
    double sr = 192000, sustainGain = 0, volumeGain = 0, rail = 3, toneMix = 1, toneOutGain = 1;
    bool wickerPath = false;
    common::OnePole inputHp, coupling1, coupling2, outputHp, outputLp, wickerLp;
    MuffClipStage clip1, clip2;
    MuffTone tone;
};
} // namespace luthier::reference::models
