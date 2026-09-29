#pragma once

/*  global-search.md 8: the provider contract.

    A feature built in parallel (jam mode, the riff library, the new preset
    browser, mic placement, auto-articulation, animated strings...) registers
    its searchable things by implementing this and adding it to the editor's
    index in LuthierAudioProcessorEditor::buildSearchProviders(). It never
    touches the search code. docs/SEARCH_INTEGRATION.md is the how-to.

    Rules:
      - collect() runs on the message thread, must finish in 5 ms for 2,000
        items and must not touch disk: read your manager's cached list.
      - Bump getGeneration() whenever that list changes; the index re-collects
        only providers whose generation moved.
      - availabilityOf() is asked on every query, never cached by the index,
        so compute it from live state.
      - Item ids are "<provider>:<id>" and must be stable across sessions:
        recent items are stored by id.
*/

#include "SearchItem.h"

namespace luthier::search
{

//==============================================================================
/*  What the palette offers a provider when one of its items is activated: the
    editor's services, without the provider knowing the editor. */
class SearchContext
{
public:
    virtual ~SearchContext() = default;

    /** Runs a UiLocation (3.3): switches mode, opens tabs / overlays / pages /
        drawer tabs / popovers and highlights the place. */
    virtual bool openLocation (const UiLocation& location, const juce::String& announceAs) = 0;

    /** Runs a command through the one action path (4.3). */
    virtual bool performAction (const juce::String& actionId) = 0;

    /** Writes the palette's footer line, in the warning colour if `warning`. */
    virtual void showFooterMessage (const juce::String& text, bool warning) = 0;

    /** Puts a banner up (for results that outlive the palette). */
    virtual void postNotice (const juce::String& text) = 0;

    /** Closes the palette (activation normally does; a provider that opens a
        dialog of its own can close it first). */
    virtual void closePalette() = 0;

    /** For ActivationKind::secondary: which entry of secondaryActions(). */
    int secondaryIndex = -1;
};

//==============================================================================
struct SearchProvider
{
    virtual ~SearchProvider() = default;

    /** "riffs". Also the prefix of every id this provider makes. */
    virtual juce::String getId() const = 0;

    /** Bump to have collect() called again. */
    virtual juce::uint32 getGeneration() const = 0;

    /** Message thread; 5 ms for 2,000 items; no disk. */
    virtual void collect (std::vector<SearchItem>& out) const = 0;

    virtual Availability availabilityOf (const SearchItem&) const = 0;

    /** Enter (primary), Shift+Enter (keepOpen), Ctrl+Enter (goOnly), or one of
        secondaryActions() (secondary, with context.secondaryIndex). Returns
        false if nothing happened, and the palette says so. */
    virtual bool activate (const SearchItem&, ActivationKind, SearchContext&) = 0;

    /** Alt+Enter / right-click entries after the defaults (5). */
    virtual juce::StringArray secondaryActions (const SearchItem&) const { return {}; }
};

} // namespace luthier::search
