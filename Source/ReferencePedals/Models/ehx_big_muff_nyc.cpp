// OWNER: WP13
#include "ehx_big_muff_nyc.h"
namespace luthier::reference::models
{
PreparedMemoryRequirement EhxBigMuffNyc::prepareMemory(double sr, int block, int, const float *) noexcept
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000 || block <= 0)
        return {};
    return {sizeof(EhxBigMuffNyc) + sizeof(common::PreparedDiodeRoot) +
                common::PreparedDiodeRoot::preparedAllocationBytes(),
            0};
}
uint64_t EhxBigMuffNyc::getPreparedMemoryBytes() const noexcept
{
    return sizeof(*this) + core[0].rootMemoryBytes();
}

const PedalDesc &EhxBigMuffNyc::descriptor() noexcept
{
    static const char *const stereo[] = {"Dual mono", "Sum to mono"};
    static const char *const revision[] = {"NYC estimate"};
    static const std::array<ParamDesc, 8> p = []
    {
        std::array<ParamDesc, 8> a{
            common::engage("ehx_big_muff_nyc_enabled", 80),
            knob("ehx_big_muff_nyc_sustain", "SUSTAIN", 70),
            knob("ehx_big_muff_nyc_tone", "TONE", 71),
            knob("ehx_big_muff_nyc_volume", "VOLUME", 72),
            common::continuous("ehx_big_muff_nyc_supply_v", "SUPPLY", "V", 5, 9.6f, 9, -1),
            common::choice("ehx_big_muff_nyc_quality", "QUALITY", common::qualityNames, 3, 1, -1, PF_Config),
            common::choice("ehx_big_muff_nyc_stereo_mode", "STEREO", stereo, 2, 0, -1, PF_Config),
            common::choice("ehx_big_muff_nyc_revision", "REVISION", revision, 1, 0, -1,
                           PF_Config | PF_Hidden)};
        a[4].smoothMs = 50;
        return a;
    }();
    static const PedalDesc d = {"ehx-big-muff-nyc",
                                "ehx_big_muff_nyc_",
                                1,
                                "Sustainer (Muff-style, NYC)",
                                "Fuzz",
                                "EHX Big Muff Pi NYC",
                                ChannelMode::Stereo,
                                BypassMode::True,
                                8,
                                p.data(),
                                140,
                                false,
                                true,
                                false,
                                2000,
                                1000};
    return d;
}
EhxBigMuffNyc::EhxBigMuffNyc() : CircuitPedal(descriptor()) {}
void EhxBigMuffNyc::prepare(double rate, int, int policy)
{
    prepareCircuit(rate, 1 << (1 + std::clamp(std::min(policy, (int)value(5)), 0, 2)));
    const auto table = MuffClipStage::makeRootTable(os[0].rate());
    for (int c = 0; c < 2; ++c)
        core[c].prepare(os[c].rate(), table);
    reset();
}
void EhxBigMuffNyc::update() noexcept
{
    for (auto &c : core)
        c.configure(p[1], p[2], p[3], p[4]);
}
void EhxBigMuffNyc::reset() noexcept
{
    resetCircuit();
    for (auto &c : core)
        c.reset();
    update();
}
void EhxBigMuffNyc::process(double *l, double *r, int n) noexcept
{
    if (!l || !r)
        return;
    for (int i = 0; i < n; ++i, ++clock)
    {
        tick();
        if ((clock & 15) == 0)
            update();
        double in[] = {bounded(l[i]), bounded(r[i])};
        if (p[6] >= .5)
            in[0] = in[1] = .5 * (in[0] + in[1]);
        double *channels[] = {l, r};
        for (int c = 0; c < 2; ++c)
            channels[c][i] = bounded(os[c].process(in[c], [&](double x) { return core[c].process(x); }));
    }
}
} // namespace luthier::reference::models
