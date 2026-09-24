#include "ParameterLocations.h"

#include "../../PluginProcessor.h"
#include "../../Parameters.h"
#include "../GuitarBodyComponent.h"

namespace luthier::search
{
namespace ParameterLocations
{
namespace
{
    using T = LocationStep::Type;

    UiLocation easyPopover (const juce::String& name)
    {
        UiLocation l;
        l.steps.push_back (LocationStep::make (T::mode, "Easy"));
        l.steps.push_back (LocationStep::make (T::popover, name));
        return l;
    }
}

const std::vector<Row>& rows()
{
    static const std::vector<Row> table =
    {
        // gui-integration 3.1: the headstock popover in Easy Mode.
        { "tuning_preset|temperament|capo_fret|concert_a", easyPopover ("headstock"), Gate::none, {} },

        // The bridge popover, only when a whammy is fitted (3.1). bridge_type
        // itself is not gated: it is how a whammy gets fitted.
        { "bridge_type", easyPopover ("bridge"), Gate::none,
          [] (LuthierAudioProcessor& p) { return isWhammyFitted (p); } },
        { "whammy_*|transpose_lock", easyPopover ("bridge"), Gate::whammy,
          [] (LuthierAudioProcessor& p) { return isWhammyFitted (p); } },

        // slide-technique-controls / gui-integration 7: the SLIDE group shows
        // only in Slide Mode.
        { "slide_pressure|slide_slant|slide_damping_behind|slide_intonation_assist|slide_noise_amount|slide_clank_amount",
          {}, Gate::slideMode, {} },

        // string-slap-technique / bass-techniques: SLAP and the bass grid show
        // only with a bass loaded.
        { "slap_*|pop_*|double_thump_*|ghost_*|finger_alternation_variation|rest_stroke",
          {}, Gate::bass, {} },
    };

    return table;
}

bool matches (const char* pattern, const juce::String& parameterId)
{
    juce::StringArray alternatives;
    alternatives.addTokens (pattern, "|", {});

    for (const auto& a : alternatives)
        if (parameterId.matchesWildcard (a, false))
            return true;

    return false;
}

bool parseSlotParameter (const juce::String& id, bool& post, int& slot, int& param)
{
    juce::String rest;

    if (id.startsWith ("post"))     { post = true;  rest = id.substring (4); }
    else if (id.startsWith ("pre")) { post = false; rest = id.substring (3); }
    else return false;

    const int underscore = rest.indexOfChar ('_');

    if (underscore <= 0 || ! rest.substring (0, underscore).containsOnly ("0123456789"))
        return false;

    slot = rest.substring (0, underscore).getIntValue();
    const auto tail = rest.substring (underscore + 1);

    if (tail == "type" || tail == "bypass" || tail == "mix")
    {
        param = -1;
        return true;
    }

    if (tail.startsWithChar ('p') && tail.length() > 1 && tail.substring (1).containsOnly ("0123456789"))
    {
        param = tail.substring (1).getIntValue();
        return true;
    }

    return false;
}

UiLocation popoverLocationFor (const juce::String& parameterId, LuthierAudioProcessor& processor)
{
    bool post = false;
    int slot = 0, param = 0;

    if (parseSlotParameter (parameterId, post, slot, param))
        return easyPopover ("rack:" + juce::String (post ? "post" : "pre") + ":" + juce::String (slot));

    for (const auto& row : rows())
        if (! row.location.isEmpty() && matches (row.idPattern, parameterId))
            if (row.usable == nullptr || row.usable (processor))
                return row.location;

    return {};
}

Gate gateFor (const juce::String& parameterId)
{
    for (const auto& row : rows())
        if (row.gate != Gate::none && matches (row.idPattern, parameterId))
            return row.gate;

    return Gate::none;
}

bool isSlideModeOn (LuthierAudioProcessor& processor)
{
    auto* p = processor.getState().getParameter (ParamIDs::slideGuitar);
    return p != nullptr && p->getValue() > 0.5f;
}

bool isBassLoaded (LuthierAudioProcessor& processor)
{
    return processor.getEngine().getGuitarSpec().category == GuitarCategory::Bass;
}

bool isWhammyFitted (LuthierAudioProcessor& processor)
{
    return WhammyPopover::isWhammyFitted (processor);
}

Availability evaluate (Gate gate, LuthierAudioProcessor& processor)
{
    switch (gate)
    {
        case Gate::slideMode: return isSlideModeOn (processor) ? Availability::available : Availability::needsSlideMode;
        case Gate::bass:      return isBassLoaded (processor)  ? Availability::available : Availability::needsBass;
        case Gate::whammy:    return isWhammyFitted (processor) ? Availability::available : Availability::needsWhammy;
        case Gate::none:
        default: break;
    }

    return Availability::available;
}

} // namespace ParameterLocations
} // namespace luthier::search
