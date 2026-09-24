#include "SearchIndex.h"

#include "../UiPreferences.h"
#include "../../Accessibility/Localisation.h"

#include <algorithm>
#include <cmath>

namespace luthier::search
{

//==============================================================================
RecentStore& RecentStore::get()
{
    static RecentStore instance;
    return instance;
}

RecentStore::RecentStore()
{
    reload();
}

bool RecentStore::isEnabled() const
{
    return UiPreferences::get().getBool (rememberKey, true);
}

void RecentStore::setEnabled (bool shouldRemember)
{
    UiPreferences::get().setBool (rememberKey, shouldRemember);

    // 7: turning it off clears both stores.
    if (! shouldRemember)
        clear();
}

void RecentStore::reload()
{
    items.clear();
    queries.clear();

    const auto parsed = juce::JSON::parse (UiPreferences::get().getString (itemsKey, "[]"));

    if (auto* array = parsed.getArray())
        for (const auto& v : *array)
        {
            Entry e;
            e.id = v.getProperty ("id", {}).toString();
            e.lastUsedMs = (juce::int64) v.getProperty ("t", 0);
            e.uses = (int) v.getProperty ("n", 0);

            if (e.id.isNotEmpty() && (int) items.size() < kMaxItems)
                items.push_back (e);
        }

    const auto parsedQueries = juce::JSON::parse (UiPreferences::get().getString (queriesKey, "[]"));

    if (auto* array = parsedQueries.getArray())
        for (const auto& v : *array)
            if (v.toString().isNotEmpty() && queries.size() < kMaxQueries)
                queries.add (v.toString());
}

void RecentStore::save() const
{
    juce::Array<juce::var> itemArray;

    for (const auto& e : items)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", e.id);
        o->setProperty ("t", e.lastUsedMs);
        o->setProperty ("n", e.uses);
        itemArray.add (juce::var (o));
    }

    juce::Array<juce::var> queryArray;

    for (const auto& q : queries)
        queryArray.add (q);

    UiPreferences::get().setString (itemsKey, juce::JSON::toString (juce::var (itemArray), true));
    UiPreferences::get().setString (queriesKey, juce::JSON::toString (juce::var (queryArray), true));
}

void RecentStore::recordItem (const juce::String& id, juce::int64 nowMs)
{
    if (! isEnabled() || id.isEmpty())
        return;

    Entry entry { id, nowMs, 0 };

    for (auto it = items.begin(); it != items.end(); ++it)
        if (it->id == id)
        {
            entry.uses = it->uses;
            items.erase (it);
            break;
        }

    ++entry.uses;
    items.insert (items.begin(), entry);

    // At most 200, the oldest dropped.
    if ((int) items.size() > kMaxItems)
        items.resize ((size_t) kMaxItems);

    save();
}

void RecentStore::recordQuery (const juce::String& text)
{
    const auto t = text.trim();

    if (! isEnabled() || t.isEmpty())
        return;

    queries.removeString (t);
    queries.insert (0, t);

    while (queries.size() > kMaxQueries)
        queries.remove (queries.size() - 1);

    save();
}

const RecentStore::Entry* RecentStore::find (const juce::String& id) const
{
    for (const auto& e : items)
        if (e.id == id)
            return &e;

    return nullptr;
}

void RecentStore::remove (const juce::String& id)
{
    const auto before = items.size();
    items.erase (std::remove_if (items.begin(), items.end(), [&] (const Entry& e) { return e.id == id; }), items.end());

    if (items.size() != before)
        save();
}

void RecentStore::clear()
{
    items.clear();
    queries.clear();
    save();
}

//==============================================================================
SearchIndex::SearchIndex()
{
    nowMs = [] { return juce::Time::currentTimeMillis(); };
}

SearchIndex::~SearchIndex() = default;

void SearchIndex::addProvider (std::unique_ptr<SearchProvider> provider)
{
    if (provider == nullptr)
        return;

    // A second provider with the same id replaces the first: a feature that
    // re-registers after a rebuild must not double its items.
    for (auto& slot : providers)
        if (slot.provider->getId() == provider->getId())
        {
            slot.provider = std::move (provider);
            slot.collected = false;
            flatDirty = true;
            return;
        }

    Slot slot;
    slot.provider = std::move (provider);
    providers.push_back (std::move (slot));
    flatDirty = true;
}

SearchProvider* SearchIndex::getProvider (const juce::String& id) const
{
    for (auto& slot : providers)
        if (slot.provider->getId() == id)
            return slot.provider.get();

    return nullptr;
}

SearchProvider* SearchIndex::getProviderFor (const SearchItem& item) const
{
    if (auto it = owner.find (&item); it != owner.end())
        return it->second;

    return getProvider (item.providerId);
}

void SearchIndex::invalidate (const juce::String& providerId)
{
    for (auto& slot : providers)
        if (providerId.isEmpty() || slot.provider->getId() == providerId)
            slot.collected = false;
}

void SearchIndex::prepareItem (SearchItem& item)
{
    item.preparedTitle = SearchMatcher::prepare (item.title);
    item.preparedEnglish = item.englishTitle.isNotEmpty() && item.englishTitle != item.title
                             ? SearchMatcher::prepare (item.englishTitle) : SearchItem::Prepared();

    item.preparedSynonyms.clear();

    for (const auto& s : item.synonyms)
        if (s.isNotEmpty())
            item.preparedSynonyms.push_back (SearchMatcher::prepare (s));

    item.preparedKeywords.clear();

    for (const auto& k : item.keywords)
        if (k.isNotEmpty())
            item.preparedKeywords.push_back (SearchMatcher::prepare (k));

    item.preparedBreadcrumb = SearchMatcher::prepare (item.breadcrumb);

    item.charMask = 0;

    auto addMask = [&item] (const SearchItem::Prepared& p)
    {
        for (auto c : p.text)
            item.charMask |= (juce::uint64) 1 << (c % 64);
    };

    addMask (item.preparedTitle);
    addMask (item.preparedEnglish);
    addMask (item.preparedBreadcrumb);

    for (const auto& p : item.preparedSynonyms) addMask (p);
    for (const auto& p : item.preparedKeywords) addMask (p);
}

void SearchIndex::refreshIfNeeded()
{
    for (auto& slot : providers)
    {
        const auto generation = slot.provider->getGeneration();

        if (slot.collected && generation == slot.generation)
            continue;

        slot.items.clear();
        slot.provider->collect (slot.items);

        for (auto& item : slot.items)
        {
            if (item.providerId.isEmpty())
                item.providerId = slot.provider->getId();

            prepareItem (item);
        }

        slot.generation = generation;
        slot.collected = true;
        flatDirty = true;
    }

    if (flatDirty)
        rebuildFlat();
}

void SearchIndex::rebuildFlat()
{
    size_t total = 0;

    for (auto& slot : providers)
        total += slot.items.size();

    flat.clear();
    flat.reserve (total);
    owner.reserve (total);
    byId.clear();
    owner.clear();
    duplicateLog.clear();

    byId.reserve (total);

    for (auto& slot : providers)
    {
        for (auto& item : slot.items)
        {
            // 13: duplicate ids - the later one is dropped and logged.
            if (byId.count (item.id) > 0)
            {
                duplicateLog.add (slot.provider->getId() + ": " + item.id);
                DBG ("search: duplicate item id dropped: " << item.id);
                jassert (! assertOnDuplicates);
                continue;
            }

            flat.push_back (&item);
            byId[item.id] = &item;
            owner[&item] = slot.provider.get();

        }
    }

    vocabulary.clear();   // built on demand by didYouMean
    flatDirty = false;
}

const std::vector<const SearchItem*>& SearchIndex::getItems()
{
    refreshIfNeeded();
    return flat;
}

const SearchItem* SearchIndex::find (const juce::String& itemId)
{
    refreshIfNeeded();

    if (auto it = byId.find (itemId); it != byId.end())
        return it->second;

    return nullptr;
}

//==============================================================================
const juce::StringArray& SearchIndex::getScopeNames()
{
    static const juce::StringArray names { "All", "Controls", "Places", "Commands", "Content", "Help" };
    return names;
}

bool SearchIndex::scopeIncludes (Scope scope, ItemKind kind) noexcept
{
    switch (scope)
    {
        case Scope::all:            return true;
        case Scope::controls:       return kind == ItemKind::parameter || kind == ItemKind::choiceOption || kind == ItemKind::setting;
        case Scope::parametersOnly: return kind == ItemKind::parameter || kind == ItemKind::choiceOption;
        case Scope::places:         return kind == ItemKind::place;
        case Scope::commands:       return kind == ItemKind::command || kind == ItemKind::shortcut;
        case Scope::help:           return kind == ItemKind::help;
        case Scope::content:        return kind == ItemKind::preset || kind == ItemKind::guitar || kind == ItemKind::part
                                        || kind == ItemKind::pedal || kind == ItemKind::snapshot || kind == ItemKind::provider;
        default: break;
    }

    return true;
}

juce::String SearchIndex::parseScope (const juce::String& raw, Scope& scope)
{
    const auto t = raw.trimStart();

    if (t.isEmpty())
        return t;

    switch (t[0])
    {
        case '>': scope = Scope::commands;       break;
        case '?': scope = Scope::help;           break;
        case '#': scope = Scope::content;        break;
        case '@': scope = Scope::places;         break;
        case '=': scope = Scope::parametersOnly; break;
        default:  return t;
    }

    return t.substring (1).trimStart();
}

bool SearchIndex::before (const Result& a, const Result& b) noexcept
{
    if (a.score != b.score)
        return a.score > b.score;

    const int ka = kindRank (a.item->kind), kb = kindRank (b.item->kind);

    if (ka != kb)
        return ka < kb;

    // Title order on the normalised title: case and accents do not reorder
    // ties, and it is a plain code-point compare (no allocation, 11).
    if (const int c = a.item->preparedTitle.text.compare (b.item->preparedTitle.text); c != 0)
        return c < 0;

    return a.item->id < b.item->id;
}

double SearchIndex::recencyBonus (const juce::String& id) const
{
    if (const auto* e = RecentStore::get().find (id))
    {
        const double days = juce::jmax (0.0, (double) (nowMs() - e->lastUsedMs) / 86400000.0);
        return 150.0 * std::pow (0.5, days / 7.0);
    }

    return 0.0;
}

double SearchIndex::frequencyBonus (const juce::String& id) const
{
    if (const auto* e = RecentStore::get().find (id))
        return juce::jmin (100.0, 20.0 * std::log2 (1.0 + (double) e->uses));

    return 0.0;
}

std::vector<SearchIndex::Result> SearchIndex::query (const juce::String& rawQuery, Scope chip, bool includeHidden)
{
    refreshIfNeeded();

    std::vector<Result> results;
    results.reserve (SearchMatcher::kMaxResults + 1);

    Scope prefixScope = Scope::all;
    const auto text = parseScope (rawQuery.substring (0, SearchMatcher::kMaxQueryLength), prefixScope);
    const auto q = SearchMatcher::makeQuery (text);

    if (q.isEmpty())
        return results;

    const double weight = englishWeight ? englishWeight()
                                        : (Localisation::get().getLocale().startsWith ("en") ? 1.0 : 0.8);

    // The recent store, looked up once per query rather than per item.
    const auto now = nowMs();
    std::unordered_map<juce::String, const RecentStore::Entry*, StringHash> recent;

    for (const auto& e : RecentStore::get().getItems())
        recent.emplace (e.id, &e);

    for (const auto* item : flat)
    {
        if (! scopeIncludes (chip, item->kind) || ! scopeIncludes (prefixScope, item->kind))
            continue;

        if (item->hiddenByDefault && ! includeHidden)
            continue;

        const double match = SearchMatcher::scoreItem (q, *item, weight);

        if (match <= 0.0)
            continue;

        Result r;
        r.item = item;
        r.matchScore = match;

        auto found = owner.find (item);
        auto* provider = found != owner.end() ? found->second : nullptr;
        r.availability = provider != nullptr ? provider->availabilityOf (*item) : Availability::available;

        double score = match;

        if (! recent.empty())
            if (auto found = recent.find (item->id); found != recent.end())
            {
                const auto* e = found->second;
                const double days = juce::jmax (0.0, (double) (now - e->lastUsedMs) / 86400000.0);
                score += 150.0 * std::pow (0.5, days / 7.0) + juce::jmin (100.0, 20.0 * std::log2 (1.0 + (double) e->uses));
            }

        if (isVisibleNow && isVisibleNow (*item))
            score += 40.0;

        if (r.availability == Availability::available)
            score += 30.0;
        else if (isLocked (r.availability))
            score -= 100.0;

        r.score = score;

        if (score < SearchMatcher::Score::minimumToShow)
            continue;

        if ((int) results.size() >= SearchMatcher::kMaxResults && score < results.back().score)
            continue;

        // Keep the best 50, in order, without growing past the reservation.
        auto at = std::upper_bound (results.begin(), results.end(), r, before);

        if (at - results.begin() >= SearchMatcher::kMaxResults)
            continue;

        results.insert (at, r);

        if ((int) results.size() > SearchMatcher::kMaxResults)
            results.pop_back();
    }

    return results;
}

SearchIndex::Result SearchIndex::bestMatch (const juce::String& nameQuery, std::initializer_list<ItemKind> kinds)
{
    refreshIfNeeded();

    Result best;
    const auto q = SearchMatcher::makeQuery (nameQuery);

    if (q.isEmpty())
        return best;

    const double weight = englishWeight ? englishWeight()
                                        : (Localisation::get().getLocale().startsWith ("en") ? 1.0 : 0.8);

    for (const auto* item : flat)
    {
        if (item->hiddenByDefault || std::find (kinds.begin(), kinds.end(), item->kind) == kinds.end())
            continue;

        Result r;
        r.item = item;
        r.matchScore = SearchMatcher::scoreItem (q, *item, weight);
        r.score = r.matchScore + recencyBonus (item->id) + frequencyBonus (item->id)
                    + (isVisibleNow && isVisibleNow (*item) ? 40.0 : 0.0);

        if (r.matchScore <= 0.0)
            continue;

        if (auto found = owner.find (item); found != owner.end())
            r.availability = found->second->availabilityOf (*item);

        r.score += r.availability == Availability::available ? 30.0 : isLocked (r.availability) ? -100.0 : 0.0;

        if (best.item == nullptr || before (r, best))
            best = r;
    }

    return best;
}

void SearchIndex::recordActivation (const juce::String& itemId, const juce::String& committedQuery)
{
    auto& store = RecentStore::get();

    if (! store.isEnabled())
        return;

    store.recordItem (itemId, nowMs());

    if (committedQuery.trim().isNotEmpty())
        store.recordQuery (committedQuery);
}

std::vector<const SearchItem*> SearchIndex::getRecentItems (int maxItems)
{
    refreshIfNeeded();

    std::vector<const SearchItem*> recent;
    juce::StringArray stale;
    auto& store = RecentStore::get();

    for (const auto& e : store.getItems())
    {
        if (auto* item = find (e.id))
        {
            if ((int) recent.size() < maxItems)
                recent.push_back (item);
        }
        else
        {
            stale.add (e.id);
        }
    }

    // 4.5: an id that no longer resolves is pruned when read.
    for (const auto& id : stale)
        store.remove (id);

    return recent;
}

juce::String SearchIndex::didYouMean (const juce::String& rawQuery)
{
    refreshIfNeeded();

    // The vocabulary is only needed when a query found nothing, so it is
    // built then, not on every index build (11).
    if (vocabulary.empty())
    {
        for (const auto* item : flat)
            for (size_t w = 0; w < item->preparedTitle.numWords(); ++w)
                if (item->preparedTitle.word (w).size() >= 3)
                    vocabulary.emplace_back (item->preparedTitle.word (w));

        std::sort (vocabulary.begin(), vocabulary.end());
        vocabulary.erase (std::unique (vocabulary.begin(), vocabulary.end()), vocabulary.end());
    }

    Scope unused = Scope::all;
    const auto q = SearchMatcher::makeQuery (parseScope (rawQuery, unused));

    if (q.isEmpty())
        return {};

    juce::StringArray corrected;
    bool changed = false;

    for (const auto& token : q.tokens)
    {
        auto word = SearchMatcher::closestWord (token, vocabulary);

        if (word.isNotEmpty())
        {
            corrected.add (word);
            changed = true;
        }
        else
        {
            corrected.add (SearchMatcher::toString (token));
        }
    }

    return changed ? corrected.joinIntoString (" ") : juce::String();
}

} // namespace luthier::search
