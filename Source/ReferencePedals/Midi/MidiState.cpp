#include "MidiState.h"
#include "MidiDefaults.h"
#include "../PedalRegistry.h"
namespace luthier::reference::midi
{
namespace
{
float number (const juce::DynamicObject& o, const char* key, float def)
{ return o.hasProperty (key) ? (float) o.getProperty (key) : def; }
MidiSrc sourceType (const juce::var& v)
{
    if (v.isInt()) return (MidiSrc) (int) v;
    const auto text = v.toString();
    const char* names[] { "cc", "cc14", "nrpn", "note", "noteVel", "polyAT", "chanAT", "pitchBend", "pc", "start", "stop", "continue", "uiSlider", "keyboardVelocity" };
    for (int i = 0; i < kSourceTypes; ++i) if (text == names[i]) return (MidiSrc) i;
    return (MidiSrc) 255;
}
}
bool MidiState::validate (const juce::var& state, juce::String* error)
{
    if (state.isVoid()) return true;
    const auto* object = state.getDynamicObject();
    auto fail = [&] (const char* message) { if (error) *error = message; return false; };
    if (! object) return fail ("MIDI state must be an object");
    auto scalar = [&] (const juce::DynamicObject& o, const char* key, double lo, double hi)
    {
        if (! o.hasProperty (key)) return true;
        const auto v = o.getProperty (key);
        return (v.isInt() || v.isInt64() || v.isDouble()) && std::isfinite ((double) v) && (double) v >= lo && (double) v <= hi;
    };
    if (! scalar (*object, "channel", 0, 16) || ! scalar (*object, "bpm", 20, 300) || ! scalar (*object, "tempoSource", 0, 3))
        return fail ("MIDI channel, tempo or source is out of range");
    if (object->hasProperty ("bindings") && ! object->getProperty ("bindings").isArray()) return fail ("MIDI bindings must be an array");
    if (const auto* bindings = object->getProperty ("bindings").getArray())
    {
        if (bindings->size() > kMaxBindings) return fail ("MIDI bindings exceed 1024");
        for (const auto& b : *bindings)
        {
            const auto* o = b.getDynamicObject(); if (! o) return fail ("MIDI binding must be an object");
            if (! scalar (*o, "id", 0, 65535) || ! scalar (*o, "chan", 0, 16) || ! scalar (*o, "smoothMs", 0, 500)
                || ! scalar (*o, "priority", 0, 255) || ! scalar (*o, "flags", 0, 63)) return fail ("MIDI binding scalar is out of range");
            for (const auto* range : { "in", "out" })
                if (o->hasProperty (range))
                {
                    const auto* values = o->getProperty (range).getArray();
                    if (! values || values->size() != 2) return fail ("MIDI range must contain two values");
                    for (const auto& v : *values)
                        if (! (v.isInt() || v.isInt64() || v.isDouble()) || ! std::isfinite ((double) v) || (double) v < 0 || (double) v > 1)
                            return fail ("MIDI range value is outside 0..1");
                    if (juce::String (range) == "in" && (double) (*values)[0] >= (double) (*values)[1]) return fail ("MIDI input range must increase");
                }
            if (const auto* src = o->getProperty ("src").getDynamicObject())
                if (! scalar (*src, "num", 0, 127)) return fail ("MIDI source number is out of range");
        }
    }
    if (object->hasProperty ("scenes") && ! object->getProperty ("scenes").isArray()) return fail ("MIDI scenes must be an array");
    if (const auto* scenes = object->getProperty ("scenes").getArray())
        if (scenes->size() > 128) return fail ("MIDI scenes exceed 128");
    return true;
}
std::unique_ptr<MapSnap> MidiState::buildMap (const std::vector<PedalInstance>& instances, const BoardSettings& settings)
{
    auto map = std::make_unique<MapSnap>();
    map->boardEnabled = settings.enabled;
    map->midiChan = std::clamp (settings.midiChan, 0, 16); map->tempoSource = settings.tempoSource;
    map->internalBpm = settings.tempoBpm;
    // Note defaults follow chain order; destinations remain physical storage slots.
    const auto* midi = settings.midi.getDynamicObject();
    if (! validate (settings.midi)) { map->compile(); return map; }
    for (size_t i = 0; i < instances.size() && i < kMaxSlots; ++i)
        if (map->keyboardBaseNote + (int) i < 128 && instances[i].slot >= 0 && instances[i].slot < kMaxSlots)
            map->noteToSlot[(size_t) (map->keyboardBaseNote + (int) i)] = (int8_t) instances[i].slot;
    if (! midi) { addDefaultMaps (*map, instances, settings); map->compile(); return map; }
    if (auto* kbd = midi->getProperty ("kbd").getDynamicObject())
        map->keyboardBaseNote = std::clamp ((int) kbd->getProperty ("baseNote"), 0, 127);
    map->noteToSlot.fill (-1);
    for (size_t i = 0; i < instances.size() && i < kMaxSlots; ++i)
        if (map->keyboardBaseNote + (int) i < 128 && instances[i].slot >= 0 && instances[i].slot < kMaxSlots)
            map->noteToSlot[(size_t) (map->keyboardBaseNote + (int) i)] = (int8_t) instances[i].slot;
    const auto* bindings = midi->getProperty ("bindings").getArray();
    if (! bindings) { addDefaultMaps (*map, instances, settings); map->compile(); return map; }
    for (const auto& value : *bindings)
    {
        const auto* o = value.getDynamicObject(); if (! o) continue;
        Binding b; b.id = (uint16_t) (int) o->getProperty ("id"); b.chan = (uint8_t) (int) o->getProperty ("chan");
        const auto* src = o->getProperty ("src").getDynamicObject(); if (! src) continue;
        b.srcType = (uint8_t) sourceType (src->getProperty ("type")); b.srcNum = (uint8_t) (int) src->getProperty ("num");
        b.flags = o->hasProperty ("flags") ? (uint8_t) (int) o->getProperty ("flags") : BF_Enabled;
        b.priority = (uint8_t) (int) o->getProperty ("priority");
        b.smoothMs = (uint16_t) std::clamp ((int) number (*o, "smoothMs", 10), 0, 500);
        const auto paramId = o->getProperty ("paramId").toString();
        const auto id = o->getProperty ("instanceId").toString();
        const auto uuid = juce::Uuid (id);
        bool resolved = false;
        if (id == "board")
        {
            if (paramId == "ref_board_mix") { b.target = kTargetBoardBase + (uint16_t) BoardTarget::Mix; resolved = true; }
            else if (paramId == "ref_board_tempo_bpm") { b.target = kTargetBoardBase + (uint16_t) BoardTarget::Tempo; resolved = true; }
            else if (paramId == "ref_board_enabled") { b.target = kTargetBoardBase + (uint16_t) BoardTarget::Enabled; resolved = true; }
        }
        for (const auto& instance : instances)
            if (instance.instanceId == uuid)
                if (auto* desc = PedalRegistry::find (instance.modelId.c_str()))
                {
                    const int index = PedalRegistry::findParam (*desc, paramId.toRawUTF8());
                    if (index >= 0 && instance.slot >= 0 && instance.slot < kMaxSlots)
                    { b.target = (uint16_t) (instance.slot * kMaxParams + index); resolved = true; }
                    break;
                }
        if (! resolved) continue;
        if (auto* in = o->getProperty ("in").getArray(); in && in->size() == 2) { b.inMin = (float) (*in)[0]; b.inMax = (float) (*in)[1]; }
        if (auto* out = o->getProperty ("out").getArray(); out && out->size() == 2) { b.outMin = (float) (*out)[0]; b.outMax = (float) (*out)[1]; }
        CurveId curve = CurveId::Linear;
        if (auto* c = o->getProperty ("curve").getDynamicObject()) { curve = (CurveId) (int) c->getProperty ("id"); b.curveParam = number (*c, "p", 2); }
        BindMode mode = BindMode::Absolute;
        const auto m = o->getProperty ("mode");
        if (m.isInt()) mode = (BindMode) (int) m;
        else
        {
            const char* modes[] { "absolute", "relative", "toggle", "momentary", "trigger", "step" };
            for (int i = 0; i < 6; ++i) if (m.toString() == modes[i]) mode = (BindMode) i;
        }
        b.setModeCurve (mode, curve);
        const int index = map->numBindings;
        if (map->add (b))
        {
            for (const auto& instance : instances)
                if (instance.instanceId == uuid)
                    if (const auto* desc = PedalRegistry::find (instance.modelId.c_str()))
                    {
                        const int p = PedalRegistry::findParam (*desc, paramId.toRawUTF8());
                        if (p >= 0) { map->initialValues[(size_t) index] = instance.params[(size_t) p]; map->steps[(size_t) index] = (uint16_t) desc->params[p].steps; }
                    }
            if (id == "board")
                map->initialValues[(size_t) index] = paramId == "ref_board_mix" ? settings.mix : paramId == "ref_board_enabled" ? (settings.enabled ? 1.f : 0.f) : (float) ((settings.tempoBpm - 20.) / 280.);
        }
    }
    addDefaultMaps (*map, instances, settings); map->compile(); return map;
}
}
