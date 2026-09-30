#pragma once

/*  preset-browser-previews.md 6.3 and 6.5: auto-descriptors, genres and the
    calibration that makes "warm" mean "warmer than most factory presets".

    Percentiles (pNN) come from the factory corpus, separately for guitar and
    bass, and ship as descriptor-calibration.json. The same file carries the
    mean and spread of each similarity dimension, so 6.5's vector is z-scored
    against the factory bank rather than against whatever happens to be loaded.

    Descriptors are derived data: never written into a preset file (5.4).
*/

#include "PresetFeatures.h"
#include "../Preview/ToneFeatures.h"

namespace luthier
{

//==============================================================================
class DescriptorCalibration
{
public:
    static constexpr int kNumDims = 24;       ///< 6.5's vector
    static constexpr int kNumPercentiles = 21; ///< p0, p5 ... p100
    static const char* const kMagic;          ///< "luthier.calibration"

    enum Group { guitar = 0, bass, numGroups };

    DescriptorCalibration();

    /** Builds a calibration from analysed factory presets. */
    static DescriptorCalibration fromCorpus (const std::vector<std::pair<PresetFeatures, ToneFeatures>>& corpus);

    juce::var toVar() const;
    static DescriptorCalibration fromVar (const juce::var&);
    bool isValid() const noexcept { return valid; }

    /** The value of spectral feature `feature` at percentile `p` (0-100). */
    double percentile (Group, int feature, double p) const noexcept;

    /** 6.5: the raw 24-dimension vector of a preset, and its z-scored form. */
    static std::array<double, kNumDims> rawVector (const PresetFeatures&, const ToneFeatures&);
    std::array<double, kNumDims> zVector (const PresetFeatures&, const ToneFeatures&) const;

    /** 6.5's weights: spectral 1.0, drive 1.5, family 2.0, the rest 0.7. */
    static double weight (int dim) noexcept;

    /** The shipped file beside the factory bank, when there is one. */
    static juce::File getShippedFile();

    /** Neutral values for a first run with no shipped file and nothing analysed. */
    static DescriptorCalibration fallback();

private:
    std::array<std::array<std::array<double, kNumPercentiles>, ToneFeatures::kNumSpectral>, numGroups> table {};
    std::array<double, kNumDims> mean {}, spread {};
    bool valid = false;
};

//==============================================================================
struct Descriptor
{
    juce::String word;
    double confidence = 0.0;
};

class ToneDescriptors
{
public:
    static constexpr double kAttach = 0.5;

    /** The fixed vocabulary, in 6.3's order. */
    static const juce::StringArray& vocabulary();

    /** The words a search for `descriptor` also accepts (6.3's parentheses). */
    static juce::StringArray synonymsOf (const juce::String& descriptor);

    /** The descriptor a word means, or empty ("mellow" -> "warm"). */
    static juce::String canonicalFor (const juce::String& word);

    /** Every descriptor with its confidence; attached are those >= 0.5. */
    static std::vector<Descriptor> evaluate (const PresetFeatures&, const ToneFeatures&,
                                             const DescriptorCalibration&, const juce::StringArray& genres);

    static juce::StringArray attached (const std::vector<Descriptor>&);

    //==========================================================================
    /** factory-content.md 0.4's genre list. */
    static const juce::StringArray& genreList();

    /** Genres from a preset's category, tags and name. */
    static juce::StringArray genresFor (const juce::String& name, const juce::String& category,
                                        const juce::StringArray& tags);
};

} // namespace luthier
