// OWNER: WP13
#include "ehx_muff_wicker.h"
namespace luthier::reference::models
{
PreparedMemoryRequirement EhxMuffWicker::prepareMemory(double sr, int block, int, const float *) noexcept
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000 || block <= 0)
        return {};
    const auto delaySamples = static_cast<uint64_t>(std::ceil(sr * .005)) + 18;
    return {sizeof(EhxMuffWicker) + sizeof(common::PreparedDiodeRoot) +
                common::PreparedDiodeRoot::preparedAllocationBytes() + 2 * delaySamples * sizeof(double),
            0};
}
uint64_t EhxMuffWicker::getPreparedMemoryBytes() const noexcept
{
    return sizeof(*this) + core[0].rootMemoryBytes() + howlDelay[0].ownedMemoryBytes() +
           howlDelay[1].ownedMemoryBytes();
}

const PedalDesc &EhxMuffWicker::descriptor() noexcept
{
    static const char *const stereo[] = {"Dual mono", "Sum to mono"};
    static const char *const revision[] = {"NYC Wicker estimate", "Op-amp (unavailable)"};
    static const std::array<ParamDesc, 15> p = []
    {
        std::array<ParamDesc, 15> a{
            common::engage("ehx_muff_wicker_enabled", 80),
            knob("ehx_muff_wicker_sustain", "SUSTAIN", 70),
            knob("ehx_muff_wicker_tone", "TONE", 71),
            knob("ehx_muff_wicker_volume", "VOLUME", 72),
            toggle("ehx_muff_wicker_tone_in", "TONE IN", true, PF_Automatable),
            toggle("ehx_muff_wicker_wicker", "WICKER", false, PF_Automatable),
            common::continuous("ehx_muff_wicker_wicker_morph", "WICKER MORPH", "", 0, 1, 0, -1),
            common::continuous("ehx_muff_wicker_tone_out_trim_db", "TONE OUT TRIM", "dB", -12, 6, 0, -1),
            common::continuous("ehx_muff_wicker_supply_v", "SUPPLY", "V", 5, 9.6f, 9, -1),
            common::choice("ehx_muff_wicker_quality", "QUALITY", common::qualityNames, 3, 1, -1, PF_Config),
            toggle("ehx_muff_wicker_auto_os", "AUTO OS (unavailable)", true),
            common::choice("ehx_muff_wicker_stereo_mode", "STEREO", stereo, 2, 0, -1, PF_Config),
            toggle("ehx_muff_wicker_noise_enable", "NOISE", false),
            common::continuous("ehx_muff_wicker_howl", "HOWL", "", 0, 1, 0, -1),
            common::choice("ehx_muff_wicker_revision", "REVISION (unavailable)", revision, 2, 0, -1,
                           PF_Config | PF_Hidden)};
        a[4].midiCcDefault = 81;
        a[5].midiCcDefault = 82;
        a[8].smoothMs = 50;
        a[13].smoothMs = 50;
        a[13].flags = PF_Ext | PF_OwnSmoothing;
        return a;
    }();
    static const PedalDesc d = {"ehx-muff-wicker",
                                "ehx_muff_wicker_",
                                1,
                                "Sustainer (Muff-style, Wicker)",
                                "Fuzz",
                                "EHX Big Muff Pi Tone Wicker",
                                ChannelMode::Stereo,
                                BypassMode::True,
                                15,
                                p.data(),
                                140,
                                false,
                                true,
                                false,
                                2500};
    return d;
}
EhxMuffWicker::EhxMuffWicker() : CircuitPedal(descriptor()) {}
void EhxMuffWicker::prepare(double rate, int, int policy)
{
    prepareCircuit(rate, 1 << (1 + std::clamp(std::min(policy, (int)value(9)), 0, 2)));
    toneSwitch.prepare(sr, 10);
    wickerSwitch.prepare(sr, 10);
    const auto table = MuffClipStage::makeRootTable(os[0].rate());
    for (int c = 0; c < 2; ++c)
    {
        core[c].prepare(os[c].rate(), table);
        howlDelay[c].prepare((int)std::ceil(sr * .005) + 2);
        howlHp[c].setLowpass(sr, 80);
        howlLp[c].setLowpass(sr, 800);
    }
    reset();
}
void EhxMuffWicker::update() noexcept
{
    for (auto &c : core)
        c.configure(p[1], p[2], p[3], p[8], wicker, toneMix, p[7], true);
}
void EhxMuffWicker::reset() noexcept
{
    resetCircuit();
    toneSwitch.reset(value(4));
    wickerSwitch.reset(value(5));
    toneMix = value(4);
    wicker = std::max((double)value(5), (double)value(6));
    noise.reset(seed);
    for (int c = 0; c < 2; ++c)
    {
        core[c].reset();
        howlDelay[c].reset();
        howlHp[c].reset();
        howlLp[c].reset();
    }
    update();
}
void EhxMuffWicker::process(double *l, double *r, int n) noexcept
{
    if (!l || !r)
        return;
    for (int i = 0; i < n; ++i, ++clock)
    {
        tick();
        toneSwitch.setTarget(value(4));
        wickerSwitch.setTarget(value(5));
        toneMix = toneSwitch.next();
        wicker = std::max(wickerSwitch.next(), p[6]);
        if ((clock & 15) == 0)
            update();
        double in[] = {bounded(l[i]), bounded(r[i])};
        if (p[11] >= .5)
            in[0] = in[1] = .5 * (in[0] + in[1]);
        double *channels[] = {l, r};
        for (int c = 0; c < 2; ++c)
        {
            double x = in[c];
            if (p[12] >= .5)
                x += 1e-7 * noise.next();
            if (p[13] > 0 && p[1] > .8)
                // The newest stored output is from the previous frame: read()
                // uses age zero for that value, so remove its one-frame age.
                x += (1 - toneMix) * common::dbGain(-50 + 20 * p[13]) * p[13] *
                     howlDelay[c].read(sr * .005 - 1);
            const double y = bounded(os[c].process(x, [&](double v) { return core[c].process(v); }));
            howlDelay[c].push(std::tanh(howlLp[c].process(howlHp[c].highpass(y))));
            channels[c][i] = y;
        }
    }
}
} // namespace luthier::reference::models
