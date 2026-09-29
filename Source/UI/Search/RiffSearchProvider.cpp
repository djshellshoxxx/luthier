#include "RiffSearchProvider.h"
#include "../../Riffs/RiffLibrary.h"
#include "../RiffBrowser.h"

namespace luthier::search
{

RiffSearchProvider::RiffSearchProvider (RiffLibrary& l, std::function<RiffBrowser*()> b)
    : library (l), browser (std::move (b))
{
}

juce::uint32 RiffSearchProvider::getGeneration() const
{
    // The index changes as a whole (a load) or by one riff (a save or a
    // delete): the count and the loaded flag cover both.
    return (juce::uint32) library.getNumEntries() * 2u + (library.isIndexLoaded() ? 1u : 0u);
}

void RiffSearchProvider::collect (std::vector<SearchItem>& out) const
{
    // The index loads off the message thread the first time anything asks for
    // it (the RIFFS tab does the same); its arrival moves the generation.
    if (! library.isIndexLoaded() && ! library.isLoading())
        if (auto* b = browser ? browser() : nullptr)
            b->ensureLibraryLoaded (false);

    for (int i = 0; i < library.getNumEntries(); ++i)
    {
        const auto* e = library.getEntry (i);

        if (e == nullptr || e->id.isEmpty())
            continue;

        SearchItem item;
        item.id = "riff:" + e->id;
        item.kind = ItemKind::provider;
        item.providerId = getId();
        item.title = e->name;
        item.englishTitle = e->name;
        item.target = e->id;
        item.breadcrumb = "Riffs > " + e->genre + " > " + e->type
                            + (e->keyRoot.isNotEmpty() ? " > " + e->keyRoot + " " + e->scale : juce::String())
                            + " > " + juce::String (juce::roundToInt (e->tempoBpm)) + " bpm";
        item.keywords = e->tags;
        item.keywords.addArray (e->techniques);
        item.keywords.add (e->genre);
        item.keywords.add (e->type);
        item.keywords.add ("riff");
        item.location.steps = { LocationStep::make (LocationStep::Type::mode, "Advanced"),
                                LocationStep::make (LocationStep::Type::workspaceTab, "RIFFS") };
        item.inEasy = false;
        out.push_back (std::move (item));
    }
}

Availability RiffSearchProvider::availabilityOf (const SearchItem& item) const
{
    return library.findEntry (item.target) != nullptr ? Availability::available : Availability::notBuilt;
}

juce::StringArray RiffSearchProvider::secondaryActions (const SearchItem&) const
{
    return { "Audition" };
}

bool RiffSearchProvider::activate (const SearchItem& item, ActivationKind kind, SearchContext& context)
{
    if (library.findEntry (item.target) == nullptr)
    {
        context.showFooterMessage (item.title + " no longer exists", true);
        return false;
    }

    // riff-library 7.1: the RIFFS tab, with the riff selected (filters that
    // would hide it are cleared).
    context.openLocation (item.location, item.title);

    auto* b = browser ? browser() : nullptr;

    if (b == nullptr)
        return false;

    if (b->getSelectedId() != item.target)
    {
        b->clearFilters();
        b->selectRiff (item.target);
    }

    if (kind == ActivationKind::secondary && context.secondaryIndex == 0)
        b->audition();

    return b->getSelectedId() == item.target;
}

} // namespace luthier::search
