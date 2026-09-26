#include "FileOpenRouter.h"

#include "../PluginProcessor.h"
#include "../Export/MidiProfiles.h"

namespace luthier::FileOpenRouter
{

Target route (const juce::String& fileNameOrPath) noexcept
{
    const auto ext = fileNameOrPath.fromLastOccurrenceOf (".", true, false).toLowerCase();

    if (ext == ".luthierpreset")  return Target::preset;
    if (ext == ".luthierguitar")  return Target::guitar;
    if (ext == ".luthiertune")    return Target::tune;
    if (ext == ".luthierloop")    return Target::loop;
    if (ext == ".luthierset")     return Target::setlist;
    if (ext == ".midprofile")     return Target::midiProfile;

    return Target::unknown;
}

const char* describe (Target target) noexcept
{
    switch (target)
    {
        case Target::preset:      return "preset";
        case Target::guitar:      return "guitar";
        case Target::tune:        return "tune";
        case Target::loop:        return "loop";
        case Target::setlist:     return "setlist";
        case Target::midiProfile: return "MIDI export profile";
        case Target::unknown:
        default:                  return "unknown";
    }
}

juce::File fileFromCommandLine (const juce::String& commandLine)
{
    juce::StringArray args;
    args.addTokens (commandLine, true);

    for (auto arg : args)
    {
        arg = arg.trim().unquoted();

        if (arg.isEmpty() || arg.startsWithChar ('-'))
            continue;

        if (arg.startsWith ("file://"))
            arg = juce::URL (arg).getLocalFile().getFullPathName();

        if (juce::File::isAbsolutePath (arg))
            return juce::File (arg);

        return juce::File::getCurrentWorkingDirectory().getChildFile (arg);
    }

    return {};
}

bool open (LuthierAudioProcessor& processor, const juce::File& file, juce::String& error)
{
    const auto target = route (file.getFileName());

    if (target == Target::unknown)
    {
        error = file.getFileName() + " is not a Luthier file.";
        return false;
    }

    if (! file.existsAsFile())
    {
        error = file.getFullPathName() + " does not exist.";
        return false;
    }

    switch (target)
    {
        case Target::preset:
            if (processor.getPresetManager().loadPreset (file))
                return true;

            error = processor.getPresetManager().getLastLoadError();
            return false;

        case Target::guitar:
        {
            WorkshopGuitar guitar;
            PartLibrary::LoadReport report;

            if (! processor.getPartLibrary().loadGuitar (file, guitar, report))
            {
                error = file.getFileName() + " could not be read as a guitar.";
                return false;
            }

            processor.applyEditedGuitar (guitar);
            return true;
        }

        case Target::tune:
            return processor.getTuneSession().load (file, error);

        case Target::loop:
            if (processor.getLooper().load (file))
                return true;

            error = file.getFileName() + " could not be read as a loop.";
            return false;

        case Target::setlist:
            if (processor.loadSetlist (file))
                return true;

            error = file.getFileName() + " could not be read as a setlist.";
            return false;

        case Target::midiProfile:
        {
            MidiExportOptions options;
            juce::String name;

            if (MidiProfiles::loadProfile (file, options, &name, &error))
                return true;

            if (error.isEmpty())
                error = file.getFileName() + " could not be read as a MIDI export profile.";

            return false;
        }

        case Target::unknown:
        default:
            break;
    }

    return false;
}

} // namespace luthier::FileOpenRouter
