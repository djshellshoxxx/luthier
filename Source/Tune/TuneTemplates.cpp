#include "TuneTemplates.h"
#include "../Support/IrLibrary.h"

namespace luthier
{

juce::File TuneTemplateLibrary::getFactoryDirectory()
{
    const auto resources = IrLibrary::getResourcesFolder();

    if (resources == juce::File())
        return {};

    return resources.getChildFile ("Tunes").getChildFile ("Templates");
}

std::vector<TuneTemplate> TuneTemplateLibrary::loadDirectory (const juce::File& directory, juce::StringArray* errors)
{
    std::vector<TuneTemplate> templates;

    if (directory == juce::File() || ! directory.isDirectory())
    {
        if (errors != nullptr)
            errors->add ("No tune template folder at " + directory.getFullPathName());

        return templates;
    }

    auto files = directory.findChildFiles (juce::File::findFiles, false,
                                           juce::String ("*") + TuneFile::kFileExtension);
    files.sort();

    for (const auto& file : files)
    {
        TuneTemplate t;
        const auto result = TuneFile::load (file, t.tune);

        if (! result.ok())
        {
            if (errors != nullptr)
                errors->add (juce::String (getTuneLoadErrorName (result.error)) + " " + result.message);

            continue;
        }

        t.name = t.tune.meta.title;
        t.file = file;
        templates.push_back (std::move (t));
    }

    return templates;
}

std::vector<TuneTemplate> TuneTemplateLibrary::loadFactory (juce::StringArray* errors)
{
    return loadDirectory (getFactoryDirectory(), errors);
}

Tune TuneTemplateLibrary::createBlank()
{
    // Must match Resources/Tunes/Templates/01-blank.luthiertune field for field.
    Tune tune;
    tune.meta.title = "Blank";
    tune.meta.author = "Factory";
    tune.meta.tempoBpm = 120.0;
    tune.meta.timeSigNumerator = 4;
    tune.meta.timeSigDenominator = 4;
    tune.meta.keyTonic = 0;
    tune.meta.mode = TuneMode::ionian;
    tune.meta.tags = juce::StringArray { "template" };
    tune.meta.notes = "No sections, no chords.";
    tune.meta.created = "2026-09-23T00:00:00Z";
    tune.meta.modified = "2026-09-23T00:00:00Z";
    return tune;
}

Tune TuneTemplateLibrary::instantiate (const Tune& templateTune)
{
    auto tune = templateTune;

    tune.meta.title = "Untitled Tune";
    tune.meta.author.clear();
    tune.meta.tags.removeString ("template");

    const auto now = juce::Time::getCurrentTime().toISO8601 (true);
    tune.meta.created = now;
    tune.meta.modified = now;

    return tune;
}

} // namespace luthier
