#pragma once

/*  global-search.md 8 (INTEGRATE-2): the riff library's items in the palette.

    Every riff in RiffLibrary's cached index is a `riff:<id>` item (title, genre,
    type, key, tempo, tags; the "#" content scope). Enter opens the RIFFS tab
    with the riff selected; the secondary action auditions it. The list is the
    library's index as loaded: collect() never touches disk.
*/

#include "SearchProvider.h"

#include <functional>

namespace luthier
{
class RiffLibrary;
class RiffBrowser;
}

namespace luthier::search
{

class RiffSearchProvider : public SearchProvider
{
public:
    /** `browser` returns the RIFFS tab's browser once the tab is on screen. */
    RiffSearchProvider (RiffLibrary& library, std::function<RiffBrowser*()> browser);

    juce::String getId() const override { return "riff"; }
    juce::uint32 getGeneration() const override;
    void collect (std::vector<SearchItem>& out) const override;
    Availability availabilityOf (const SearchItem&) const override;
    bool activate (const SearchItem&, ActivationKind, SearchContext&) override;
    juce::StringArray secondaryActions (const SearchItem&) const override;

private:
    RiffLibrary& library;
    std::function<RiffBrowser*()> browser;
};

} // namespace luthier::search
