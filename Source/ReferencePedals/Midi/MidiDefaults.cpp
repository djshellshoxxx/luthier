#include "MidiDefaults.h"
#include "../PedalRegistry.h"
namespace luthier::reference::midi
{
void addDefaultMaps (MapSnap& map, const std::vector<PedalInstance>& instances, const BoardSettings& settings)
{
    if (instances.empty()) return;
    const PedalInstance* selected = &instances.front();
    if (const auto* midi = settings.midi.getDynamicObject())
    {
        const auto focus = juce::Uuid (midi->getProperty ("selectedInstance").toString());
        for (const auto& instance : instances) if (instance.instanceId == focus) selected = &instance;
    }
    uint16_t id = 60000;
    auto bind = [&] (const PedalInstance& instance, const ParamDesc& param, int index, int cc, int chan, int layer)
    {
        if (cc < 0 || cc > 127 || reservedCc (cc) || instance.slot < 0 || instance.slot >= kMaxSlots) return;
        Binding b; b.id = id++; b.chan = (uint8_t) chan; b.srcNum = (uint8_t) cc;
        b.target = (uint16_t) (instance.slot * kMaxParams + index);
        b.smoothMs = param.has (PF_OwnSmoothing) ? 0 : (uint16_t) std::clamp (param.smoothMs, 0.f, 500.f);
        if (param.kind == ParamKind::Continuous || param.kind == ParamKind::Tempo) b.flags |= BF_SoftTakeover;
        const auto mode = param.kind == ParamKind::Trigger ? BindMode::Trigger : param.kind == ParamKind::Momentary ? BindMode::Momentary
            : param.kind == ParamKind::Toggle ? BindMode::Toggle : param.isDiscreteKind() ? BindMode::Step : BindMode::Absolute;
        b.setModeCurve (mode, CurveId::Linear);
        const int binding = map.numBindings;
        if (map.add (b, (uint8_t) layer)) { map.initialValues[(size_t) binding] = instance.params[(size_t) index]; map.steps[(size_t) binding] = (uint16_t) param.steps; }
    };
    auto foot = [&] (int slot, int cc, int channel, uint8_t layer)
    {
        if (slot < 0 || slot >= kMaxSlots || cc < 0 || cc > 127 || reservedCc (cc)) return;
        for (int ch = 1; ch <= 16; ++ch)
            if ((channel == 0 || channel == ch) && map.footLayer[(size_t) ch - 1][(size_t) cc] > layer)
            { map.footCc[(size_t) ch - 1][(size_t) cc] = (int8_t) slot; map.footLayer[(size_t) ch - 1][(size_t) cc] = layer; }
    };
    if (const auto* desc = PedalRegistry::find (selected->modelId.c_str()))
    {
        int knob = 0; bool expression = false;
        for (int p = 0; p < desc->numParams; ++p)
        {
            const auto& param = desc->params[p];
            if (param.has (PF_Hidden) || param.has (PF_Engage) || param.has (PF_Config) || param.has (PF_Ext)) continue;
            if (knob < 8 && param.kind != ParamKind::Trigger && param.kind != ParamKind::Momentary) bind (*selected, param, p, 70 + knob++, 0, 1);
            if (! expression && param.has (PF_ExprTarget)) { bind (*selected, param, p, 11, 0, 1); expression = true; }
        }
        foot (selected->slot, 64, 0, 1);
    }
    for (const auto& instance : instances)
    {
        const auto* desc = PedalRegistry::find (instance.modelId.c_str()); if (! desc) continue;
        const auto local = instance.extras["midi"];
        const int channel = std::clamp ((int) local["chan"], 0, 16);
        const bool profile = channel != 0 || (&instance == selected && (bool) local["useProfile"]);
        if (! profile) continue;
        bool virtualOff = instance.modelId == "boss-dd3t" || instance.modelId == "tc-ditto" || instance.modelId == "ehx-nano-360";
        if (virtualOff)
            for (int p = 0; p < desc->numParams; ++p)
                if (juce::String (desc->params[p].id).endsWith ("_midi_enable") && instance.params[(size_t) p] >= .5f) virtualOff = false;
        if (virtualOff) continue;
        bool hasProfile = false; int genericKnob = 0;
        for (int p = 0; p < desc->numParams; ++p) if (desc->params[p].midiCcDefault >= 0) hasProfile = true;
        for (int p = 0; p < desc->numParams; ++p)
        {
            const auto& param = desc->params[p];
            if (param.has (PF_Hidden) || param.has (PF_Config)) continue;
            if (param.has (PF_Engage)) { foot (instance.slot, param.midiCcDefault >= 0 ? param.midiCcDefault : 85, channel, 2); continue; }
            int cc = param.midiCcDefault;
            if (! hasProfile && ! param.has (PF_Ext) && param.kind != ParamKind::Trigger && param.kind != ParamKind::Momentary && genericKnob < 12) cc = 20 + genericKnob++;
            bind (instance, param, p, cc, channel, 2);
        }
        if (! hasProfile) foot (instance.slot, 85, channel, 2);
    }
}
}
