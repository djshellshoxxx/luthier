#pragma once

/*  action-and-undo.md: the pieces of the processor's undo stack that do not
    need the processor.

    The stack is snapshot based: each entry keeps the whole plugin state from
    before its action (and, once undone, from after it). A snapshot restore
    would also rewrite everything else in the state, so undo leaves out the
    layers that are not values the user changed (3.17 view state, 7 A/B,
    the tune with its own history, the setlist, the bench's A/B slots).
*/

#include <juce_core/juce_core.h>

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

} // namespace luthier
