#pragma once

/*  global-search.md 4.1: normalising, tokenising and scoring.

    No JUCE GUI dependency, so it is testable without an editor and reusable by
    HelpContent::findTopic, whose normalisation this is (the same rule for "what
    counts as the same name" everywhere a user types one).

    Scores follow 4.1's table exactly:

        whole title equals query                      1000
        title starts with query                        800
        token is a word-start prefix in the title      600 (+50 in title order)
        acronym of the title words                     500
        synonym exact / prefix                         450 / 400
        breadcrumb word-start                          300
        substring in the title                         250
        typo within Damerau-Levenshtein 1 (>= 4) / 2 (>= 8)   200
        ordered subsequence, -10 per gap, floor 100    100-240

    One addition to the table: a synonym equal to the whole query scores as a
    title word (600). "reverb" is the room's wet/dry by another name, and at
    450 it lost to every preset with "Reverb" in its title, so the control the
    user story promises (1) was not in the top three (GS-11). The addition is
    recorded in docs/coverage/FEAT-SEARCH.md.

    The item score is the mean of its token scores; every token has to match
    (AND). Bonuses for recency, frequency, mode and availability are the
    index's business (SearchIndex), because they need state the matcher does
    not have.

    Everything runs on pre-computed UTF-32 text (SearchItem::Prepared), so a
    query does not allocate per item (11).
*/

#include "SearchItem.h"

namespace luthier::search::SearchMatcher
{
    //==========================================================================
    namespace Score
    {
        constexpr int wholeTitle = 1000;
        constexpr int titlePrefix = 800;
        constexpr int wordStart = 600;
        constexpr int wordOrderBonus = 50;
        constexpr int acronym = 500;
        constexpr int synonymWholeQuery = 600;   ///< a synonym that is the whole query (see below)
        constexpr int synonymExact = 450;
        constexpr int synonymPrefix = 400;
        constexpr int breadcrumb = 300;
        constexpr int substring = 250;
        constexpr int typo = 200;
        constexpr int fuzzyMax = 240;
        constexpr int fuzzyFloor = 100;
        constexpr int fuzzyGapPenalty = 10;

        constexpr int minimumToShow = 150;   ///< 4.1: items below are dropped
    }

    constexpr int kMaxResults = 50;
    constexpr int kMaxQueryLength = 200;   ///< 6.3

    /** Case fold, fold Latin diacritics, '-', '_', '/', '›' (and bracket and
        list punctuation) to spaces, collapse whitespace, trim. */
    juce::String normalise (const juce::String& text);
    std::u32string normaliseToUtf32 (const juce::String& text);

    /** As normalise, with `map[i]` the index in `text` that output character i
        came from - for drawing matched characters in the original title. */
    std::u32string normaliseWithMap (const juce::String& text, std::vector<int>& map);

    /** True for the scripts where every character starts a word (CJK). */
    bool isWordPerCharacter (char32_t c) noexcept;

    /** Splits a normalised string on spaces. */
    std::vector<std::u32string> tokenise (const std::u32string& normalised);

    SearchItem::Prepared prepare (const juce::String& text);

    /** Optimal-string-alignment Damerau-Levenshtein, giving up (returning
        maxDistance + 1) as soon as it cannot be within maxDistance. No heap
        allocation for words up to 48 characters; longer ones return
        maxDistance + 1. */
    int damerauLevenshtein (const std::u32string& a, const std::u32string& b, int maxDistance) noexcept;

    /** The typo budget for a token (4.1): 0 below 4 characters, 1 from 4, 2 from 8. */
    int typoBudget (size_t tokenLength) noexcept;

    //==========================================================================
    struct Query
    {
        std::u32string whole;                 ///< normalised
        std::vector<std::u32string> tokens;

        bool isEmpty() const noexcept { return tokens.empty(); }
    };

    Query makeQuery (const juce::String& raw);

    /** The mean token score, or 0 when a token matches nothing. `englishWeight`
        is 1 in English and 0.8 elsewhere (2: "matches on it score x0.8 in a
        non-English locale"). */
    double scoreItem (const Query& query, const SearchItem& item, double englishWeight = 1.0) noexcept;

    /** Scores one token against prepared text as a title (the table's title
        rows). `previousWord` carries the word order between tokens. */
    int scoreTitleToken (const std::u32string& token, const SearchItem::Prepared& title,
                         int& previousWord) noexcept;

    /** The title characters a query matched, as ranges in `title`'s own
        indices (for the accent-and-bold rendering, 6.2). */
    juce::Array<juce::Range<int>> matchedRanges (const Query& query, const juce::String& title);

    /** The nearest word in `vocabulary` to `word` within a typo budget of 2
        (did-you-mean, 6.2), or empty. */
    juce::String closestWord (const std::u32string& word, const std::vector<std::u32string>& vocabulary);

    juce::String toString (const std::u32string& s);
}
