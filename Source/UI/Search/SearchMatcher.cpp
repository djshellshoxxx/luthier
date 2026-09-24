#include "SearchMatcher.h"

#include <algorithm>

namespace luthier::search
{

const char* getKindName (ItemKind kind) noexcept
{
    switch (kind)
    {
        case ItemKind::parameter:    return "Parameter";
        case ItemKind::choiceOption: return "Option";
        case ItemKind::place:        return "Place";
        case ItemKind::command:      return "Command";
        case ItemKind::preset:       return "Preset";
        case ItemKind::guitar:       return "Guitar";
        case ItemKind::part:         return "Part";
        case ItemKind::pedal:        return "Pedal";
        case ItemKind::snapshot:     return "Snapshot";
        case ItemKind::help:         return "Help";
        case ItemKind::shortcut:     return "Shortcut";
        case ItemKind::setting:      return "Setting";
        case ItemKind::provider:     return "Item";
        default: break;
    }

    return "Item";
}

namespace SearchMatcher
{
namespace
{
    /*  Latin Extended-A, U+0100 to U+017F, one base letter per code point.
        '*' marks the two that fold to two letters (U+0132 IJ, U+0152 OE). */
    constexpr const char* kLatinExtendedA =
        "aaaaaa" "cccccccc" "dddd" "eeeeeeeeee" "gggggggg" "hhhh" "iiiiiiiiii" "**" "jj" "kkk"
        "llllllllll" "nnnnnnnnn" "oooooo" "**" "rrrrrr" "ssssssss" "tttttt" "uuuuuuuuuuuu" "ww" "yyy"
        "zzzzzz" "s";

    /** Appends the folded form of one code point. */
    template <typename Emit>
    void fold (juce::juce_wchar c, Emit&& emit)
    {
        // Separators: 4.1's '-', '_', '/', '›', plus bracket and list punctuation
        // so "Delay time (Post 3)" has a word "post".
        switch (c)
        {
            case '-': case '_': case '/': case 0x203a: case 0x2039: case 0xbb: case 0xab:
            case '(': case ')': case '[': case ']': case '{': case '}': case ',': case ';':
            case ':': case '!': case '?': case '"': case '\'': case '|': case 0x2026: case 0xb7:
            case 0x2013: case 0x2014:
                emit (U' ');
                return;
            default: break;
        }

        if (juce::CharacterFunctions::isWhitespace (c))
        {
            emit (U' ');
            return;
        }

        if (c >= 0xc0 && c <= 0xff)
        {
            static constexpr const char* latin1 =
                // C0-DF
                "aaaaaa*ceeeeiiiidnooooo*ouuuuy**"
                // E0-FF
                "aaaaaa*ceeeeiiiidnooooo*ouuuuy*y";

            const char base = latin1[c - 0xc0];

            if (base != '*')
            {
                emit ((char32_t) base);
                return;
            }

            switch (c)
            {
                case 0xc6: case 0xe6: emit (U'a'); emit (U'e'); return;   // Æ æ
                case 0xd7: case 0xf7: emit (U' '); return;                // × ÷
                case 0xd8: case 0xf8: emit (U'o'); return;                // Ø ø
                case 0xde: case 0xfe: emit (U't'); emit (U'h'); return;   // Þ þ
                case 0xdf: emit (U's'); emit (U's'); return;              // ß
                default: break;
            }
        }

        if (c >= 0x100 && c <= 0x17f)
        {
            const char base = kLatinExtendedA[c - 0x100];

            if (base != '*')
            {
                emit ((char32_t) base);
                return;
            }

            if (c == 0x132 || c == 0x133) { emit (U'i'); emit (U'j'); return; }
            emit (U'o'); emit (U'e'); return;   // Œ œ
        }

        emit ((char32_t) juce::CharacterFunctions::toLowerCase (c));
    }

    bool startsWith (const std::u32string& s, const std::u32string& prefix) noexcept
    {
        return s.size() >= prefix.size() && std::equal (prefix.begin(), prefix.end(), s.begin());
    }

    /** Leftmost ordered subsequence; the number of breaks, or -1. */
    int subsequenceGaps (const std::u32string& token, const std::u32string& text) noexcept
    {
        size_t at = 0;
        int gaps = 0;
        size_t last = std::u32string::npos;

        for (auto c : token)
        {
            const auto found = text.find (c, at);

            if (found == std::u32string::npos)
                return -1;

            if (last != std::u32string::npos && found != last + 1)
                ++gaps;

            last = found;
            at = found + 1;
        }

        return gaps;
    }

    int scoreAgainstWords (const std::u32string& token, const std::vector<std::u32string>& words,
                           int exact, int prefix) noexcept
    {
        int best = 0;

        for (const auto& w : words)
        {
            if (w == token)                         return exact;
            if (prefix > best && startsWith (w, token)) best = prefix;
        }

        return best;
    }
}

//==============================================================================
bool isWordPerCharacter (char32_t c) noexcept
{
    return (c >= 0x3040 && c <= 0x30ff)     // kana
        || (c >= 0x3400 && c <= 0x4dbf)     // CJK extension A
        || (c >= 0x4e00 && c <= 0x9fff)     // CJK unified
        || (c >= 0xac00 && c <= 0xd7af)     // Hangul syllables
        || (c >= 0xf900 && c <= 0xfaff);    // CJK compatibility
}

std::u32string normaliseWithMap (const juce::String& text, std::vector<int>& map)
{
    std::u32string out;
    out.reserve ((size_t) text.length());
    map.clear();

    int index = 0;

    for (auto p = text.getCharPointer(); ! p.isEmpty(); ++index)
    {
        const auto c = p.getAndAdvance();

        fold (c, [&] (char32_t folded)
        {
            if (folded == U' ')
            {
                if (out.empty() || out.back() == U' ')
                    return;
            }

            out.push_back (folded);
            map.push_back (index);
        });
    }

    while (! out.empty() && out.back() == U' ')
    {
        out.pop_back();
        map.pop_back();
    }

    return out;
}

std::u32string normaliseToUtf32 (const juce::String& text)
{
    std::u32string out;
    out.reserve ((size_t) text.length());

    for (auto p = text.getCharPointer(); ! p.isEmpty();)
    {
        fold (p.getAndAdvance(), [&out] (char32_t folded)
        {
            if (folded == U' ' && (out.empty() || out.back() == U' '))
                return;

            out.push_back (folded);
        });
    }

    while (! out.empty() && out.back() == U' ')
        out.pop_back();

    return out;
}

juce::String toString (const std::u32string& s)
{
    juce::String result;
    result.preallocateBytes (s.size() * 2);

    for (auto c : s)
        result += (juce::juce_wchar) c;

    return result;
}

juce::String normalise (const juce::String& text)
{
    return toString (normaliseToUtf32 (text));
}

std::vector<std::u32string> tokenise (const std::u32string& normalised)
{
    std::vector<std::u32string> tokens;
    std::u32string current;

    for (auto c : normalised)
    {
        if (c == U' ')
        {
            if (! current.empty())
                tokens.push_back (std::move (current));

            current.clear();
        }
        else
        {
            current.push_back (c);
        }
    }

    if (! current.empty())
        tokens.push_back (std::move (current));

    return tokens;
}

SearchItem::Prepared prepare (const juce::String& text)
{
    SearchItem::Prepared p;
    p.text = normaliseToUtf32 (text);

    bool atStart = true;
    size_t wordStart = 0;

    for (size_t i = 0; i <= p.text.size(); ++i)
    {
        if (i == p.text.size() || p.text[i] == U' ')
        {
            if (! atStart)
                p.words.push_back (p.text.substr (wordStart, i - wordStart));

            atStart = true;
            continue;
        }

        const auto c = p.text[i];

        if (atStart)
            wordStart = i;

        // 4.1: in CJK scripts every character counts as a word start.
        if (atStart || isWordPerCharacter (c))
        {
            p.wordStarts.push_back ((int) i);
            p.acronym.push_back (c);
        }

        atStart = false;
    }

    // A CJK run is one "word" to the loop above, so its characters are added
    // as words of their own too: a token can then prefix-match from any of them.
    for (size_t w = 0, n = p.words.size(); w < n; ++w)
        if (! p.words[w].empty() && isWordPerCharacter (p.words[w].front()) && p.words[w].size() > 1)
            for (size_t i = 1; i < p.words[w].size(); ++i)
                p.words.push_back (p.words[w].substr (i));

    return p;
}

int typoBudget (size_t tokenLength) noexcept
{
    return tokenLength >= 8 ? 2 : tokenLength >= 4 ? 1 : 0;
}

int damerauLevenshtein (const std::u32string& a, const std::u32string& b, int maxDistance) noexcept
{
    constexpr size_t kMax = 48;
    const int fail = maxDistance + 1;

    if (a.size() > kMax || b.size() > kMax)
        return fail;

    const int n = (int) a.size(), m = (int) b.size();

    if (std::abs (n - m) > maxDistance)
        return fail;

    int d[kMax + 1][kMax + 1];

    for (int i = 0; i <= n; ++i) d[i][0] = i;
    for (int j = 0; j <= m; ++j) d[0][j] = j;

    for (int i = 1; i <= n; ++i)
    {
        int rowBest = fail;

        for (int j = 1; j <= m; ++j)
        {
            const int cost = a[(size_t) i - 1] == b[(size_t) j - 1] ? 0 : 1;
            int v = std::min ({ d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + cost });

            if (i > 1 && j > 1 && a[(size_t) i - 1] == b[(size_t) j - 2] && a[(size_t) i - 2] == b[(size_t) j - 1])
                v = std::min (v, d[i - 2][j - 2] + 1);

            d[i][j] = v;
            rowBest = std::min (rowBest, v);
        }

        if (rowBest > maxDistance)
            return fail;
    }

    return std::min (d[n][m], fail);
}

//==============================================================================
Query makeQuery (const juce::String& raw)
{
    Query q;
    q.whole = normaliseToUtf32 (raw.substring (0, kMaxQueryLength));
    q.tokens = tokenise (q.whole);
    return q;
}

int scoreTitleToken (const std::u32string& token, const SearchItem::Prepared& title, int& previousWord) noexcept
{
    int best = 0;

    // Word-start prefix, with the order bonus from the second token on.
    for (size_t w = 0; w < title.words.size(); ++w)
    {
        if (startsWith (title.words[w], token))
        {
            const bool inOrder = previousWord >= 0 && (int) w > previousWord;
            best = Score::wordStart + (inOrder ? Score::wordOrderBonus : 0);
            previousWord = (int) w;
            break;
        }
    }

    if (best >= Score::wordStart)
        return best;

    if (token.size() >= 2 && startsWith (title.acronym, token))
        return Score::acronym;

    if (token.size() >= 2 && title.text.find (token) != std::u32string::npos)
        return Score::substring;

    if (const int budget = typoBudget (token.size()); budget > 0)
        for (const auto& w : title.words)
            if (damerauLevenshtein (token, w, budget) <= budget)
                return Score::typo;

    if (token.size() >= 3)
    {
        const int gaps = subsequenceGaps (token, title.text);

        if (gaps >= 0)
            return std::max (Score::fuzzyFloor, Score::fuzzyMax - Score::fuzzyGapPenalty * gaps);
    }

    return best;
}

double scoreItem (const Query& query, const SearchItem& item, double englishWeight) noexcept
{
    if (query.isEmpty())
        return 0.0;

    const auto& title = item.preparedTitle;
    const auto& english = item.preparedEnglish;
    const bool hasEnglish = ! english.text.empty() && english.text != title.text;

    const int wholeFloor = title.text == query.whole ? Score::wholeTitle
                         : startsWith (title.text, query.whole) ? Score::titlePrefix : 0;

    const int englishFloor = ! hasEnglish ? 0
                           : english.text == query.whole ? Score::wholeTitle
                           : startsWith (english.text, query.whole) ? Score::titlePrefix : 0;

    int previousTitleWord = -1, previousEnglishWord = -1;
    double total = 0.0;

    for (const auto& token : query.tokens)
    {
        double best = std::max (wholeFloor, scoreTitleToken (token, title, previousTitleWord));

        if (hasEnglish)
            best = std::max (best, englishWeight * std::max (englishFloor, scoreTitleToken (token, english, previousEnglishWord)));

        if (best < Score::synonymWholeQuery)
        {
            for (const auto& syn : item.preparedSynonyms)
            {
                if (syn.text == query.whole)
                {
                    best = std::max (best, (double) Score::synonymWholeQuery);
                    break;
                }

                best = std::max (best, (double) scoreAgainstWords (token, syn.words, Score::synonymExact, Score::synonymPrefix));
            }

            if (best < Score::synonymExact)
                for (const auto& keyword : item.preparedKeywords)
                    best = std::max (best, (double) scoreAgainstWords (token, keyword.words, Score::synonymExact, Score::synonymPrefix));
        }

        if (best < Score::breadcrumb)
            for (const auto& w : item.breadcrumbWords)
                if (startsWith (w, token))
                {
                    best = Score::breadcrumb;
                    break;
                }

        if (best <= 0.0)
            return 0.0;

        total += best;
    }

    return total / (double) query.tokens.size();
}

//==============================================================================
juce::Array<juce::Range<int>> matchedRanges (const Query& query, const juce::String& title)
{
    std::vector<int> map;
    const auto text = normaliseWithMap (title, map);

    std::vector<bool> hit (text.size(), false);

    auto mark = [&] (size_t from, size_t count)
    {
        for (size_t i = from; i < from + count && i < hit.size(); ++i)
            hit[i] = true;
    };

    for (const auto& token : query.tokens)
    {
        bool done = false;

        // A word start first, then anywhere, then as a subsequence.
        for (size_t i = 0; i < text.size() && ! done; ++i)
            if ((i == 0 || text[i - 1] == U' ' || isWordPerCharacter (text[i]))
                  && text.compare (i, token.size(), token) == 0)
            {
                mark (i, token.size());
                done = true;
            }

        if (! done)
            if (const auto at = text.find (token); at != std::u32string::npos)
            {
                mark (at, token.size());
                done = true;
            }

        if (! done && token.size() >= 3 && subsequenceGaps (token, text) >= 0)
        {
            size_t at = 0;

            for (auto c : token)
            {
                const auto found = text.find (c, at);
                mark (found, 1);
                at = found + 1;
            }
        }
    }

    juce::Array<juce::Range<int>> ranges;

    for (size_t i = 0; i < hit.size(); ++i)
    {
        if (! hit[i])
            continue;

        const int start = map[i];
        const int end = map[i] + 1;

        if (! ranges.isEmpty() && ranges.getReference (ranges.size() - 1).getEnd() >= start)
            ranges.getReference (ranges.size() - 1).setEnd (juce::jmax (end, ranges.getLast().getEnd()));
        else
            ranges.add ({ start, end });
    }

    return ranges;
}

juce::String closestWord (const std::u32string& word, const std::vector<std::u32string>& vocabulary)
{
    const int budget = juce::jmax (1, typoBudget (word.size()) + 1);
    int bestDistance = budget + 1;
    const std::u32string* best = nullptr;

    for (const auto& candidate : vocabulary)
    {
        if (candidate == word)
            return {};

        const int d = damerauLevenshtein (word, candidate, budget);

        if (d < bestDistance || (d == bestDistance && best != nullptr && candidate < *best))
        {
            bestDistance = d;
            best = &candidate;
        }
    }

    return best != nullptr && bestDistance <= budget ? toString (*best) : juce::String();
}

} // namespace SearchMatcher
} // namespace luthier::search
