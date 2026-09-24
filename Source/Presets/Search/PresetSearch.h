#pragma once

/*  preset-browser-previews.md 6.4 and 6.5: the query, the filters, the sort
    orders and "Sounds like this", over the in-memory PresetIndex.

    Message thread, no allocation beyond the result vector, no debounce: it
    runs on every keystroke (budget: 16 ms at 5,000 presets).

    FEAT-SEARCH (global-search.md) keeps its own matcher in the UI layer; this
    one lives beside the index so luthier-render and the tests can run it
    without an editor. The rules are the same family (prefix, then one
    Damerau edit for longer tokens).
*/

#include "PresetIndex.h"

namespace luthier
{

class PresetLibraryPrefs;

class PresetSearch
{
public:
    enum class Sort { relevance, name, category, rating, recentlyLoaded, mostLoaded, dateModified, similarity };

    struct Filters
    {
        int family = -1;                    ///< PresetFeatures::Family, or -1 for all
        juce::StringArray families;         ///< Advanced sidebar: OR within the group
        juce::StringArray genres;           ///< OR within the group
        juce::StringArray tones;            ///< descriptor chips, OR within the group
        bool usesTechniques = false;        ///< "Uses Techniques": any armed technique
        std::array<bool, PresetFeatures::numTechniques> techniques {};   ///< specific ones, OR
        int minRating = 0;
        bool favourites = false;
        bool recent = false;
        std::array<bool, 4> sources { true, true, true, true };          ///< PresetIndex::Source

        bool isEmpty() const noexcept;
        juce::var toVar() const;
        static Filters fromVar (const juce::var&);
    };

    struct Result
    {
        int entry = -1;
        double score = 0.0;
    };

    /** Every entry that passes the filters and matches every token. */
    static std::vector<Result> run (const PresetIndex&, const juce::String& query, const Filters&,
                                    Sort, const PresetLibraryPrefs&);

    /** Whether an entry passes the filters (AND across groups, OR within). */
    static bool passes (const PresetIndex::Entry&, const Filters&, const PresetLibraryPrefs&);

    /** 6.4: the tokens of a query, multi-word vocabulary and quoted phrases first. */
    static juce::StringArray tokenise (const juce::String& query);

    /** A query token, resolved once per query rather than once per entry. */
    struct Token
    {
        juce::String text;
        juce::String descriptor;          ///< the vocabulary word it names, or empty
        juce::StringArray matchWords;     ///< what the text fields are searched for
        bool phrase = false;
    };

    static Token prepare (const juce::String& token);

    /** The score of one token against one entry; 0 when it matches nothing. */
    static double scoreToken (const Token& token, const PresetIndex::Entry&);
    static double scoreToken (const juce::String& token, const PresetIndex::Entry& e) { return scoreToken (prepare (token), e); }

    /** Damerau-Levenshtein distance, stopping early above `limit`. `bLength`
        compares only the first so many characters of `b` (a prefix). */
    static int damerau (const juce::String& a, const juce::String& b, int limit = 2, int bLength = -1);

    /** 6.5: the 8 nearest by weighted Euclidean distance. Leaves out the entry
        itself, entries with its sound hash, anything the Source filter
        excludes and anything not yet analysed. */
    static std::vector<Result> similar (const PresetIndex&, int entry, const Filters& sourceFilter, int count = 8);

    static double distance (const PresetIndex::Entry&, const PresetIndex::Entry&);

    static const char* sortName (Sort) noexcept;
};

} // namespace luthier
