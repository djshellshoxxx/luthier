#include "PresetIndex.h"

namespace luthier
{

juce::StringArray PresetIndex::wordsOf (const juce::String& text)
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

juce::String PresetIndex::keyFor (const PresetInfo& info, const juce::Array<juce::File>& searchFolders)
{
    if (info.uid.isNotEmpty())
        return info.uid;

    // No uid (a read-only folder): the path relative to the folder it was found in.
    for (const auto& folder : searchFolders)
        if (info.file.isAChildOf (folder))
            return info.file.getRelativePathFrom (folder).replaceCharacter ('\\', '/');

    return info.file.getFullPathName();
}

//==============================================================================
void PresetIndex::syncWithManager (const PresetManager& manager)
{
    std::vector<Entry> next;
    next.reserve ((size_t) manager.getNumPresets());

    const auto folders = manager.getSearchFolders();

    for (int i = 0; i < manager.getNumPresets(); ++i)
    {
        const auto* info = manager.getPreset (i);

        if (info == nullptr)
            continue;

        const int existing = indexOfFile (info->file);

        Entry e;

        if (existing >= 0 && entries[(size_t) existing].info.modified == info->modified)
            e = entries[(size_t) existing];   // unchanged: keep what is known

        e.info = *info;
        e.key = keyFor (*info, folders);
        e.source = info->isFactory ? Source::factory
                 : (info->file.isAChildOf (PresetManager::getUserPresetFolder()) ? Source::user : Source::extra);

        if (info->file.getFullPathName().containsIgnoreCase ("ContentPacks"))
            e.source = Source::pack;

        e.genres = ToneDescriptors::genresFor (info->name, info->category, info->tags);
        refreshWords (e);
        next.push_back (std::move (e));
    }

    entries = std::move (next);
    sendChangeMessage();
}

PresetIndex::Parsed PresetIndex::parseFile (const juce::File& file, const PresetFeatureReader& reader)
{
    Parsed p;
    p.file = file;
    p.modified = file.getLastModificationTime();

    const auto json = juce::JSON::parse (file.loadFileAsString());
    auto* object = json.getDynamicObject();

    // 15: a corrupt or refused file is still listed by name, without a preview.
    if (object == nullptr || object->getProperty ("parameters").getDynamicObject() == nullptr)
        return p;

    p.params = reader.read (json);
    p.phrase = PreviewRenderer::choosePhrase (json, p.params);
    p.soundHash = PreviewRenderer::computeSoundHash (json, p.phrase);
    p.uid = object->getProperty ("uid").toString();
    p.previewPhraseField = object->getProperty ("previewPhrase").toString();
    p.ok = true;
    return p;
}

void PresetIndex::applyParsed (const Parsed& p)
{
    const int i = indexOfFile (p.file);

    if (i < 0)
        return;

    auto& e = entries[(size_t) i];
    e.parsed = true;
    e.corrupt = ! p.ok;

    if (! p.ok)
    {
        e.preview = PreviewState::failed;
        e.previewError = "the preset file is not readable";
        sendChangeMessage();
        return;
    }

    if (e.soundHash != p.soundHash)
    {
        // A different sound: what was known about the old one no longer applies.
        e.tone = {};
        e.preview = PreviewState::none;
        e.failures = 0;
        e.peaks = {};
    }

    e.params = p.params;
    e.soundHash = p.soundHash;
    e.phrase = p.phrase;
    e.previewPhraseField = p.previewPhraseField;
    e.info.guitarName = p.params.guitarName;
    e.info.ampName = p.params.ampName;
    e.info.family = PresetFeatures::getFamilyName (p.params.family);

    refreshWords (e);
    refreshDescriptors (e);
    sendChangeMessage();
}

void PresetIndex::parseAllSynchronously (const PresetFeatureReader& reader)
{
    for (size_t i = 0; i < entries.size(); ++i)
        if (! entries[i].parsed)
            applyParsed (parseFile (entries[i].info.file, reader));
}

void PresetIndex::applyAnalysis (const juce::String& soundHash, const ToneFeatures& tone,
                                 const std::array<float, PreviewResult::kNumPeaks>& peaks,
                                 bool approximate, bool stale)
{
    bool any = false;

    for (auto& e : entries)
    {
        if (e.soundHash != soundHash || soundHash.isEmpty())
            continue;

        e.tone = tone;
        e.peaks = peaks;
        e.approximate = approximate;
        e.preview = stale ? PreviewState::stale : PreviewState::ready;
        e.previewError.clear();
        refreshDescriptors (e);
        any = true;
    }

    if (any)
    {
        recalibrateFromFactory();
        sendChangeMessage();
    }
}

void PresetIndex::setPreviewState (const juce::String& soundHash, PreviewState state, const juce::String& error)
{
    for (auto& e : entries)
        if (e.soundHash == soundHash && soundHash.isNotEmpty())
        {
            // A ready clip stays ready while a stale-replacement renders.
            if (state == PreviewState::rendering && e.preview == PreviewState::ready)
                continue;

            e.preview = state;
            e.previewError = error;

            if (state == PreviewState::failed)
                ++e.failures;
        }

    sendChangeMessage();
}

void PresetIndex::setCalibration (const DescriptorCalibration& c)
{
    calibration = c;

    for (auto& e : entries)
        refreshDescriptors (e);

    sendChangeMessage();
}

bool PresetIndex::recalibrateFromFactory()
{
    if (calibratedFromFactory)
        return false;

    std::vector<std::pair<PresetFeatures, ToneFeatures>> corpus;
    int factory = 0;

    for (const auto& e : entries)
    {
        if (e.source != Source::factory)
            continue;

        ++factory;

        if (! e.isAnalysed())
            return false;

        corpus.push_back ({ e.params, e.tone });
    }

    if (factory == 0)
        return false;

    calibratedFromFactory = true;
    setCalibration (DescriptorCalibration::fromCorpus (corpus));
    return true;
}

void PresetIndex::refreshWords (Entry& e)
{
    e.nameWords = wordsOf (e.info.name);
    e.tagWords = wordsOf (e.info.tags.joinIntoString (" "));
    e.categoryWords = wordsOf (e.info.category + " " + e.genres.joinIntoString (" "));
    e.gearWords = wordsOf (e.info.guitarName + " " + e.info.ampName);
    e.authorWords = wordsOf (e.info.author);
    e.descriptionWords = wordsOf (e.info.description);
}

void PresetIndex::refreshDescriptors (Entry& e)
{
    if (! e.parsed || e.corrupt)
        return;

    e.descriptorConfidences = ToneDescriptors::evaluate (e.params, e.tone, calibration, e.genres);
    e.descriptors = ToneDescriptors::attached (e.descriptorConfidences);
    e.vector = calibration.zVector (e.params, e.tone);
}

//==============================================================================
int PresetIndex::indexOfKey (const juce::String& key) const
{
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].key == key)
            return (int) i;

    return -1;
}

int PresetIndex::indexOfName (const juce::String& name) const
{
    int user = -1;

    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].info.name.equalsIgnoreCase (name))
        {
            if (entries[i].source == Source::factory)
                return (int) i;

            if (user < 0)
                user = (int) i;
        }

    return user;
}

int PresetIndex::indexOfFile (const juce::File& file) const
{
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].info.file == file)
            return (int) i;

    return -1;
}

juce::Array<int> PresetIndex::indicesOfHash (const juce::String& hash) const
{
    juce::Array<int> out;

    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].soundHash == hash && hash.isNotEmpty())
            out.add ((int) i);

    return out;
}

int PresetIndex::numUnanalysed() const
{
    int n = 0;

    for (const auto& e : entries)
        if (! e.isAnalysed() && ! e.corrupt)
            ++n;

    return n;
}

void PresetIndex::addEntryForTesting (Entry e)
{
    refreshWords (e);
    refreshDescriptors (e);
    entries.push_back (std::move (e));
}

} // namespace luthier
