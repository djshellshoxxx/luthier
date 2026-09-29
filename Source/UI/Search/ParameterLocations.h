#pragma once

/*  global-search.md 3.2 and 3.4: what the live walk cannot know.

    Most controls are found live (LiveControls). Two things are declared here
    instead, as rows of { idPattern, location, gate }:

      - controls that exist only when a popover is open: the headstock
        TuningPopover, the WhammyPopover and the Easy rig strip's CompactRack
        slot popovers. The navigator runs the location's steps, which builds
        the control, then finds it in LiveControls.

      - context gates (3.4): parameters that do nothing, and are refused an
        inline set, unless Slide Mode is on (the SLIDE group), a bass is
        loaded (SLAP and the bass grid), a whammy bridge is fitted or an
        acoustic guitar is loaded (the acoustic mics, mic-placement.md 3).

    Patterns are juce wildcards, '|'-separated.
*/

#include "SearchItem.h"

#include <functional>

namespace luthier
{
class LuthierAudioProcessor;
}

namespace luthier::search
{

namespace ParameterLocations
{
    enum class Gate { none, slideMode, bass, whammy, acoustic };

    struct Row
    {
        const char* idPattern;
        UiLocation location;                ///< empty: only a gate
        Gate gate = Gate::none;

        /** False when the popover would not open (a hardtail's bridge). */
        std::function<bool (LuthierAudioProcessor&)> usable;
    };

    const std::vector<Row>& rows();

    bool matches (const char* pattern, const juce::String& parameterId);

    /** The popover route for a parameter in Easy Mode, or empty. Pedal slot
        parameters get their rack slot ("rack:post:3"). */
    UiLocation popoverLocationFor (const juce::String& parameterId, LuthierAudioProcessor& processor);

    Gate gateFor (const juce::String& parameterId);

    /** The gate's availability in the processor's current state. */
    Availability evaluate (Gate gate, LuthierAudioProcessor& processor);

    bool isSlideModeOn (LuthierAudioProcessor& processor);
    bool isBassLoaded (LuthierAudioProcessor& processor);
    bool isWhammyFitted (LuthierAudioProcessor& processor);
    bool isAcousticLoaded (LuthierAudioProcessor& processor);

    /** "pre3_p2" -> chain (false = pre), slot 2 (0-based), param 2; false if
        it is not a pedal-slot parameter. `param` is -1 for type/bypass/mix. */
    bool parseSlotParameter (const juce::String& parameterId, bool& post, int& slot, int& param);
}

} // namespace luthier::search
