#pragma once

/*  action-and-undo.md: the processor's undo stack.

    The stack is snapshot based: each entry keeps the whole plugin state from
    before its action (and, once undone, from after it), so any single entry
    reverses atomically (section 6). On top of that this class carries what
    the spec asks an entry to be (section 1): an action class, a target, a
    timestamp and a boundary flag, and it applies the rules that use them -
    the 200-entry cap (2), 200 ms grouping (4) and boundaries (5).

    A snapshot restore would also rewrite everything else in the state, so
    undo leaves out the layers that are not values the user changed (3.17 view
    state, 7 A/B, the tune with its own history, the bench's A/B slots):
    see UndoState::withSessionLayers.

    Message thread only.
*/

#include <juce_core/juce_core.h>

#include <functional>

namespace luthier
{

namespace UndoState
{
    /*  Returns `state` (a getStateInformation blob) with the session layers
        taken out or replaced by `keep`: each property of `keep` overwrites the
        one in the state, and each key in `drop` is removed, so the restore
        leaves whatever it guards alone. */
    juce::MemoryBlock withSessionLayers (const juce::MemoryBlock& state,
                                        const juce::NamedValueSet& keep,
                                        const juce::StringArray& drop);
}

//==============================================================================
class UndoHistory
{
public:
    static constexpr int kMaxEntries = 200;          // section 2
    static constexpr double kGroupWindowMs = 200.0;  // sections 0.3 and 4

    struct Entry
    {
        juce::MemoryBlock before;     ///< The state before the action.
        juce::MemoryBlock after;      ///< The state after it, filled in on undo.

        juce::String actionClass;     ///< "param", "mod-route-edit", ... Empty never groups.
        juce::String target;          ///< Parameter ID, route, snapshot index. Empty never groups.
        juce::String description;

        /*  For descriptions that carry a before and after value ("Change Gain
            from 0.20 to 0.70"): a merged entry keeps the first entry's `fromText`
            and the latest `toText`. Empty for everything else. */
        juce::String subject, fromText, toText;

        double startMs = 0.0;         ///< When the action began (a gesture's start).
        double timeMs = 0.0;          ///< Its most recent contribution.
        bool boundary = false;        ///< Section 5.
    };

    /** Tests pin time; the default is the millisecond counter. */
    void setClock (std::function<double()> newClock) { clock = std::move (newClock); }
    double now() const { return clock != nullptr ? clock() : juce::Time::getMillisecondCounterHiRes(); }

    /*  Adds an entry, dropping the redo tail and the oldest over the cap - or,
        when it is the same class on the same target within 200 ms of the top
        entry and neither is a boundary, merges it into the top entry (section
        4): the top keeps its before-state and takes the new time and after
        value. Returns true when it merged. */
    bool push (Entry&& entry);

    /*  Plain undo stops at a boundary (section 5): the boundary entry itself can
        be undone, but once it has been, canUndo is false until the user crosses
        with undoAcrossBoundary. */
    bool canUndo() const noexcept;
    bool canUndoAcrossBoundary() const noexcept { return position >= 0; }
    bool isStoppedAtBoundary() const noexcept { return canUndoAcrossBoundary() && ! canUndo(); }
    bool canRedo() const noexcept { return position + 1 < entries.size(); }

    int getNumUndoSteps() const noexcept { return position + 1; }
    int getNumRedoSteps() const noexcept { return entries.size() - position - 1; }

    /** The entry the next undo reverses, or nullptr. */
    Entry* peekUndo() noexcept;
    const Entry* peekUndo() const noexcept { return const_cast<UndoHistory*> (this)->peekUndo(); }
    const Entry* peekRedo() const noexcept;

    /** Moves back one step and returns the entry to reverse. The caller
        stores the current state in its `after` first and restores `before`. */
    Entry* stepBack (bool crossBoundary);

    /** Moves forward one step and returns the entry to re-apply (`after`). */
    const Entry* stepForward();

    /*  Section 9's history list, newest first: every entry that can be undone
        from here, each with how many undos reach the state before it. */
    struct Item
    {
        juce::String description;
        bool boundary = false;
        int stepsBack = 0;
    };

    juce::Array<Item> getHistory (int maxItems = kMaxEntries) const;

    void clear();

private:
    juce::Array<Entry> entries;
    int position = -1;   // the index the next undo reverses; -1 = nothing
    std::function<double()> clock;
};

} // namespace luthier
