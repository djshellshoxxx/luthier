#include "OpenFile.h"

namespace luthier
{

OpenFileKind classifyOpenFile (const juce::File& file)
{
    const auto extension = file.getFileExtension().toLowerCase();

    if (extension == ".luthierpreset")                  return OpenFileKind::preset;
    if (extension == ".luthierguitar")                  return OpenFileKind::guitar;
    if (extension == ".luthiertune")                    return OpenFileKind::tune;
    if (extension == ".mid" || extension == ".midi")    return OpenFileKind::midi;
    return OpenFileKind::unknown;
}

juce::Array<juce::File> filesFromCommandLine (const juce::String& commandLine,
                                              const juce::File& workingDirectory)
{
    // addTokens with quote characters keeps a quoted path with spaces whole.
    juce::StringArray tokens;
    tokens.addTokens (commandLine, " \t\r\n", "\"");
    tokens.removeEmptyStrings (true);

    juce::Array<juce::File> files;

    for (auto token : tokens)
    {
        token = token.trim().unquoted().trim();

        if (token.isEmpty() || token.startsWithChar ('-'))
            continue;

        // A file:// URI, which some Linux file managers pass for %u.
        if (token.startsWithIgnoreCase ("file://"))
            token = juce::URL::removeEscapeChars (token.substring (7));

        const auto file = juce::File::isAbsolutePath (token) ? juce::File (token)
                                                             : workingDirectory.getChildFile (token);

        // Every file Luthier opens has an extension; a bare word is an option's value.
        if (file.getFileExtension().isNotEmpty())
            files.addIfNotAlreadyThere (file);
    }

    return files;
}

juce::String commandLineForFiles (const juce::Array<juce::File>& files)
{
    juce::StringArray quoted;

    for (const auto& file : files)
        quoted.add (file.getFullPathName().quoted());

    return quoted.joinIntoString (" ");
}

} // namespace luthier
