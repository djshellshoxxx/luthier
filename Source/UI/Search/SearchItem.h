#pragma once

/*  Global search (global-search.md 2, 3.1, 3.3, 3.4): one indexed thing.

    Every entry in the palette - a parameter, a place, a command, a preset, a
    help topic, a riff a parallel feature registers - is one of these. The id is
    stable across sessions (recent items are stored by it), the kind decides the
    tie-break order and the glyph, and the location says how to get there.

    Message thread only, like everything in Source/UI/Search.
*/

#include <juce_core/juce_core.h>

#include <string>
#include <vector>

namespace luthier::search
{

//==============================================================================
/*  The kinds, declared in 4.1's tie-break order so the enum's value is the
    rank: Parameter, Place, Command, Preset, Guitar, Part, Pedal, Snapshot,
    Help, Shortcut, Setting, then provider kinds. A choice option sorts with the
    parameters it belongs to. */
enum class ItemKind
{
    parameter = 0,
    choiceOption,
    place,
    command,
    preset,
    guitar,
    part,
    pedal,
    snapshot,
    help,
    shortcut,
    setting,
    provider
};

const char* getKindName (ItemKind kind) noexcept;   ///< "Parameter", "Place", ...

//==============================================================================
/** 3.4. Computed when asked, never cached: guitar, mode and width change. */
enum class Availability
{
    available = 0,
    needsSlideMode,
    needsBass,
    needsWhammy,
    needsEmptySlot,
    modeUnavailable,
    proLocked,
    notBuilt
};

inline bool isLocked (Availability a) noexcept
{
    return a == Availability::proLocked || a == Availability::notBuilt;
}

inline bool isContextGate (Availability a) noexcept
{
    return a == Availability::needsSlideMode || a == Availability::needsBass
        || a == Availability::needsWhammy || a == Availability::needsEmptySlot;
}

//==============================================================================
/** How a result was activated (4.4, 5). */
enum class ActivationKind
{
    primary,     ///< Enter, a click
    keepOpen,    ///< Shift+Enter
    goOnly,      ///< Ctrl+Enter: go to the control without applying
    secondary    ///< Alt+Enter / right-click: an entry of secondaryActions
};

//==============================================================================
/** 3.3: one step of a route to a control or a place. */
struct LocationStep
{
    enum class Type
    {
        mode,              ///< name: "Easy", "Advanced" or "Either"
        workspaceTab,      ///< name: the tab's label
        subTab,            ///< name: "<host place id>|<sub-tab>"
        column,            ///< number: 1-3
        overlay,           ///< name: options / presetBrowser / workshop / help / export / chords / debug / saveAs
        optionsPage,       ///< name: the page's tab label
        drawerTab,         ///< name: the practice drawer's tab label
        popover,           ///< name: headstock / bridge / rack:<pre|post>:<slot>
        workshopCategory   ///< name: a WorkshopPanel::drawerCategories() entry
    };

    Type type = Type::mode;
    juce::String name;
    int number = 0;

    static LocationStep make (Type t, const juce::String& n, int num = 0) { return { t, n, num }; }
};

struct UiLocation
{
    std::vector<LocationStep> steps;

    bool isEmpty() const noexcept { return steps.empty(); }
};

//==============================================================================
struct SearchItem
{
    juce::String id;              ///< "param:amp_gain", "place:tab:WORKSHOP", ...
    ItemKind kind = ItemKind::parameter;
    juce::String providerId;      ///< which SearchProvider made it

    juce::String title;           ///< localized
    juce::String englishTitle;    ///< for English tutorials in every locale (2)
    juce::StringArray synonyms;   ///< search.syn.<id>, localized, English fallback
    juce::StringArray keywords;   ///< units spelled out, aliases, tags
    juce::String breadcrumb;      ///< "Adv › Col 2 › CIRCUIT"

    juce::String target;          ///< kind-specific: the parameter id, the action id, the topic id...
    int index = -1;               ///< kind-specific: an option, a preset or a snapshot index
    UiLocation location;

    /** Left out of default results (an empty pedal slot's knobs, 2) but still
        counted as indexed. */
    bool hiddenByDefault = false;

    /** Visible in Easy (true), Advanced (false), or wherever (both true). */
    bool inEasy = true, inAdvanced = true;

    //==========================================================================
    // Pre-computed by SearchIndex at build time (11): a query then allocates
    // only its result vector.
    struct Prepared
    {
        std::u32string text;                 ///< normalised
        std::vector<int> wordStarts;         ///< code-point offsets into text
        std::vector<std::u32string> words;   ///< the words, in order
        std::u32string acronym;
    };

    Prepared preparedTitle, preparedEnglish;
    std::vector<Prepared> preparedSynonyms;   ///< curated synonyms (search.syn.*)
    std::vector<Prepared> preparedKeywords;   ///< id words, units, tags
    std::vector<std::u32string> breadcrumbWords;
};

} // namespace luthier::search
