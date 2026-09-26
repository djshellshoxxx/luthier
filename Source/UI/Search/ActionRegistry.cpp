#include "ActionRegistry.h"
#include "SearchCatalog.h"

#include "../../Accessibility/Localisation.h"

namespace luthier::search
{

void ActionRegistry::add (ActionDef def)
{
    if (def.id.isEmpty())
        return;

    if (def.synonymsKey.isEmpty())
        def.synonymsKey = "search.syn.cmd:" + def.id;

    for (auto& existing : actions)
        if (existing.id == def.id)
        {
            existing = std::move (def);
            ++generation;
            return;
        }

    actions.push_back (std::move (def));
    ++generation;
}

const ActionDef* ActionRegistry::find (const juce::String& id) const
{
    for (auto& a : actions)
        if (a.id == id)
            return &a;

    return nullptr;
}

Availability ActionRegistry::availabilityOf (const juce::String& id) const
{
    if (const auto* a = find (id))
        return a->available ? a->available() : Availability::available;

    return Availability::notBuilt;
}

bool ActionRegistry::perform (const juce::String& id) const
{
    const auto* a = find (id);

    if (a == nullptr || a->perform == nullptr)
        return false;

    const auto availability = a->available ? a->available() : Availability::available;

    if (isLocked (availability))
        return false;

    return a->perform();
}

juce::String ActionRegistry::titleOf (const ActionDef& def)
{
    if (def.fixedTitle.isNotEmpty())
        return def.fixedTitle;

    // The shortcut descriptions live in Localisation's own catalog; the
    // palette's commands in the search catalog. Either answers.
    if (Localisation::get().hasKey (def.titleKey))
    {
        const auto t = Localisation::get().translate (def.titleKey);

        if (t != def.titleKey)
            return t;
    }

    if (const auto t = SearchCatalog::text (def.titleKey); t.isNotEmpty())
        return t;

    return def.id;
}

} // namespace luthier::search
