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
