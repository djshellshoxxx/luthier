#include "ToneDescriptors.h"
#include "../../Support/IrLibrary.h"

namespace luthier
{

const char* const DescriptorCalibration::kMagic = "luthier.calibration";

namespace
{
    enum Spectral { centroid = 0, rolloff, flatness, lowBand, midBand, highBand, crest, attack, tail, width };

    constexpr double kPercentileStep = 5.0;

    /** Confidence that x is below (or above) a threshold: 0.5 at the threshold,
        1 a full `scale` beyond it on the right side, 0 a scale the other way. */
    double below (double x, double threshold, double scale) noexcept
    {
        return 0.5 + 0.5 * juce::jlimit (-1.0, 1.0, (threshold - x) / juce::jmax (1.0e-9, scale));
    }

    double above (double x, double threshold, double scale) noexcept
    {
        return below (-x, -threshold, scale);
    }

    double fromBool (bool b) noexcept { return b ? 1.0 : 0.0; }

    juce::StringArray tokensOf (const juce::String& text)
    {
        juce::StringArray words;
        juce::String current;

        for (auto ch : text.toLowerCase())
        {
            if (juce::CharacterFunctions::isLetterOrDigit (ch))
                current += juce::String::charToString (ch);
            else if (current.isNotEmpty())
            {
                words.add (current);
                current.clear();
            }
        }

        if (current.isNotEmpty())
            words.add (current);

        return words;
    }
}

//==============================================================================
DescriptorCalibration::DescriptorCalibration()
{
    spread.fill (1.0);
}

double DescriptorCalibration::weight (int dim) noexcept
{
    if (dim < ToneFeatures::kNumSpectral)   return 1.0;   // spectral
    if (dim == 10)                          return 1.5;   // drive
    if (dim >= 14 && dim < 18)              return 2.0;   // family one-hot
    return 0.7;
}

std::array<double, DescriptorCalibration::kNumDims> DescriptorCalibration::rawVector (const PresetFeatures& p, const ToneFeatures& t)
{
    std::array<double, kNumDims> v {};
    const auto s = t.spectralVector();

    for (int i = 0; i < ToneFeatures::kNumSpectral; ++i)
        v[(size_t) i] = s[(size_t) i];

    v[10] = p.drive;
    v[11] = p.reverb;
    v[12] = p.delay;
    v[13] = p.compression;

    for (int f = 0; f < PresetFeatures::numFamilies; ++f)
        v[(size_t) (14 + f)] = fromBool (p.family == f);

    for (int k = 0; k < PresetFeatures::numPickups; ++k)
        v[(size_t) (18 + k)] = fromBool (p.pickup == k);

    v[21] = fromBool (p.nylon);
    v[22] = fromBool (p.flat);
    v[23] = fromBool (p.slideOn);
    return v;
}

std::array<double, DescriptorCalibration::kNumDims> DescriptorCalibration::zVector (const PresetFeatures& p, const ToneFeatures& t) const
{
    auto v = rawVector (p, t);

    for (int d = 0; d < kNumDims; ++d)
        v[(size_t) d] = (v[(size_t) d] - mean[(size_t) d]) / juce::jmax (1.0e-6, spread[(size_t) d]);

    return v;
}

DescriptorCalibration DescriptorCalibration::fromCorpus (const std::vector<std::pair<PresetFeatures, ToneFeatures>>& corpus)
{
    DescriptorCalibration c;

    // Percentile tables, per group.
    for (int g = 0; g < numGroups; ++g)
    {
        std::vector<ToneFeatures> members;

        for (const auto& e : corpus)
            if (e.second.valid && ((e.first.family == PresetFeatures::bass) == (g == bass)))
                members.push_back (e.second);

        // A group with no members borrows the whole corpus.
        if (members.empty())
            for (const auto& e : corpus)
                if (e.second.valid)
                    members.push_back (e.second);

        for (int f = 0; f < ToneFeatures::kNumSpectral; ++f)
        {
            std::vector<double> values;

            for (const auto& m : members)
                values.push_back (m.spectralVector()[(size_t) f]);

            std::sort (values.begin(), values.end());

            for (int i = 0; i < kNumPercentiles; ++i)
            {
                double v = 0.0;

                if (! values.empty())
                {
                    const double pos = (i * kPercentileStep) / 100.0 * (double) (values.size() - 1);
                    const size_t lo = (size_t) std::floor (pos);
                    const size_t hi = juce::jmin (values.size() - 1, lo + 1);
                    v = values[lo] + (values[hi] - values[lo]) * (pos - (double) lo);
                }

                c.table[(size_t) g][(size_t) f][(size_t) i] = v;
            }
        }
    }

    // Mean and spread for the similarity vector. The one-hot and flag
    // dimensions stay 0/1 (their spread across 36 presets says nothing useful).
    std::vector<std::array<double, kNumDims>> vectors;

    for (const auto& e : corpus)
        if (e.second.valid)
            vectors.push_back (rawVector (e.first, e.second));

    for (int d = 0; d < kNumDims; ++d)
    {
        if (d >= 14 || vectors.empty())
        {
            c.mean[(size_t) d] = 0.0;
            c.spread[(size_t) d] = 1.0;
            continue;
        }

        double sum = 0.0, squares = 0.0;

        for (const auto& v : vectors)
            sum += v[(size_t) d];

        const double m = sum / (double) vectors.size();

        for (const auto& v : vectors)
            squares += (v[(size_t) d] - m) * (v[(size_t) d] - m);

        c.mean[(size_t) d] = m;
        c.spread[(size_t) d] = juce::jmax (1.0e-3, std::sqrt (squares / (double) vectors.size()));
    }

    c.valid = ! vectors.empty();
    return c;
}

double DescriptorCalibration::percentile (Group g, int feature, double p) const noexcept
{
    const auto& row = table[(size_t) juce::jlimit (0, (int) numGroups - 1, (int) g)]
                           [(size_t) juce::jlimit (0, ToneFeatures::kNumSpectral - 1, feature)];
    const double pos = juce::jlimit (0.0, 100.0, p) / kPercentileStep;
    const int lo = juce::jmin (kNumPercentiles - 1, (int) std::floor (pos));
    const int hi = juce::jmin (kNumPercentiles - 1, lo + 1);
    return row[(size_t) lo] + (row[(size_t) hi] - row[(size_t) lo]) * (pos - lo);
}

juce::var DescriptorCalibration::toVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("schema", 1);
    root->setProperty ("magic", kMagic);

    auto* groups = new juce::DynamicObject();

    for (int g = 0; g < numGroups; ++g)
    {
        auto* group = new juce::DynamicObject();

        for (int f = 0; f < ToneFeatures::kNumSpectral; ++f)
        {
            juce::Array<juce::var> values;

            for (double v : table[(size_t) g][(size_t) f])
                values.add (v);

            group->setProperty (ToneFeatures::spectralName (f), values);
        }

        groups->setProperty (g == guitar ? "guitar" : "bass", juce::var (group));
    }

    root->setProperty ("percentiles", juce::var (groups));

    juce::Array<juce::var> means, spreads;

    for (int d = 0; d < kNumDims; ++d)
    {
        means.add (mean[(size_t) d]);
        spreads.add (spread[(size_t) d]);
    }

    root->setProperty ("mean", means);
    root->setProperty ("spread", spreads);
    return juce::var (root);
}

DescriptorCalibration DescriptorCalibration::fromVar (const juce::var& v)
{
    DescriptorCalibration c;

    if (v.getProperty ("magic", {}).toString() != kMagic)
        return c;

    auto* groups = v.getProperty ("percentiles", {}).getDynamicObject();

    if (groups == nullptr)
        return c;

    for (int g = 0; g < numGroups; ++g)
    {
        const auto group = groups->getProperty (g == guitar ? "guitar" : "bass");

        for (int f = 0; f < ToneFeatures::kNumSpectral; ++f)
            if (auto* values = group.getProperty (ToneFeatures::spectralName (f), {}).getArray())
                for (int i = 0; i < juce::jmin (kNumPercentiles, values->size()); ++i)
                    c.table[(size_t) g][(size_t) f][(size_t) i] = (double) (*values)[i];
    }

    if (auto* means = v.getProperty ("mean", {}).getArray())
        for (int d = 0; d < juce::jmin (kNumDims, means->size()); ++d)
            c.mean[(size_t) d] = (double) (*means)[d];

    if (auto* spreads = v.getProperty ("spread", {}).getArray())
        for (int d = 0; d < juce::jmin (kNumDims, spreads->size()); ++d)
            c.spread[(size_t) d] = juce::jmax (1.0e-6, (double) (*spreads)[d]);

    c.valid = true;
    return c;
}

juce::File DescriptorCalibration::getShippedFile()
{
    const auto resources = IrLibrary::getResourcesFolder();
    return resources == juce::File() ? juce::File()
                                     : resources.getChildFile ("Presets").getChildFile ("descriptor-calibration.json");
}

DescriptorCalibration DescriptorCalibration::fallback()
{
    // Broad guesses for a guitar render at -18 LUFS, used only until the
    // factory bank has been analysed once.
    std::vector<std::pair<PresetFeatures, ToneFeatures>> corpus;

    for (int i = 0; i < 21; ++i)
    {
        const double t = i / 20.0;
        ToneFeatures f;
        f.valid = true;
        f.centroidLog2Hz = 9.5 + 2.5 * t;
        f.rolloffLog2Hz = 10.5 + 3.0 * t;
        f.flatness = 0.02 + 0.3 * t;
        f.lowBand = 0.1 + 0.5 * (1.0 - t);
        f.midBand = 0.3 + 0.3 * t;
        f.highBand = 0.02 + 0.3 * t;
        f.crestDb = 10.0 + 10.0 * t;
        f.attack = 8.0 + 20.0 * t;
        f.tailSeconds = 0.1 + 1.5 * t;
        f.width = 0.4 * t;
        PresetFeatures p;
        corpus.push_back ({ p, f });
        p.family = PresetFeatures::bass;
        corpus.push_back ({ p, f });
    }

    return fromCorpus (corpus);
}

//==============================================================================
const juce::StringArray& ToneDescriptors::vocabulary()
{
    static const juce::StringArray words { "warm", "bright", "clean", "crunchy", "high-gain", "fuzzy", "djent",
                                           "spacious", "dry", "twangy", "percussive", "compressed", "fat",
                                           "jazz", "acoustic", "nylon", "bass", "slide" };
    return words;
}

juce::StringArray ToneDescriptors::synonymsOf (const juce::String& d)
{
    if (d == "warm")        return { "dark", "mellow", "smooth", "round" };
    if (d == "bright")      return { "sparkly", "chimey", "glassy", "crisp" };
    if (d == "crunchy")     return { "crunch", "gritty", "breakup", "edge of breakup" };
    if (d == "high-gain")   return { "heavy", "metal", "chug", "distorted", "high gain", "highgain" };
    if (d == "fuzzy")       return { "fuzz" };
    if (d == "spacious")    return { "ambient", "wet", "washy", "big" };
    if (d == "dry")         return { "tight", "close" };
    if (d == "twangy")      return { "twang", "snappy" };
    if (d == "fat")         return { "thick", "full" };
    return {};
}

juce::String ToneDescriptors::canonicalFor (const juce::String& word)
{
    const auto w = word.trim().toLowerCase();

    for (const auto& d : vocabulary())
        if (w == d || synonymsOf (d).contains (w))
            return d;

    return {};
}

std::vector<Descriptor> ToneDescriptors::evaluate (const PresetFeatures& p, const ToneFeatures& t,
                                                   const DescriptorCalibration& cal, const juce::StringArray& genres)
{
    std::vector<Descriptor> out;
    const auto group = p.family == PresetFeatures::bass ? DescriptorCalibration::bass : DescriptorCalibration::guitar;
    const auto s = t.spectralVector();

    auto pc = [&] (int feature, double percent) { return cal.percentile (group, feature, percent); };

    // A feature's natural scale: half its inter-quartile range across the corpus.
    auto scaleOf = [&] (int feature) { return juce::jmax (1.0e-4, 0.5 * (pc (feature, 75.0) - pc (feature, 25.0))); };

    auto lt = [&] (int feature, double percent) { return below (s[(size_t) feature], pc (feature, percent), scaleOf (feature)); };
    auto gt = [&] (int feature, double percent) { return above (s[(size_t) feature], pc (feature, percent), scaleOf (feature)); };

    const bool spectral = t.valid;
    const double spectralOrNeutral = spectral ? 1.0 : 0.0;   // no audio yet: no spectral descriptor

    const double warm = spectral ? juce::jmin (lt (centroid, 35.0), lt (highBand, 40.0), lt (flatness, 50.0)) : 0.0;
    const double bright = spectral ? juce::jmax (gt (centroid, 65.0), gt (highBand, 70.0)) : 0.0;
    const double clean = juce::jmin (below (p.drive, 0.25, 0.1), spectral ? lt (flatness, 50.0) : 1.0);
    const double crunchy = juce::jmin (above (p.drive, 0.3, 0.08), below (p.drive, 0.6, 0.08));
    const double highGain = above (p.drive, 0.6, 0.08);
    const double fuzzy = juce::jmax (fromBool (p.fuzzPedal),
                                     spectral ? juce::jmin (gt (flatness, 85.0), above (p.drive, 0.5, 0.08)) : 0.0);
    const bool extended = p.numStrings >= 7 || p.lowestOpenMidi <= 35;   // B1 or below
    /*  6.3 says "tail < p30". A noise gate also counts as tight (DECISIONS,
        FEAT-BROWSER): in the physical model a chug's tail is the low string's
        own release, which a gate set under that level does not shorten, so a
        gated extended-range rig - the djent recipe - measured as loose. */
    const double tight = juce::jmax (spectral ? lt (tail, 30.0) : 0.0, fromBool (p.gatePedal));
    const double djent = p.family == PresetFeatures::bass
                           ? 0.0
                           : juce::jmin (highGain, fromBool (extended), tight);
    const double spacious = juce::jmax (spectral ? gt (tail, 75.0) : 0.0,
                                        above (p.reverb, 0.5, 0.1), above (p.delay, 0.35, 0.08));
    const double dry = juce::jmin (spectral ? lt (tail, 25.0) : 0.0, below (p.delay, 0.1, 0.05));
    const double twangy = juce::jmin (fromBool (p.pickup == PresetFeatures::singleCoil && p.family == PresetFeatures::electric),
                                      spectral ? gt (attack, 70.0) : 0.0, bright);
    const double percussive = spectral ? juce::jmin (gt (attack, 80.0), gt (crest, 70.0)) : 0.0;
    const double compressed = juce::jmax (above (p.compression, 0.4, 0.1), spectral ? lt (crest, 20.0) : 0.0);
    const double fat = spectral ? juce::jmin (gt (lowBand, 65.0), gt (midBand, 50.0)) : 0.0;
    const double jazz = juce::jmin (warm, clean, juce::jmax (fromBool (p.hollowOrArchtop), fromBool (p.neckHumbucker),
                                                             fromBool (genres.contains ("jazz"))));

    juce::ignoreUnused (spectralOrNeutral);

    const double values[] = { warm, bright, clean, crunchy, highGain, fuzzy, djent, spacious, dry, twangy,
                              percussive, compressed, fat, jazz,
                              fromBool (p.family == PresetFeatures::acoustic || p.family == PresetFeatures::classical),
                              fromBool (p.nylon), fromBool (p.family == PresetFeatures::bass), fromBool (p.slideOn) };

    const auto& words = vocabulary();

    for (int i = 0; i < words.size(); ++i)
        out.push_back ({ words[i], juce::jlimit (0.0, 1.0, values[i]) });

    return out;
}

juce::StringArray ToneDescriptors::attached (const std::vector<Descriptor>& all)
{
    juce::StringArray out;

    for (const auto& d : all)
        if (d.confidence >= kAttach)
            out.add (d.word);

    return out;
}

const juce::StringArray& ToneDescriptors::genreList()
{
    static const juce::StringArray genres { "rock", "blues", "jazz", "country", "folk", "classical", "metal",
                                            "funk", "reggae", "latin", "indie", "ambient" };
    return genres;
}

juce::StringArray ToneDescriptors::genresFor (const juce::String& name, const juce::String& category,
                                              const juce::StringArray& tags)
{
    const auto words = tokensOf (name + " " + category + " " + tags.joinIntoString (" "));

    static const std::vector<std::pair<const char*, const char*>> aliases {
        { "rock", "rock" }, { "crunch", "rock" }, { "rockabilly", "rock" }, { "surf", "rock" }, { "stoner", "rock" },
        { "shred", "rock" }, { "blues", "blues" }, { "jazz", "jazz" }, { "bebop", "jazz" }, { "fretless", "jazz" },
        { "country", "country" }, { "twang", "country" }, { "bluegrass", "country" }, { "nashville", "country" },
        { "rockabilly", "country" }, { "folk", "folk" }, { "dadgad", "folk" }, { "fingerstyle", "folk" },
        { "classical", "classical" }, { "nylon", "classical" }, { "flamenco", "classical" },
        { "metal", "metal" }, { "djent", "metal" }, { "chug", "metal" },
        { "funk", "funk" }, { "wah", "funk" }, { "slap", "funk" }, { "motown", "funk" },
        { "reggae", "reggae" }, { "dub", "reggae" }, { "latin", "latin" }, { "bossa", "latin" },
        { "flamenco", "latin" }, { "indie", "indie" }, { "jangle", "indie" }, { "ambient", "ambient" },
        { "swell", "ambient" }, { "shoegaze", "ambient" }
    };

    juce::StringArray out;

    for (const auto& a : aliases)
        if (words.contains (a.first))
            out.addIfNotAlreadyThere (a.second);

    return out;
}

} // namespace luthier
