// OWNER: WP17 / global-search integration
#include "ReferenceSearch.h"
#include "../../ReferencePedals/PedalRegistry.h"
#include "../../ReferencePedals/Midi/ExtendedHostAutomation.h"

namespace luthier::reference
{
ReferenceSearchTarget resolveSearchTarget(const BoardModel& model, const juce::String& id)
{
    ReferenceSearchTarget target;
    if (id.startsWith("ref_board_"))
    {
        const bool toolbar = id == "ref_board_enabled" || id == "ref_board_mix"
            || id == "ref_board_tempo_bpm" || id == "ref_board_mode" || id == "ref_board_mix_law";
        const char* label = nullptr;
        for (const auto& parameter : extendedHostParameters)
            if (id == parameter.id) { label = parameter.name; break; }
        if (!toolbar && !label) return target;
        target.recognized = target.available = true;
        target.surface = toolbar ? ReferenceSearchTarget::Surface::toolbar : ReferenceSearchTarget::Surface::settings;
        target.breadcrumb = toolbar ? "Pedalboard" : "Pedalboard / Settings";
        target.title = label ? juce::String(label) : id == "ref_board_enabled" ? "Board enabled"
            : id == "ref_board_mix" ? "Board wet/dry mix" : "Board tempo BPM";
        return target;
    }

    if (!id.startsWith("ref_s") || id.length() < 9 || id[7] != '_'
        || !id.substring(5, 7).containsOnly("0123456789")) return target;
    const int slot = id.substring(5, 7).getIntValue() - 1;
    if (slot < 0 || slot >= kMaxSlots) return target;
    const auto field = id.substring(8);
    const bool lane = field.length() == 3 && field[0] == 'p' && field.substring(1).containsOnly("0123456789");
    const int number = lane ? field.substring(1).getIntValue() : 0;
    if (lane ? number < 1 || number > kMaxLaneParams
             : !(field == "enabled" || field == "in_db" || field == "out_db" || field == "pos")) return target;

    target.recognized = true;
    target.slot = slot;
    target.surface = ReferenceSearchTarget::Surface::pedal;
    const auto location = "Slot " + juce::String(slot + 1);
    target.breadcrumb = "Pedalboard / " + location;
    target.title = location + ": " + (lane ? "host lane " + juce::String(number) : field == "enabled" ? "enabled"
        : field == "in_db" ? "input trim" : field == "out_db" ? "output trim" : "signal position");

    std::optional<PedalInstance> instance;
    for (const auto& uuid : model.getOrder())
        if (auto value = model.getInstance(uuid); value && value->slot == slot)
        { instance = std::move(value); break; }
    if (!instance)
    { target.reason = location + " is empty. Add a pedal to use this control."; return target; }

    target.instance = instance->instanceId;
    const auto* descriptor = PedalRegistry::find(instance->modelId.c_str());
    if (lane)
    {
        if (instance->missing || !descriptor)
        { target.reason = location + " pedal model is unavailable."; return target; }
        const auto& map = PedalRegistry::laneMap(instance->modelId.c_str());
        const int parameter = number <= map.numLanes ? map.laneToParam[(size_t)(number - 1)] : -1;
        if (parameter < 0 || parameter >= descriptor->numParams)
        { target.reason = location + " has no control assigned to host lane " + juce::String(number) + "."; return target; }
        target.param = parameter;
        target.title = juce::String(descriptor->displayName) + " " + descriptor->params[parameter].label + " (" + location + ")";
    }
    else if (field == "enabled" && (instance->missing || !descriptor))
    { target.reason = location + " pedal model is unavailable."; return target; }
    target.available = true;
    return target;
}

juce::uint32 searchModelGeneration(const BoardModel& model)
{
    // Storage identity/model changes invalidate titles. Parameter, focus, view and
    // order-only changes do not churn the index or invalidate an active result.
    std::array<juce::String, kMaxSlots> identities;
    for (const auto& id : model.getOrder())
        if (const auto value = model.getInstance(id); value && value->slot >= 0 && value->slot < kMaxSlots)
            identities[(size_t)value->slot] = id.toString() + ":" + juce::String(value->modelId) + ":" + juce::String(value->missing ? 1 : 0);
    juce::uint32 hash = 2166136261u;
    for (const auto& identity : identities) hash = (hash ^ (juce::uint32)identity.hashCode()) * 16777619u;
    return hash == 0 ? 1u : hash;
}
}
