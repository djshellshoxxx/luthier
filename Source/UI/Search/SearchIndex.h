#pragma once

/*  global-search.md 3.1, 4.1, 4.5, 11: the index.

    Owns the providers and their items, answers queries, and keeps the recent
    store's bonuses. One per editor (each plugin instance has its own), message
    thread only. The recent store itself is user-global (RecentStore), so two
    instances share their recent items (12, multi-instance).

    Items are collected lazily: query() re-collects any provider whose
    generation moved, so a new preset or a rescanned part library appears on
    the next query without anyone telling the palette (GS-06).
*/

#include "SearchProvider.h"
#include "SearchMatcher.h"

#include <map>
#include <unordered_map>
#include <memory>

namespace luthier::search
{

//==============================================================================
/*  4.5: recent items and recent searches, in UiPreferences as JSON strings
    under search.recentItems / search.recentQueries. Never in presets, host
    state or telemetry. */
class RecentStore
{
public:
    static RecentStore& get();

    struct Entry
    {
        juce::String id;
        juce::int64 lastUsedMs = 0;
        int uses = 0;
    };

    static constexpr int kMaxItems = 200;
    static constexpr int kMaxQueries = 10;

    static constexpr const char* itemsKey = "search.recentItems";
    static constexpr const char* queriesKey = "search.recentQueries";
    static constexpr const char* rememberKey = "search.rememberRecent";

    /** 7: "Remember recent searches". Off clears both stores. */
    bool isEnabled() const;
    void setEnabled (bool shouldRemember);

    void recordItem (const juce::String& id, juce::int64 nowMs);
    void recordQuery (const juce::String& text);

    /** Most recent first. */
    const std::vector<Entry>& getItems() const noexcept { return items; }
    const juce::StringArray& getQueries() const noexcept { return queries; }
    const Entry* find (const juce::String& id) const;

    void remove (const juce::String& id);
    void clear();

    /** Re-reads UiPreferences (another instance, or a test, wrote them). */
    void reload();

private:
    RecentStore();
    void save() const;

    std::vector<Entry> items;
    juce::StringArray queries;
};

//==============================================================================
class SearchIndex
{
public:
    SearchIndex();
    ~SearchIndex();

    //==========================================================================
    void addProvider (std::unique_ptr<SearchProvider> provider);
    SearchProvider* getProvider (const juce::String& id) const;
    SearchProvider* getProviderFor (const SearchItem& item) const;
    int getNumProviders() const noexcept { return (int) providers.size(); }

    /** Re-collects providers whose generation moved. Called by query(). */
    void refreshIfNeeded();

    /** Forces a re-collect of one provider, or all when `id` is empty. */
    void invalidate (const juce::String& providerId = {});

    /** Every item, including ones hidden by default. */
    const std::vector<const SearchItem*>& getItems();
    const SearchItem* find (const juce::String& itemId);

    /** Duplicate ids dropped at the last collect ("provider: id"). */
    const juce::StringArray& getDuplicateLog() const noexcept { return duplicateLog; }

    /** A debug build asserts on a duplicate (13); GS-44 makes them on purpose. */
    bool assertOnDuplicates = true;

    //==========================================================================
    /** 4.1's scope chips; a leading character narrows too. */
    enum class Scope { all, controls, places, commands, content, help, parametersOnly };

    static const juce::StringArray& getScopeNames();   ///< All, Controls, Places, Commands, Content, Help
    static bool scopeIncludes (Scope scope, ItemKind kind) noexcept;

    /** Strips a scope prefix (> ? # @ =) and returns the rest. */
    static juce::String parseScope (const juce::String& raw, Scope& scope);

    struct Result
    {
        const SearchItem* item = nullptr;
        double score = 0.0;
        double matchScore = 0.0;   ///< before bonuses
        Availability availability = Availability::available;
    };

    /** At most 50, best first, fully deterministic (4.1). `includeHidden`
        admits items hidden by default (GS-01 counts them). */
    std::vector<Result> query (const juce::String& rawQuery, Scope chip = Scope::all, bool includeHidden = false);

    /** The best match score (no bonuses) among `kinds`, for 4.4's two readings. */
    Result bestMatch (const juce::String& nameQuery, std::initializer_list<ItemKind> kinds);

    /** The kind order used for ties. */
    static int kindRank (ItemKind kind) noexcept { return (int) kind; }

    /** Deterministic ordering: score, kind, title, id. */
    static bool before (const Result& a, const Result& b) noexcept;

    //==========================================================================
    // Ranking context (4.1).

    /** Is the item visible in the current mode without switching (+40)? */
    std::function<bool (const SearchItem&)> isVisibleNow;

    /** 1 in English, 0.8 elsewhere; SearchIndex asks Localisation when unset. */
    std::function<double()> englishWeight;

    /** The clock for recency (GS-13 mocks it). */
    std::function<juce::int64()> nowMs;

    /** The bonuses for one item, exposed for GS-13. */
    double recencyBonus (const juce::String& id) const;
    double frequencyBonus (const juce::String& id) const;

    /** 4.5: an activation, recorded when "Remember recent" is on. */
    void recordActivation (const juce::String& itemId, const juce::String& committedQuery);

    /** The most recent items that still resolve, pruning the ones that do not. */
    std::vector<const SearchItem*> getRecentItems (int maxItems);

    /** Every distinct word in the titles, for did-you-mean (6.2). */
    juce::String didYouMean (const juce::String& rawQuery);

private:
    struct Slot
    {
        std::unique_ptr<SearchProvider> provider;
        juce::uint32 generation = 0;
        bool collected = false;
        std::vector<SearchItem> items;
    };

    void rebuildFlat();
    static void prepareItem (SearchItem& item);

    std::vector<Slot> providers;
    std::vector<const SearchItem*> flat;
    struct StringHash { size_t operator() (const juce::String& s) const noexcept { return (size_t) s.hashCode64(); } };

    std::unordered_map<juce::String, const SearchItem*, StringHash> byId;
    std::unordered_map<const SearchItem*, SearchProvider*> owner;
    std::vector<std::u32string> vocabulary;
    juce::StringArray duplicateLog;
    bool flatDirty = true;
};

} // namespace luthier::search
