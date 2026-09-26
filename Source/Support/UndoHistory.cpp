#include "UndoHistory.h"

namespace luthier
{

juce::MemoryBlock UndoState::withSessionLayers (const juce::MemoryBlock& state,
                                                const juce::NamedValueSet& keep,
                                                const juce::StringArray& drop)
{
    const juce::String json (juce::CharPointer_UTF8 (static_cast<const char*> (state.getData())),
                             state.getSize());

    auto parsed = juce::JSON::parse (json);
    auto* root = parsed.getDynamicObject();

    if (root == nullptr)
        return state;

    for (const auto& key : drop)
        root->removeProperty (key);

    for (const auto& property : keep)
        root->setProperty (property.name, property.value);

    const auto out = juce::JSON::toString (parsed, false);

    juce::MemoryBlock block;
    block.append (out.toRawUTF8(), out.getNumBytesAsUTF8());
    return block;
}

} // namespace luthier

namespace luthier
{

//==============================================================================
bool UndoHistory::push (Entry&& entry)
{
    // Section 4: adjacent, same class, same target, within the window, and no
    // boundary on either side. Only the top of the stack with no redo tail is
    // adjacent to a new action.
    if (position >= 0 && position == entries.size() - 1)
    {
        auto& top = entries.getReference (position);

        const bool merges = ! top.boundary && ! entry.boundary
                              && entry.actionClass.isNotEmpty() && entry.target.isNotEmpty()
                              && top.actionClass == entry.actionClass && top.target == entry.target
                              && entry.startMs - top.timeMs <= kGroupWindowMs;

        if (merges)
        {
            top.timeMs = entry.timeMs;
            top.toText = entry.toText;

            if (entry.redoAction != nullptr)
                top.redoAction = std::move (entry.redoAction);

            top.description = top.subject.isNotEmpty()
                                ? top.subject + " from " + top.fromText + " to " + top.toText
                                : entry.description;
            return true;
        }
    }

    // A new edit after an undo starts a new branch: the redo tail goes.
    while (entries.size() > position + 1)
        entries.removeLast();

    entries.add (std::move (entry));

    while (entries.size() > kMaxEntries)
        entries.remove (0);

    position = entries.size() - 1;
    return false;
}

bool UndoHistory::wouldMerge (const juce::String& actionClass, const juce::String& target, double startMs) const noexcept
{
    if (position < 0 || position != entries.size() - 1 || actionClass.isEmpty() || target.isEmpty())
        return false;

    const auto& top = entries.getReference (position);
    return ! top.boundary && top.actionClass == actionClass && top.target == target
             && startMs - top.timeMs <= kGroupWindowMs;
}

bool UndoHistory::canUndo() const noexcept
{
    if (position < 0)
        return false;

    // The last entry undone was a boundary: plain undo stops here.
    return ! (position + 1 < entries.size() && entries.getReference (position + 1).boundary);
}

UndoHistory::Entry* UndoHistory::peekUndo() noexcept
{
    return position >= 0 ? &entries.getReference (position) : nullptr;
}

const UndoHistory::Entry* UndoHistory::peekRedo() const noexcept
{
    return canRedo() ? &entries.getReference (position + 1) : nullptr;
}

UndoHistory::Entry* UndoHistory::stepBack (bool crossBoundary)
{
    if (! (crossBoundary ? canUndoAcrossBoundary() : canUndo()))
        return nullptr;

    return &entries.getReference (position--);
}

const UndoHistory::Entry* UndoHistory::stepForward()
{
    if (! canRedo())
        return nullptr;

    return &entries.getReference (++position);
}

juce::Array<UndoHistory::Item> UndoHistory::getHistory (int maxItems) const
{
    juce::Array<Item> items;

    for (int i = position; i >= 0 && items.size() < maxItems; --i)
    {
        const auto& e = entries.getReference (i);
        items.add ({ e.description, e.boundary, position - i + 1 });
    }

    return items;
}

void UndoHistory::clear()
{
    entries.clear();
    position = -1;
}

} // namespace luthier
