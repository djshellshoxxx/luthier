#pragma once

/*  preset-browser-previews.md 5.6: everything the browser knows about every
    preset, in memory, for search, filters and similarity.

    One per processor. Names appear at once (from PresetManager's scan); the
    parameter features and the sound hash are parsed from each file without
    loading it; the tone features and descriptors arrive as previews are
    rendered or read from the cache. Message thread only: the parsing runs on
    the preview worker (PreviewRenderService) and results are applied here.
*/

#include "PresetFeatures.h"
#include "ToneDescriptors.h"
#include "../PresetManager.h"
#include "../Preview/PreviewPhrase.h"
#include "../Preview/PreviewRenderer.h"

namespace luthier
{

class PresetIndex : public juce::ChangeBroadcaster
{
public:
    enum class Source { factory, user, pack, extra };
    enum class PreviewState { none, queued, rendering, ready, stale, failed };

    struct Entry
    {
        PresetInfo info;
        juce::String key;                 ///< uid, or the path relative to its folder (5.5)
        Source source = Source::user;
        bool locked = false;              ///< a Pro preset in the Free build (7.3)

        // Parsed from the file (6.1).
        bool parsed = false;
        bool corrupt = false;
        PresetFeatures params;
        juce::String soundHash;
        PreviewPhrase::Id phrase = PreviewPhrase::Id::clean_arp;
        juce::StringArray genres;
        juce::String previewPhraseField;

        // From the preview (6.2, 6.3).
        ToneFeatures tone;
        std::vector<Descriptor> descriptorConfidences;
        juce::StringArray descriptors;    ///< attached (>= 0.5)
        std::array<double, DescriptorCalibration::kNumDims> vector {};

        PreviewState preview = PreviewState::none;
        juce::String previewError;
        bool approximate = false;
        int failures = 0;
        std::array<float, PreviewResult::kNumPeaks> peaks {};

        // Lower-cased words per field, for the search (6.4).
        juce::StringArray nameWords, tagWords, categoryWords, gearWords, authorWords, descriptionWords;

        bool isAnalysed() const noexcept { return tone.valid; }
    };

    PresetIndex() = default;

    //==========================================================================
    /** Takes the preset list from the manager: new entries appear by name at
        once, removed ones go, unchanged ones keep what they already know. */
    void syncWithManager (const PresetManager& manager);

    /** Parses one file's features and hash (worker thread safe: pure). */
    struct Parsed
    {
        juce::File file;
        juce::Time modified;
        bool ok = false;
        PresetFeatures params;
        juce::String soundHash, uid, previewPhraseField;
        PreviewPhrase::Id phrase = PreviewPhrase::Id::clean_arp;
    };

    static Parsed parseFile (const juce::File& file, const PresetFeatureReader& reader);

    /** Applies a parsed result (message thread). */
    void applyParsed (const Parsed&);

    /** Parses every unparsed entry here and now (tests, the CLI, small banks). */
    void parseAllSynchronously (const PresetFeatureReader& reader);

    /** Applies analysed audio: tone features, peaks, state. */
    void applyAnalysis (const juce::String& soundHash, const ToneFeatures& tone,
                        const std::array<float, PreviewResult::kNumPeaks>& peaks,
                        bool approximate, bool stale);
    void setPreviewState (const juce::String& soundHash, PreviewState, const juce::String& error = {});

    /** Recomputes every descriptor and vector with a new calibration. */
    void setCalibration (const DescriptorCalibration&);
    const DescriptorCalibration& getCalibration() const noexcept { return calibration; }

    /** 2 / 6.3: the factory features calibrate the tagger. Once every factory
        entry is analysed the calibration is rebuilt from them. */
    bool recalibrateFromFactory();

    //==========================================================================
    int size() const noexcept { return (int) entries.size(); }
    const Entry& operator[] (int i) const { return entries[(size_t) i]; }
    const std::vector<Entry>& getEntries() const noexcept { return entries; }

    int indexOfKey (const juce::String& key) const;
    int indexOfName (const juce::String& name) const;
    int indexOfFile (const juce::File& file) const;
    juce::Array<int> indicesOfHash (const juce::String& soundHash) const;

    /** How many entries have no tone features yet (6.5's footer). */
    int numUnanalysed() const;

    /** Tests: add synthetic entries (PB-27's 5,000). */
    void addEntryForTesting (Entry e);
    void clear() { entries.clear(); }

    static juce::StringArray wordsOf (const juce::String& text);

    /** The key for a preset (5.5). */
    static juce::String keyFor (const PresetInfo& info, const juce::Array<juce::File>& searchFolders);

private:
    void refreshWords (Entry&);
    void refreshDescriptors (Entry&);

    std::vector<Entry> entries;
    DescriptorCalibration calibration { DescriptorCalibration::fallback() };
    bool calibratedFromFactory = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetIndex)
};

} // namespace luthier
