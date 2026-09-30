#include "FactoryPreviews.h"
#include "../FactoryPresets.h"
#include "../../PluginProcessor.h"

namespace luthier
{

namespace
{
    juce::StringArray tagsOf (const juce::var& json)
    {
        juce::StringArray tags;

        if (auto* a = json.getProperty ("tags", {}).getArray())
            for (const auto& t : *a)
                tags.add (t.toString());

        return tags;
    }
}

juce::String FactoryPreviews::fileNameFor (const juce::String& uid)
{
    return juce::File::createLegalFileName (uid.replaceCharacter (':', '_').replaceCharacter (' ', '_')) + ".ogg";
}

std::vector<FactoryPreviews::Rendered> FactoryPreviews::renderBank (PreviewRenderer& renderer,
                                                                    std::function<void (int, int, const juce::String&)> progress)
{
    std::vector<Rendered> out;
    auto& instance = renderer.getRangeSource();
    const int total = FactoryPresets::getNumPresets();

    for (int i = 0; i < total; ++i)
    {
        const auto& def = FactoryPresets::getPreset (i);
        Rendered r;
        r.name = def.name;
        r.uid = "factory:" + r.name;
        r.fileName = fileNameFor (r.uid);

        // The definition as FactoryPresets writes it to disk, so the manifest's
        // hash is the hash of FactoryPresets::toVar (PB-02).
        r.json = FactoryPresets::toVar (def, instance);

        if (progress)
            progress (i, total, r.name);

        r.result = renderer.render (r.json);
        out.push_back (std::move (r));
    }

    return out;
}

DescriptorCalibration FactoryPreviews::calibrationFor (const std::vector<Rendered>& bank)
{
    std::vector<std::pair<PresetFeatures, ToneFeatures>> corpus;

    for (const auto& r : bank)
        if (r.result.ok)
            corpus.push_back ({ r.result.params, r.result.features });

    return DescriptorCalibration::fromCorpus (corpus);
}

bool FactoryPreviews::write (const std::vector<Rendered>& bank, const juce::File& outDir, juce::String& error)
{
    const auto previews = outDir.getChildFile ("Previews");

    if (! previews.createDirectory())
    {
        error = "could not create " + previews.getFullPathName();
        return false;
    }

    const auto calibration = calibrationFor (bank);
    juce::Array<juce::var> entries;

    for (const auto& r : bank)
    {
        if (! r.result.ok)
        {
            error = r.name + ": " + r.result.error;
            return false;
        }

        if (! previews.getChildFile (r.fileName).replaceWithData (r.result.ogg.getData(), r.result.ogg.getSize()))
        {
            error = "could not write " + r.fileName;
            return false;
        }

        auto* e = new juce::DynamicObject();
        e->setProperty ("uid", r.uid);
        e->setProperty ("name", r.name);
        e->setProperty ("file", r.fileName);
        e->setProperty ("soundHash", r.result.soundHash);
        e->setProperty ("phrase", PreviewPhrase::getIdString (r.result.phrase));
        e->setProperty ("pluginVersion", JucePlugin_VersionString);
        e->setProperty ("features", r.result.features.toVar());

        const auto descriptors = ToneDescriptors::attached (ToneDescriptors::evaluate (
            r.result.params, r.result.features, calibration,
            ToneDescriptors::genresFor (r.name, r.json.getProperty ("category", {}).toString(), tagsOf (r.json))));
        e->setProperty ("descriptors", descriptors.joinIntoString (","));

        juce::Array<juce::var> peaks;

        for (float p : r.result.peaks)
            peaks.add ((double) p);

        e->setProperty ("peaks", peaks);
        entries.add (juce::var (e));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("schema", 1);
    root->setProperty ("magic", "luthier.previews");
    root->setProperty ("pluginVersion", JucePlugin_VersionString);
    root->setProperty ("renderRevision", PreviewRenderer::kRenderRevision);
    root->setProperty ("entries", entries);

    if (! previews.getChildFile ("previews.json").replaceWithText (juce::JSON::toString (juce::var (root), false)))
    {
        error = "could not write previews.json";
        return false;
    }

    if (! outDir.getChildFile ("descriptor-calibration.json").replaceWithText (juce::JSON::toString (calibration.toVar(), false)))
    {
        error = "could not write descriptor-calibration.json";
        return false;
    }

    return true;
}

} // namespace luthier
