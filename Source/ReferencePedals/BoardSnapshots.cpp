// OWNER: WP4
#include "BoardSnapshots.h"
namespace luthier::reference
{
namespace
{
bool invalid (juce::String* error, const char* text) { if (error) *error = text; return false; }
}
juce::var BoardSnapshots::get (const BoardModel& model, int index)
{
    if (index < 0 || index >= count) return {};
    const auto bank = model.getBoardSettings().unknown["source"]["snapshots"].clone();
    if (const auto* a = bank.getArray()) if (index < a->size()) return a->getReference(index).clone();
    return {};
}
bool BoardSnapshots::capture (BoardModel& model, int index, const juce::String& name, juce::String* error)
{
    if (index < 0 || index >= count) return invalid(error,"Invalid snapshot index");
    juce::var preset;
    if (! BoardState::save (model, preset, error)) return false;
    if (auto* root = preset.getDynamicObject()) root->setProperty ("name", name);
    // The model validates the bank and captures runtime extras outside its mutex.
    // Use the command path so capture is one undo step, not a full host restore.
    return model.apply (cmd::SetScene { index, preset }, error);
}

bool BoardSnapshots::recall (BoardModel& model, int index, juce::String* error)
{
    if (index < 0 || index >= count) return invalid(error,"Invalid snapshot index");
    // ApplySnapshot preserves the bank and outgoing runtime state for undo.
    return model.apply (cmd::ApplySnapshot { index, {} }, error);
}
}
