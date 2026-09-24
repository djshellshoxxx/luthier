#include "PresetSearch.h"
#include "../PresetLibraryPrefs.h"
#include "../../Accessibility/Localisation.h"

namespace luthier
{

namespace
{
    constexpr double kName = 5.0, kTags = 4.0, kDescriptor = 3.0, kCategory = 3.0, kGear = 2.0,
                     kAuthor = 1.5, kDescription = 1.0;

    /** How well a single-word token matches a word: 1.05 exact, 1 prefix,
        0.8 within one Damerau edit (tokens of 5 or more), else 0. */
    double wordMatch (const juce::String& token, const juce::String& word)
    {
        if (word.startsWith (token))
            return word.length() == token.length() ? 1.05 : 1.0;

        const int n = token.length();

        if (n >= 5)
        {
            if (PresetSearch::damerau (token, word, 1) <= 1)
                return 0.8;

            for (int len = n - 1; len <= n + 1; ++len)
                if (len < word.length() && PresetSearch::damerau (token, word.substring (0, len), 1) <= 1)
                    return 0.8;
        }

        return 0.0;
    }

    double fieldMatch (const juce::String& token, const juce::StringArray& words)
    {
        if (token.containsChar (' '))
            return words.joinIntoString (" ").contains (token) ? 1.0 : 0.0;

        double best = 0.0;

        for (const auto& w : words)
        {
            best = juce::jmax (best, wordMatch (token, w));

            if (best >= 1.05)
                break;
        }

        return best;
    }

    /** The descriptor a token names: exactly, by synonym, by prefix, by one typo,
        or by its localised word (10). */
    juce::String descriptorForToken (const juce::String& token)
    {
        auto exact = ToneDescriptors::canonicalFor (token);

        if (exact.isNotEmpty())
            return exact;

        auto& loc = Localisation::get();

        for (const auto& d : ToneDescriptors::vocabulary())
        {
            const auto key = "descriptor." + d;

            if (loc.hasKey (key) && loc.translate (key).toLowerCase() == token)
                return d;
        }

        if (token.length() >= 3)
            for (const auto& d : ToneDescriptors::vocabulary())
            {
                if (d.startsWith (token))
                    return d;

                for (const auto& s : ToneDescriptors::synonymsOf (d))
                    if (s.startsWith (token))
                        return d;
            }

        if (token.length() >= 5)
            for (const auto& d : ToneDescriptors::vocabulary())
            {
                if (PresetSearch::damerau (token, d, 1) <= 1)
                    return d;

                for (const auto& s : ToneDescriptors::synonymsOf (d))
                    if (PresetSearch::damerau (token, s, 1) <= 1)
                        return d;
            }

        return {};
    }
}

//==============================================================================
int PresetSearch::damerau (const juce::String& a, const juce::String& b, int limit)
{
    const int n = a.length(), m = b.length();

    if (std::abs (n - m) > limit)
        return limit + 1;

    // Three rows on the stack are enough for the transposition term; words
    // longer than 63 characters are compared on their first 63.
    if (n > 63 || m > 63)
        return damerau (a.substring (0, 63), b.substring (0, 63), limit);

    std::array<int, 64> prev2 {}, prev {}, cur {};
    std::array<juce::juce_wchar, 64> ca {}, cb {};

    {
        auto pa = a.getCharPointer();
        for (int i = 0; i < n; ++i) ca[(size_t) i] = pa.getAndAdvance();
        auto pb = b.getCharPointer();
        for (int j = 0; j < m; ++j) cb[(size_t) j] = pb.getAndAdvance();
    }

    for (int j = 0; j <= m; ++j)
        prev[(size_t) j] = j;

    for (int i = 1; i <= n; ++i)
    {
        cur[0] = i;
        int rowMin = cur[0];

        for (int j = 1; j <= m; ++j)
        {
            const int cost = ca[(size_t) i - 1] == cb[(size_t) j - 1] ? 0 : 1;
            int v = juce::jmin (prev[(size_t) j] + 1, cur[(size_t) j - 1] + 1, prev[(size_t) j - 1] + cost);

            if (i > 1 && j > 1 && ca[(size_t) i - 1] == cb[(size_t) j - 2] && ca[(size_t) i - 2] == cb[(size_t) j - 1])
                v = juce::jmin (v, prev2[(size_t) j - 2] + 1);

            cur[(size_t) j] = v;
            rowMin = juce::jmin (rowMin, v);
        }

        if (rowMin > limit)
            return limit + 1;

        prev2 = prev;
        prev = cur;
    }

    return prev[(size_t) m];
}

juce::StringArray PresetSearch::tokenise (const juce::String& query)
{
    juce::StringArray tokens;
    auto text = query.toLowerCase();

    // Quoted phrases first.
    while (text.containsChar ('"'))
    {
        const int open = text.indexOfChar ('"');
        const int close = text.indexOfChar (open + 1, '"');

        if (close < 0)
        {
            text = text.replaceCharacter ('"', ' ');
            break;
        }

        const auto phrase = text.substring (open + 1, close).trim();

        if (phrase.isNotEmpty())
            tokens.add (PresetIndex::wordsOf (phrase).joinIntoString (" "));

        text = text.substring (0, open) + " " + text.substring (close + 1);
    }

    // Then the multi-word vocabulary ("edge of breakup").
    auto normalised = " " + PresetIndex::wordsOf (text).joinIntoString (" ") + " ";

    for (const auto& d : ToneDescriptors::vocabulary())
        for (const auto& s : ToneDescriptors::synonymsOf (d))
            if (s.containsChar (' ') && normalised.contains (" " + s + " "))
            {
                tokens.add (s);
                normalised = normalised.replace (" " + s + " ", " ");
            }

    for (const auto& w : PresetIndex::wordsOf (normalised))
        tokens.add (w);

    return tokens;
}

double PresetSearch::scoreToken (const juce::String& token, const PresetIndex::Entry& e)
{
    const auto descriptor = descriptorForToken (token);
    const bool vocabulary = descriptor.isNotEmpty();

    double best = 0.0;

    best = juce::jmax (best, kName * fieldMatch (token, e.nameWords));
    best = juce::jmax (best, kTags * fieldMatch (token, e.tagWords));
    best = juce::jmax (best, kCategory * fieldMatch (token, e.categoryWords));
    best = juce::jmax (best, kGear * fieldMatch (token, e.gearWords));

    if (vocabulary)
    {
        // A sound word is matched against what the preset sounds like, not
        // against its prose: "clean" must not find a metal preset whose
        // description happens to mention a clean channel.
        for (const auto& d : e.descriptorConfidences)
            if (d.word == descriptor && d.confidence >= ToneDescriptors::kAttach)
                best = juce::jmax (best, kDescriptor * d.confidence);
    }
    else
    {
        best = juce::jmax (best, kAuthor * fieldMatch (token, e.authorWords));
        best = juce::jmax (best, kDescription * fieldMatch (token, e.descriptionWords));
    }

    return best;
}

//==============================================================================
bool PresetSearch::Filters::isEmpty() const noexcept
{
    bool anyTechnique = false;

    for (bool t : techniques)
        anyTechnique = anyTechnique || t;

    return family < 0 && families.isEmpty() && genres.isEmpty() && tones.isEmpty() && ! usesTechniques
           && ! anyTechnique && minRating == 0 && ! favourites && ! recent
           && sources[0] && sources[1] && sources[2] && sources[3];
}

juce::var PresetSearch::Filters::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("family", family);
    o->setProperty ("families", families.joinIntoString (","));
    o->setProperty ("genres", genres.joinIntoString (","));
    o->setProperty ("tones", tones.joinIntoString (","));
    o->setProperty ("usesTechniques", usesTechniques);

    juce::String t;

    for (bool b : techniques)
        t << (b ? "1" : "0");

    o->setProperty ("techniques", t);
    o->setProperty ("minRating", minRating);
    o->setProperty ("favourites", favourites);
    o->setProperty ("recent", recent);

    juce::String s;

    for (bool b : sources)
        s << (b ? "1" : "0");

    o->setProperty ("sources", s);
    return juce::var (o);
}

PresetSearch::Filters PresetSearch::Filters::fromVar (const juce::var& v)
{
    Filters f;

    if (v.getDynamicObject() == nullptr)
        return f;

    auto list = [] (const juce::var& x)
    {
        juce::StringArray a;
        a.addTokens (x.toString(), ",", "");
        a.removeEmptyStrings();
        return a;
    };

    f.family = juce::jlimit (-1, (int) PresetFeatures::numFamilies - 1, (int) v.getProperty ("family", -1));
    f.families = list (v.getProperty ("families", {}));
    f.genres = list (v.getProperty ("genres", {}));
    f.tones = list (v.getProperty ("tones", {}));
    f.usesTechniques = v.getProperty ("usesTechniques", false);
    f.minRating = juce::jlimit (0, 5, (int) v.getProperty ("minRating", 0));
    f.favourites = v.getProperty ("favourites", false);
    f.recent = v.getProperty ("recent", false);

    const auto t = v.getProperty ("techniques", {}).toString();

    for (int i = 0; i < juce::jmin ((int) f.techniques.size(), t.length()); ++i)
        f.techniques[(size_t) i] = t[i] == '1';

    const auto s = v.getProperty ("sources", {}).toString();

    for (int i = 0; i < juce::jmin (4, s.length()); ++i)
        f.sources[(size_t) i] = s[i] == '1';

    return f;
}

bool PresetSearch::passes (const PresetIndex::Entry& e, const Filters& f, const PresetLibraryPrefs& prefs)
{
    if (! f.sources[(size_t) e.source])
        return false;

    if (f.family >= 0 && (! e.parsed || e.params.family != f.family))
        return false;

    if (! f.families.isEmpty() && ! f.families.contains (PresetFeatures::getFamilyName (e.params.family), true))
        return false;

    if (! f.genres.isEmpty())
    {
        bool any = false;

        for (const auto& g : f.genres)
            any = any || e.genres.contains (g, true);

        if (! any)
            return false;
    }

    if (! f.tones.isEmpty())
    {
        bool any = false;

        for (const auto& t : f.tones)
            any = any || e.descriptors.contains (ToneDescriptors::canonicalFor (t).isNotEmpty()
                                                   ? ToneDescriptors::canonicalFor (t) : t);

        if (! any)
            return false;
    }

    bool specific = false, anySpecific = false;

    for (int t = 0; t < PresetFeatures::numTechniques; ++t)
        if (f.techniques[(size_t) t])
        {
            specific = true;
            anySpecific = anySpecific || e.params.techniques[(size_t) t];
        }

    if (specific && ! anySpecific)
        return false;

    if (! specific && f.usesTechniques && ! e.params.usesAnyTechnique())
        return false;

    if (f.minRating > 0 && prefs.getRating (e.key) < f.minRating)
        return false;

    if (f.favourites && ! prefs.isFavourite (e.key))
        return false;

    if (f.recent && ! prefs.getRecent().contains (e.key))
        return false;

    return true;
}

std::vector<PresetSearch::Result> PresetSearch::run (const PresetIndex& index, const juce::String& query,
                                                     const Filters& filters, Sort sort, const PresetLibraryPrefs& prefs)
{
    std::vector<Result> results;
    const auto tokens = tokenise (query);
    results.reserve ((size_t) index.size());

    for (int i = 0; i < index.size(); ++i)
    {
        const auto& e = index[i];

        if (! passes (e, filters, prefs))
            continue;

        double score = 0.0;
        bool all = true;

        for (const auto& t : tokens)
        {
            const double s = scoreToken (t, e);

            if (s <= 0.0)
            {
                all = false;
                break;
            }

            score += s;
        }

        if (all)
            results.push_back ({ i, score });
    }

    auto byName = [&] (const Result& a, const Result& b)
    {
        const int c = index[a.entry].info.name.compareIgnoreCase (index[b.entry].info.name);
        return c != 0 ? c < 0 : index[a.entry].key < index[b.entry].key;
    };

    if (sort == Sort::relevance && tokens.isEmpty())
        sort = Sort::name;

    std::stable_sort (results.begin(), results.end(), [&] (const Result& a, const Result& b)
    {
        const auto& ea = index[a.entry];
        const auto& eb = index[b.entry];

        switch (sort)
        {
            case Sort::relevance:
                if (a.score != b.score) return a.score > b.score;
                break;
            case Sort::category:
                if (ea.info.category != eb.info.category) return ea.info.category.compareIgnoreCase (eb.info.category) < 0;
                break;
            case Sort::rating:
                if (prefs.getRating (ea.key) != prefs.getRating (eb.key)) return prefs.getRating (ea.key) > prefs.getRating (eb.key);
                break;
            case Sort::recentlyLoaded:
            {
                const auto ta = prefs.getLastLoaded (ea.key), tb = prefs.getLastLoaded (eb.key);
                if (ta != tb) return ta > tb;
                break;
            }
            case Sort::mostLoaded:
                if (prefs.getLoadCount (ea.key) != prefs.getLoadCount (eb.key)) return prefs.getLoadCount (ea.key) > prefs.getLoadCount (eb.key);
                break;
            case Sort::dateModified:
                if (ea.info.modified != eb.info.modified) return ea.info.modified > eb.info.modified;
                break;
            case Sort::name:
            case Sort::similarity:
            default:
                break;
        }

        return byName (a, b);
    });

    return results;
}

//==============================================================================
double PresetSearch::distance (const PresetIndex::Entry& a, const PresetIndex::Entry& b)
{
    double sum = 0.0;

    for (int d = 0; d < DescriptorCalibration::kNumDims; ++d)
    {
        const double diff = a.vector[(size_t) d] - b.vector[(size_t) d];
        sum += DescriptorCalibration::weight (d) * diff * diff;
    }

    return std::sqrt (sum);
}

std::vector<PresetSearch::Result> PresetSearch::similar (const PresetIndex& index, int entry,
                                                         const Filters& sourceFilter, int count)
{
    std::vector<Result> out;

    if (! juce::isPositiveAndBelow (entry, index.size()) || ! index[entry].isAnalysed())
        return out;

    const auto& self = index[entry];

    for (int i = 0; i < index.size(); ++i)
    {
        const auto& e = index[i];

        if (i == entry || ! e.isAnalysed() || e.corrupt)
            continue;

        if (e.soundHash == self.soundHash || ! sourceFilter.sources[(size_t) e.source])
            continue;

        out.push_back ({ i, distance (self, e) });
    }

    std::sort (out.begin(), out.end(), [&] (const Result& a, const Result& b)
    {
        if (a.score != b.score)
            return a.score < b.score;

        return index[a.entry].key < index[b.entry].key;
    });

    if ((int) out.size() > count)
        out.resize ((size_t) count);

    return out;
}

const char* PresetSearch::sortName (Sort s) noexcept
{
    switch (s)
    {
        case Sort::relevance:      return "Relevance";
        case Sort::name:           return "Name";
        case Sort::category:       return "Category";
        case Sort::rating:         return "Rating";
        case Sort::recentlyLoaded: return "Recently loaded";
        case Sort::mostLoaded:     return "Most loaded";
        case Sort::dateModified:   return "Date modified";
        case Sort::similarity:     return "Similarity";
        default:                   return "Name";
    }
}

} // namespace luthier
