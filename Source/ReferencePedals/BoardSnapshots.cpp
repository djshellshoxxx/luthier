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
    std::vector<PedalInstance> instances; BoardSettings settings; model.snapshot(instances,settings);
    juce::var preset;
    if (! BoardState::encode(instances,settings,preset,error)) return false;
    if ((int)preset["schema"] > kSchemaVersion) return invalid(error,"Edit the newer-format board before capturing a snapshot");
    auto board = preset.clone();
    auto* root = preset.getDynamicObject();
    auto bank = root->getProperty("snapshots").clone();
    if (! bank.isArray()) bank = juce::Array<juce::var>{};
    while (bank.size() < count) bank.append(juce::var());
    root->removeProperty("snapshots"); root->setProperty("name",name);
    bank.getArray()->set(index,preset.clone());
    board.getDynamicObject()->setProperty("snapshots",bank);
    // Reparse the whole bank before replacement to enforce the shared cap.
    return BoardState::restore(model,board,error);
}
bool BoardSnapshots::recall (BoardModel& model, int index, juce::String* error)
{
    if (index < 0 || index >= count) return invalid(error,"Invalid snapshot index");
    auto settings = model.getBoardSettings();
    auto preset = get(model,index);
    if (! preset.getDynamicObject()) return invalid(error,"Snapshot is empty");
    preset.getDynamicObject()->setProperty("snapshots",settings.unknown["source"]["snapshots"].clone());
    preset.getDynamicObject()->removeProperty("name");
    return BoardState::restore(model,preset,error);
}
}
