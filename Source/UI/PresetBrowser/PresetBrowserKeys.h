#pragma once

/*  preset-browser-previews.md 7.4: the preset browser's own keys.

    They are scoped to the focused browser, so they live in their own group
    rather than in AccessibilitySettings' global table: Space, F and S are
    already global shortcuts, and that table (rightly) refuses two actions on
    one key. The group is shown and rebound in Options -> ACCESSIBILITY,
    "Preset browser", and stored in UiPreferences under presetBrowser.key.<id>.

    Arrows, Page Up / Down, Home / End, Escape and the digits 0-5 are
    positional, like the global snapshot digits, and are not rebindable.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "../UiPreferences.h"

namespace luthier
{

class PresetBrowserKeys
{
public:
    struct Action { const char* id; const char* label; juce::KeyPress defaultKey; };

    static const std::vector<Action>& actions()
    {
        using KP = juce::KeyPress;
        static const std::vector<Action> list {
            { "focusSearch", "Focus the search box",     KP ('f', juce::ModifierKeys::commandModifier, 0) },
            { "preview",     "Play or stop the preview", KP (KP::spaceKey) },
            { "load",        "Load the selected preset", KP (KP::returnKey) },
            { "favourite",   "Toggle favourite",         KP ('f', 0, 0) },
            { "soundsLike",  "Sounds like this",         KP ('s', 0, 0) },
            { "clearAll",    "Clear search and filters", KP (KP::backspaceKey, juce::ModifierKeys::commandModifier, 0) },
        };
        return list;
    }

    static juce::KeyPress get (const juce::String& id)
    {
        for (const auto& a : actions())
            if (id == a.id)
            {
                const auto stored = UiPreferences::get().getString ("presetBrowser.key." + id, {});
                return stored.isNotEmpty() ? juce::KeyPress::createFromDescription (stored) : a.defaultKey;
            }

        return {};
    }

    /** Refuses a key another browser action already has (as the global table does). */
    static bool rebind (const juce::String& id, const juce::KeyPress& key)
    {
        for (const auto& a : actions())
            if (id != a.id && get (a.id) == key)
                return false;

        UiPreferences::get().setString ("presetBrowser.key." + id, key.getTextDescription());
        return true;
    }

    static void reset (const juce::String& id)
    {
        UiPreferences::get().remove ("presetBrowser.key." + id);
    }

    static bool matches (const juce::String& id, const juce::KeyPress& key)
    {
        return get (id) == key;
    }
};

} // namespace luthier
