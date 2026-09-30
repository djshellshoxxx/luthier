#include "BackingTrackLibrary.h"
#include "../Support/IrLibrary.h"

namespace luthier
{

juce::File BackingTrackLibrary::getFactoryDirectory()
{
    const auto resources = IrLibrary::getResourcesFolder();

    if (resources == juce::File())
        return {};

    return resources.getChildFile ("Practice").getChildFile ("BackingTracks");
}

juce::Array<juce::File> BackingTrackLibrary::findTracks (const juce::File& directory)
{
    juce::Array<juce::File> tracks;

    if (directory == juce::File() || ! directory.isDirectory())
        return tracks;

    for (const auto* pattern : { "*.flac", "*.wav", "*.ogg", "*.mp3" })
        tracks.addArray (directory.findChildFiles (juce::File::findFiles, false, pattern));

    tracks.sort();
    return tracks;
}

juce::Array<juce::File> BackingTrackLibrary::findFactoryTracks()
{
    return findTracks (getFactoryDirectory());
}

juce::String BackingTrackLibrary::getDisplayName (const juce::File& file)
{
    auto name = file.getFileNameWithoutExtension();

    // Drop the two-digit ordering prefix.
    if (name.length() > 3 && juce::CharacterFunctions::isDigit (name[0])
        && juce::CharacterFunctions::isDigit (name[1]) && name[2] == '-')
        name = name.substring (3);

    auto words = juce::StringArray::fromTokens (name.replaceCharacter ('-', ' '), " ", "");

    for (auto& word : words)
        word = word.substring (0, 1).toUpperCase() + word.substring (1);

    return words.joinIntoString (" ");
}

} // namespace luthier
