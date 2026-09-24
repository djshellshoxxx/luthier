#pragma once

/*  What Help says (include.md "Help File", accessibility.md 2 and 7,
    gui-integration.md 4.4, 16, 17 and 20).

    One source for both help surfaces: the HELP workspace tab in Advanced
    column 4, and the Help overlay the header's ? opens in Easy mode. Both show
    a HelpTab (HelpTab.h), which reads everything from here, so the tab and the
    overlay cannot say different things.

    A topic is found by its id, its title or any of its aliases. The aliases are
    the names on the surfaces a user can ask for docs from: the workspace tab
    names (4.4), the section headings in Advanced columns 1 to 3, the Options
    page names (5). That is what "Help pinned to this panel" means in 16 and 20 -
    the panel passes its own name, and a panel renamed without its alias fails
    HelpTabTests rather than silently opening the wrong page.

    No shortcut is written into the text. A body says `{key:save}` and the view
    substitutes whatever that action is bound to now, so a rebind reaches the
    prose as well as the cheat sheet (accessibility 7: "a live view of the
    current bindings, not a static string").

    English only. accessibility 7 wants the manual in the UI locale; the topics
    are long-form prose rather than catalog strings, and translating them is
    ACC-7-01's work (docs/spec-coverage.md), not something this file can fake.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace luthier
{

//==============================================================================
struct HelpTopic
{
    const char* id;       ///< Stable, e.g. "tone-match".
    const char* title;    ///< As the topic list shows it.
    const char* aliases;  ///< '|'-separated panel, tab and section names that pin here.
    const char* body;     ///< May contain {key:actionId} placeholders.
};

//==============================================================================
/** One line of the live cheat sheet (gui-integration 4.4, accessibility 7). */
struct ShortcutRow
{
    juce::String group;        ///< The heading it is listed under.
    juce::String actionId;     ///< Registry id, or empty for a fixed key.
    juce::String keyText;      ///< As the Options rebind table prints it.
    juce::String description;  ///< From the catalog, never the key it is looked up by.
    bool rebound = false;      ///< Differs from gui-integration 17's default.
    bool fixed = false;        ///< Not in the registry and not rebindable (Escape, the digits).
};

//==============================================================================
namespace HelpContent
{
    int getNumTopics() noexcept;

    /** Clamped, so an out-of-range index is the first topic rather than a crash. */
    const HelpTopic& getTopic (int index) noexcept;

    /** The topic an id, title or alias names, or -1. Case-insensitive; '-', '_'
        and runs of spaces are all one space, and a trailing " tab" or " panel" is
        ignored, so "TONE MATCH", "tone-match" and "Tone Match tab" agree. */
    int findTopic (const juce::String& nameOrAlias);

    juce::StringArray getAliases (const HelpTopic& topic);

    /** Replaces each {key:actionId} with the action's current binding. An action
        that is not in the registry reads "(not bound)" rather than a raw
        placeholder. */
    juce::String resolveKeys (const juce::String& text);

    //==========================================================================
    /** Every binding in AccessibilitySettings, grouped, plus the keys that are
        deliberately not in it. An action added to the registry that no group
        names still appears, under "Other": the sheet is built from the
        registry, not from a list here.

        @param filter  accessibility 2's search: matches description, key or id. */
    std::vector<ShortcutRow> getShortcutRows (const juce::String& filter = {});

    /** The same rows as printable text, one per line under group headings. */
    juce::String formatShortcutRows (const std::vector<ShortcutRow>& rows);

    //==========================================================================
    /*  include.md: homepage, source and support. These are the addresses the
        Help overlay has always carried, and they are placeholders: INC-HLP-02 in
        docs/spec-coverage.md is the release blocker that replaces them. */
    inline constexpr const char* homepageUrl  = "https://luthieraudio.example/luthier";
    inline constexpr const char* sourceUrl    = "https://github.com/luthieraudio/luthier";
    inline constexpr const char* supportEmail = "support@luthieraudio.example";

    /** A mailto: with the version in the subject, so support knows what it is
        looking at before the troubleshooting file arrives. */
    juce::URL getSupportMailUrl();
}

} // namespace luthier
