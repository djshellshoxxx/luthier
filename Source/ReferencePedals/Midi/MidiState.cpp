#include "MidiState.h"
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
    if (const auto* bindings = object->getProperty ("bindings").getArray())
        if (bindings->size() > kMaxBindings) return fail ("MIDI bindings exceed 1024");
    if (const auto* scenes = object->getProperty ("scenes").getArray())
        if (scenes->size() > 128) return fail ("MIDI scenes exceed 128");
    return true;
}
std::unique_ptr<MapSnap> MidiState::buildMap (const std::vector<PedalInstance>& instances, const BoardSettings& settings)
{
    auto map = std::make_unique<MapSnap>();
    map->midiChan = std::clamp (settings.midiChan, 0, 16); map->tempoSource = settings.tempoSource;
    map->internalBpm = settings.tempoBpm;
    // Default notes address storage slots using a compiled per-note mapping below.
    const auto* midi = settings.midi.getDynamicObject();
    if (! midi || ! validate (settings.midi)) { map->compile(); return map; }
    if (auto* kbd = midi->getProperty ("kbd").getDynamicObject())
        map->keyboardBaseNote = std::clamp ((int) kbd->getProperty ("baseNote"), 0, 127);
    const auto* bindings = midi->getProperty ("bindings").getArray();
    if (! bindings) { map->compile(); return map; }
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
        bool resolved = false;
        for (const auto& instance : instances)
            if (instance.instanceId.toString() == id)
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
        b.setModeCurve (mode, curve); map->add (b);
    }
    map->compile(); return map;
}
}
