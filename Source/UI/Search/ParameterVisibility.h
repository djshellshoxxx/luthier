#pragma once

/*  Parameters with no visible control on purpose (global-search.md 2).

    One list, read by two things that must agree: GuiReachabilityTests, which
    fails for any other parameter without a control, and the search index,
    which leaves these out. It is the only exemption either has. Each needs a
    reason a reviewer would accept.
*/

#include <juce_core/juce_core.h>

#include <map>

namespace luthier::ParameterVisibility
{
    inline const std::map<juce::String, juce::String>& intentionallyHidden()
    {
        static const std::map<juce::String, juce::String> m
        {
            { "feedback_on",        "superseded by feedback_amount (ambiguity-resolutions 1.2); kept for automation indices" },
            { "feedback_threshold", "superseded by the physical feedback loop (ambiguity-resolutions 1.2)" },
            { "feedback_speed",     "superseded by the physical feedback loop (ambiguity-resolutions 1.2)" },
            { "fret_action",        "superseded by the setup geometry (DECISIONS.md, fret-buzz 7); inert, kept for automation indices" },
            { "doubler_on",         "legacy engine doubler: a load migrates it to a Doubler pedal (PresetManager::fromVar)" },
            { "doubler_amount",     "legacy engine doubler; the Doubler pedal's own knobs replace it" },
            { "strum_speed",        "superseded by strum_crossing_sps (strum-dynamics 7); kept for automation indices" },
        };

        return m;
    }

    inline bool isIntentionallyHidden (const juce::String& parameterId)
    {
        return intentionallyHidden().count (parameterId) > 0;
    }
}
