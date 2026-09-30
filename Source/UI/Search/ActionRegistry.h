#pragma once

/*  global-search.md 4.3: every command, once.

    A command run from the palette, from its shortcut and from its button all
    call the same function: LuthierAudioProcessorEditor::performAction, which
    is the body keyPressed used to have. The registry is the list of those ids
    with their titles, availability and undo class, built by the editor from
    AccessibilitySettings' shortcuts plus the commands that have no key.

    A command whose feature is not built is not registered: absent, not
    present and dead (the rule buildDefaultShortcuts already follows).
*/

#include "SearchItem.h"

#include <functional>
#include <vector>

namespace luthier::search
{

/** action-and-undo's classes, as far as the palette needs to say them (9). */
enum class UndoClass
{
    none,          ///< navigation, opening things, mode switches (3.17)
    parameter,     ///< 3.1
    slideMode,     ///< 3.3
    partSwap,      ///< 3.4
    snapshot,      ///< 3.7
    pedal,         ///< 3.13
    boundary,      ///< 5: a preset load
    skipsStack     ///< 7: panic, tap tempo
};

struct ActionDef
{
    juce::String id;
    juce::String titleKey;        ///< a catalog key; accessibility.shortcut.<id> for shortcuts
    juce::String synonymsKey;     ///< search.syn.cmd:<id>
    std::function<Availability()> available;
    std::function<bool()> perform;
    UndoClass undo = UndoClass::none;

    /** A title given directly (a snapshot's number, a technique's name) instead of a key. */
    juce::String fixedTitle;
};

class ActionRegistry
{
public:
    void add (ActionDef def);
    void clear() { actions.clear(); }

    const ActionDef* find (const juce::String& id) const;
    const std::vector<ActionDef>& getActions() const noexcept { return actions; }

    /** Runs a command; false if unknown, unavailable or it did nothing. */
    bool perform (const juce::String& id) const;

    Availability availabilityOf (const juce::String& id) const;

    /** The localized title. */
    static juce::String titleOf (const ActionDef& def);

    /** Bumped on every add, for the command provider's generation. */
    juce::uint32 getGeneration() const noexcept { return generation; }

private:
    std::vector<ActionDef> actions;
    juce::uint32 generation = 1;
};

} // namespace luthier::search
